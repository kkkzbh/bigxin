#include <session.h>
#include <server.h>

#include <database.h>
#include <boost/mysql.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdio>

using nlohmann::json;
namespace mysql = boost::mysql;

auto Session::handle_conv_list_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            // 预留将来扩展过滤参数，目前仅校验 JSON 格式。
            auto _ = json::parse(payload);
            (void)_;
        }

        auto const conversations = co_await database::load_user_conversations(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& conv : conversations) {
            json c;
            c["conversationId"] = std::to_string(conv.id);
            c["conversationType"] = conv.type;
            c["title"] = conv.title;
            c["lastSeq"] = conv.last_seq;
            c["lastServerTimeMs"] = conv.last_server_time_ms;
            items.push_back(std::move(c));
        }

        resp["conversations"] = std::move(items);
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_profile_update(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = json::parse(payload);

        if(!j.contains("displayName")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 displayName 字段");
        }

        auto new_name = j.at("displayName").get<std::string>();

        // 去掉首尾空白并做简单长度校验。
        auto const trim = [](std::string& s) {
            auto const not_space = [](unsigned char ch) { return !std::isspace(ch); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        };
        trim(new_name);

        if(new_name.empty()) {
            co_return make_error_payload("INVALID_PARAM", "昵称不能为空");
        }
        if(new_name.size() > 64) {
            co_return make_error_payload("INVALID_PARAM", "昵称长度过长");
        }

        auto const result = co_await database::update_display_name(user_id_, new_name);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        // 更新当前会话缓存的昵称。
        display_name_ = result.user.display_name;

        json resp;
        resp["ok"] = true;
        resp["displayName"] = display_name_;
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_create_group_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("memberUserIds") || !j.at("memberUserIds").is_array()) {
            co_return make_error_payload("INVALID_PARAM", "缺少 memberUserIds 数组");
        }

        auto const arr = j.at("memberUserIds");
        std::vector<i64> members;
        members.reserve(arr.size());
        for(auto const& item : arr) {
            auto const str = item.get<std::string>();
            auto id = i64{};
            try {
                id = std::stoll(str);
            } catch(std::exception const&) {
                co_return make_error_payload("INVALID_PARAM", "memberUserIds 中存在非法 ID");
            }
            if(id > 0) {
                members.push_back(id);
            }
        }

        // 至少要两位好友，加上创建者总计>=3
        if(members.size() < 2) {
            co_return make_error_payload("INVALID_PARAM", "群成员不足");
        }

        auto name = std::string{};
        if(j.contains("name")) {
            name = j.at("name").get<std::string>();
            auto trim = [](std::string& s) {
                auto const not_space = [](unsigned char ch) { return !std::isspace(ch); };
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
                s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
            };
            trim(name);
        }

        auto const conv_id = co_await database::create_group_conversation(user_id_, members, name);

        // 清除新会话的缓存(虽然是新创建,但确保一致性)
        if(auto server = server_.lock()) {
            server->invalidate_conversation_cache(conv_id);
            server->invalidate_member_list_cache(conv_id);
        }

        // 取实际群名
        std::string conv_name = name;
        if(conv_name.empty()) {
            try {
                auto conn_h = co_await database::acquire_connection();
                boost::mysql::results r;
                co_await conn_h->async_execute(
                    mysql::with_params(
                        "SELECT name FROM conversations WHERE id = {} LIMIT 1",
                        conv_id),
                    r,
                    asio::use_awaitable
                );
                if(!r.rows().empty()) {
                    conv_name = r.rows().front().at(0).as_string();
                }
            } catch(...) {}
            if(conv_name.empty()) conv_name = "群聊";
        }

        // 写首条系统消息（使用创建者作为 sender，消息类型标记为 SYSTEM）
        auto const sys_content = std::string{ "你们创建了群聊：" } + conv_name;
        auto const stored =
            co_await database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["conversationType"] = "GROUP";
        resp["title"] = conv_name;
        resp["memberCount"] = static_cast<i64>(members.size() + 1);

        // 推送会话列表 & 系统消息给全体成员
        if(auto server = server_.lock()) {
            // 推送 creator + members
            server->send_conv_list_to(user_id_);
            for(auto const uid : members) {
                server->send_conv_list_to(uid);
            }
            server->broadcast_system_message(conv_id, stored, sys_content);
        }

        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_open_single_conv_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("peerUserId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 peerUserId 字段");
        }

        auto const peer_str = j.at("peerUserId").get<std::string>();
        auto peer_id = i64{};
        try {
            peer_id = std::stoll(peer_str);
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id == user_id_) {
            co_return make_error_payload("INVALID_PARAM", "不能与自己建立单聊");
        }

        if(!co_await database::is_friend(user_id_, peer_id)) {
            co_return make_error_payload("NOT_FRIEND", "对方还不是你的好友");
        }

        auto const conv_id = co_await database::get_or_create_single_conversation(user_id_, peer_id);

        // 清除单聊会话缓存(可能是新创建的)
        if(auto server = server_.lock()) {
            server->invalidate_conversation_cache(conv_id);
            server->invalidate_member_list_cache(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["conversationType"] = "SINGLE";
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_conv_members_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);
        if(!j.contains("conversationId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 conversationId 字段");
        }

        auto const conv_str = j.at("conversationId").get<std::string>();
        auto conv_id = i64{};
        try {
            conv_id = std::stoll(conv_str);
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }
        if(conv_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }

        auto const member = co_await database::get_conversation_member(conv_id, user_id_);
        if(!member.has_value()) {
            co_return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }

        // 分页参数：offset / limit，默认 0 / 50，最大 200
        auto offset = i64{ 0 };
        auto limit = i64{ 50 };
        if(j.contains("offset")) {
            offset = std::max<i64>(0, j.at("offset").get<i64>());
        }
        if(j.contains("limit")) {
            limit = std::clamp<i64>(j.at("limit").get<i64>(), 1, 200);
        }

        std::vector<database::MemberInfo> members;
        if(auto server = server_.lock()) {
            if(auto cached = server->get_member_list_cache(conv_id)) {
                members = cached->members;
            } else {
                members = co_await database::load_conversation_members(conv_id);
                server->set_member_list_cache(conv_id, members);
            }
        } else {
            members = co_await database::load_conversation_members(conv_id);
        }

        auto const total = static_cast<i64>(members.size());
        auto const begin = static_cast<size_t>(std::min(offset, total));
        auto const end = static_cast<size_t>(std::min<i64>(offset + limit, total));

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["total"] = total;
        resp["hasMore"] = (end < static_cast<size_t>(total));
        resp["nextOffset"] = static_cast<i64>(end);

        json arr = json::array();
        for(size_t idx = begin; idx < end; ++idx) {
            auto const& m = members[idx];
            json obj;
            obj["userId"] = std::to_string(m.user_id);
            obj["displayName"] = m.display_name;
            obj["role"] = m.role;
            obj["mutedUntilMs"] = m.muted_until_ms;
            arr.push_back(std::move(obj));
        }
        resp["members"] = std::move(arr);
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_mute_member_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);
        if(!j.contains("conversationId") || !j.contains("targetUserId") || !j.contains("durationSeconds")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto conv_id = i64{};
        auto target_id = i64{};
        auto duration = i64{};
        try {
            conv_id = std::stoll(j.at("conversationId").get<std::string>());
            target_id = std::stoll(j.at("targetUserId").get<std::string>());
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 或 targetUserId 非法");
        }
        duration = j.at("durationSeconds").get<i64>();

        if(conv_id <= 0 || target_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "参数非法");
        }
        if(duration <= 0) {
            co_return make_error_payload("INVALID_PARAM", "禁言时长必须大于 0");
        }

        auto const self_member = co_await database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            co_return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }
        // 群主和管理员都可以禁言
        if(self_member->role != "OWNER" && self_member->role != "ADMIN") {
            co_return make_error_payload("FORBIDDEN", "仅群主和管理员可禁言成员");
        }

        auto const target_member = co_await database::get_conversation_member(conv_id, target_id);
        if(!target_member.has_value()) {
            co_return make_error_payload("NOT_FOUND", "目标成员不存在");
        }
        if(target_member->role == "OWNER") {
            co_return make_error_payload("FORBIDDEN", "不能禁言群主");
        }
        // 管理员不能禁言其他管理员
        if(self_member->role == "ADMIN" && target_member->role == "ADMIN") {
            co_return make_error_payload("FORBIDDEN", "管理员不能禁言其他管理员");
        }

        auto const now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()
                            )
                                .count();
        auto const muted_until_ms = now_ms + duration * 1000;

        co_await database::set_member_mute_until(conv_id, target_id, muted_until_ms);

        if(auto server = server_.lock()) {
            server->invalidate_member_list_cache(conv_id);
        }

        // 系统消息
        auto const tp =
            std::chrono::system_clock::time_point{ std::chrono::milliseconds{ muted_until_ms } };
        std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm = *std::localtime(&tt);
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

        auto const target_name = target_member->display_name;
        auto const sys_content =
            "已将 " + target_name + " 禁言至 " + std::string{ buf };

        auto const stored =
            co_await database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(auto server = server_.lock()) {
            server->broadcast_system_message(conv_id, stored, sys_content);
            server->send_conv_members(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["targetUserId"] = std::to_string(target_id);
        resp["mutedUntilMs"] = muted_until_ms;
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_unmute_member_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("conversationId") || !j.contains("targetUserId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto conv_id = i64{};
        auto target_id = i64{};
        try {
            conv_id = std::stoll(j.at("conversationId").get<std::string>());
            target_id = std::stoll(j.at("targetUserId").get<std::string>());
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 或 targetUserId 非法");
        }
        if(conv_id <= 0 || target_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "参数非法");
        }

        auto const self_member = co_await database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            co_return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }
        // 群主和管理员都可以解禁
        if(self_member->role != "OWNER" && self_member->role != "ADMIN") {
            co_return make_error_payload("FORBIDDEN", "仅群主和管理员可解除禁言");
        }

        auto const target_member = co_await database::get_conversation_member(conv_id, target_id);
        if(!target_member.has_value()) {
            co_return make_error_payload("NOT_FOUND", "目标成员不存在");
        }
        // 管理员不能解禁其他管理员（虽然不能禁言也就不需要解禁，但为了逻辑完整性）
        if(self_member->role == "ADMIN" && target_member->role == "ADMIN") {
            co_return make_error_payload("FORBIDDEN", "管理员不能操作其他管理员");
        }

        co_await database::set_member_mute_until(conv_id, target_id, 0);

        if(auto server = server_.lock()) {
            server->invalidate_member_list_cache(conv_id);
        }

        auto const target_name = target_member->display_name;
        auto const sys_content =
            "已解除 " + target_name + " 的禁言";
        auto const stored =
            co_await database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(auto server = server_.lock()) {
            server->broadcast_system_message(conv_id, stored, sys_content);
            server->send_conv_members(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["targetUserId"] = std::to_string(target_id);
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_leave_conv_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("conversationId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 conversationId 字段");
        }

        auto const conv_str = j.at("conversationId").get<std::string>();
        auto conv_id = i64{};
        try {
            conv_id = std::stoll(conv_str);
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }
        if(conv_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }

        // 默认“世界”会话不允许退出 / 解散。
        try {
            auto const world_id = co_await database::get_world_conversation_id();
            if(conv_id == world_id) {
                co_return make_error_payload("FORBIDDEN", "无法退出默认会话");
            }
        } catch(std::exception const&) {
            // 若世界会话缺失，忽略该保护。
        }

        // 校验会话存在且为群聊。
        std::string conv_type;
        {
            auto conn_h = co_await database::acquire_connection();
            boost::mysql::results r;
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT type FROM conversations WHERE id = {} LIMIT 1",
                    conv_id),
                r,
                asio::use_awaitable
            );
            if(r.rows().empty()) {
                co_return make_error_payload("NOT_FOUND", "会话不存在");
            }
            conv_type = r.rows().front().at(0).as_string();
        }

        if(conv_type != "GROUP") {
            co_return make_error_payload("INVALID_PARAM", "仅支持群聊会话");
        }

        auto const self_member = co_await database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            co_return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }

        auto members = co_await database::load_conversation_members(conv_id);
        auto const member_count = static_cast<i64>(members.size());

        auto const is_owner = (self_member->role == "OWNER");
        auto const is_dissolve = is_owner || member_count <= 2;

        // 规范化昵称用于系统消息。
        auto const leaver_name = Session::normalize_whitespace(self_member->display_name);

        if(!is_dissolve) {
            // 普通成员退出群聊，群继续存在。
            auto const sys_content = leaver_name + " 退出了群聊";
            auto const stored =
                co_await database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

            if(auto server = server_.lock()) {
                server->broadcast_system_message(conv_id, stored, sys_content);
            }

            co_await database::remove_conversation_member(conv_id, user_id_);

            if(auto server = server_.lock()) {
                // 退出者的会话列表需要刷新；群内其他成员刷新成员列表。
                server->send_conv_list_to(user_id_);
                server->send_conv_members(conv_id);
            }

            json resp;
            resp["ok"] = true;
            resp["conversationId"] = std::to_string(conv_id);
            resp["isDissolved"] = false;
            resp["memberCountBefore"] = member_count;
            co_return resp.dump();
        }

        // 解散群聊：群主主动操作，或成员数不超过 2 时的任意成员操作。
        std::vector<i64> member_ids;
        member_ids.reserve(members.size());
        for(auto const& m : members) {
            member_ids.push_back(m.user_id);
        }

        std::string sys_content;
        if(is_owner) {
            sys_content = leaver_name + " 解散了群聊";
        } else {
            sys_content = leaver_name + " 退出群聊，群聊已解散";
        }

        auto const stored =
            co_await database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(auto server = server_.lock()) {
            server->broadcast_system_message(conv_id, stored, sys_content);
        }

        co_await database::dissolve_conversation(conv_id);

        // 清除会话缓存
        if(auto server = server_.lock()) {
            server->invalidate_conversation_cache(conv_id);
            server->invalidate_member_list_cache(conv_id);
            for(auto const uid : member_ids) {
                server->send_conv_list_to(uid);
            }
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["isDissolved"] = true;
        resp["memberCountBefore"] = member_count;
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_set_admin_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);
        if(!j.contains("conversationId") || !j.contains("targetUserId") || !j.contains("isAdmin")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto conv_id = i64{};
        auto target_id = i64{};
        auto is_admin = false;
        try {
            conv_id = std::stoll(j.at("conversationId").get<std::string>());
            target_id = std::stoll(j.at("targetUserId").get<std::string>());
            is_admin = j.at("isAdmin").get<bool>();
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "conversationId 或 targetUserId 非法");
        }
        if(conv_id <= 0 || target_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "参数非法");
        }

        auto const self_member = co_await database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            co_return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }
        if(self_member->role != "OWNER") {
            co_return make_error_payload("FORBIDDEN", "仅群主可设置管理员");
        }

        auto const target_member = co_await database::get_conversation_member(conv_id, target_id);
        if(!target_member.has_value()) {
            co_return make_error_payload("NOT_FOUND", "目标成员不存在");
        }
        if(target_member->role == "OWNER") {
            co_return make_error_payload("FORBIDDEN", "不能更改群主角色");
        }

        auto const new_role = is_admin ? "ADMIN" : "MEMBER";
        if(target_member->role == new_role) {
            // 角色未变，直接成功
            json resp;
            resp["ok"] = true;
            resp["conversationId"] = std::to_string(conv_id);
            resp["targetUserId"] = std::to_string(target_id);
            resp["isAdmin"] = is_admin;
            co_return resp.dump();
        }

        co_await database::set_member_role(conv_id, target_id, new_role);

        if(auto server = server_.lock()) {
            server->invalidate_member_list_cache(conv_id);
        }

        auto const target_name = target_member->display_name;
        auto const sys_content = is_admin
            ? ("已将 " + target_name + " 设为管理员")
            : ("已取消 " + target_name + " 的管理员身份");
        
        auto const stored =
            co_await database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(auto server = server_.lock()) {
            server->broadcast_system_message(conv_id, stored, sys_content);
            server->send_conv_members(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["targetUserId"] = std::to_string(target_id);
        resp["isAdmin"] = is_admin;
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}
