#include <session.h>
#include <server.h>

#include <database.h>
#include <pqxx/pqxx>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdio>

using nlohmann::json;

auto Session::handle_conv_list_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            // 预留将来扩展过滤参数，目前仅校验 JSON 格式。
            auto _ = json::parse(payload);
            (void)_;
        }

        auto const conversations = database::load_user_conversations(user_id_);

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
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_profile_update(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = json::parse(payload);

        if(!j.contains("displayName")) {
            return make_error_payload("INVALID_PARAM", "缺少 displayName 字段");
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
            return make_error_payload("INVALID_PARAM", "昵称不能为空");
        }
        if(new_name.size() > 64) {
            return make_error_payload("INVALID_PARAM", "昵称长度过长");
        }

        auto const result = database::update_display_name(user_id_, new_name);
        if(!result.ok) {
            return make_error_payload(result.error_code, result.error_msg);
        }

        // 更新当前会话缓存的昵称。
        display_name_ = result.user.display_name;

        json resp;
        resp["ok"] = true;
        resp["displayName"] = display_name_;
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_create_group_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("memberUserIds") || !j.at("memberUserIds").is_array()) {
            return make_error_payload("INVALID_PARAM", "缺少 memberUserIds 数组");
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
                return make_error_payload("INVALID_PARAM", "memberUserIds 中存在非法 ID");
            }
            if(id > 0) {
                members.push_back(id);
            }
        }

        // 至少要两位好友，加上创建者总计>=3
        if(members.size() < 2) {
            return make_error_payload("INVALID_PARAM", "群成员不足");
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

        auto const conv_id = database::create_group_conversation(user_id_, members, name);

        // 清除新会话的缓存(虽然是新创建,但确保一致性)
        if(server_ != nullptr) {
            server_->invalidate_conversation_cache(conv_id);
        }

        // 取实际群名
        std::string conv_name{ "群聊" };
        {
            auto conn = database::make_connection();
            pqxx::work tx{ conn };
            auto const q =
                "SELECT name FROM conversations WHERE id = " + tx.quote(conv_id) + " LIMIT 1";
            auto rows = tx.exec(q);
            if(!rows.empty()) {
                conv_name = rows[0][0].as<std::string>();
            }
            tx.commit();
        }

        // 写首条系统消息（使用创建者作为 sender，避免 sender_id=0 外键问题）
        auto const sys_content = std::string{ "你们创建了群聊：" } + conv_name;
        auto const stored = database::append_text_message(conv_id, user_id_, sys_content);

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["conversationType"] = "GROUP";
        resp["title"] = conv_name;
        resp["memberCount"] = static_cast<i64>(members.size() + 1);

        // 推送会话列表 & 系统消息给全体成员
        if(server_ != nullptr) {
            // 推送 creator + members
            server_->send_conv_list_to(user_id_);
            for(auto const uid : members) {
                server_->send_conv_list_to(uid);
            }
            server_->broadcast_system_message(conv_id, stored, sys_content);
        }

        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_open_single_conv_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("peerUserId")) {
            return make_error_payload("INVALID_PARAM", "缺少 peerUserId 字段");
        }

        auto const peer_str = j.at("peerUserId").get<std::string>();
        auto peer_id = i64{};
        try {
            peer_id = std::stoll(peer_str);
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id <= 0) {
            return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id == user_id_) {
            return make_error_payload("INVALID_PARAM", "不能与自己建立单聊");
        }

        if(!database::is_friend(user_id_, peer_id)) {
            return make_error_payload("NOT_FRIEND", "对方还不是你的好友");
        }

        auto const conv_id = database::get_or_create_single_conversation(user_id_, peer_id);

        // 清除单聊会话缓存(可能是新创建的)
        if(server_ != nullptr) {
            server_->invalidate_conversation_cache(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["conversationType"] = "SINGLE";
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_conv_members_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);
        if(!j.contains("conversationId")) {
            return make_error_payload("INVALID_PARAM", "缺少 conversationId 字段");
        }

        auto const conv_str = j.at("conversationId").get<std::string>();
        auto conv_id = i64{};
        try {
            conv_id = std::stoll(conv_str);
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }
        if(conv_id <= 0) {
            return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }

        auto const member = database::get_conversation_member(conv_id, user_id_);
        if(!member.has_value()) {
            return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }

        auto members = database::load_conversation_members(conv_id);

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        json arr = json::array();
        for(auto const& m : members) {
            json obj;
            obj["userId"] = std::to_string(m.user_id);
            obj["displayName"] = m.display_name;
            obj["role"] = m.role;
            obj["mutedUntilMs"] = m.muted_until_ms;
            arr.push_back(std::move(obj));
        }
        resp["members"] = std::move(arr);
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_mute_member_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);
        if(!j.contains("conversationId") || !j.contains("targetUserId") || !j.contains("durationSeconds")) {
            return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto conv_id = i64{};
        auto target_id = i64{};
        auto duration = i64{};
        try {
            conv_id = std::stoll(j.at("conversationId").get<std::string>());
            target_id = std::stoll(j.at("targetUserId").get<std::string>());
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "conversationId 或 targetUserId 非法");
        }
        duration = j.at("durationSeconds").get<i64>();

        if(conv_id <= 0 || target_id <= 0) {
            return make_error_payload("INVALID_PARAM", "参数非法");
        }
        if(duration <= 0) {
            return make_error_payload("INVALID_PARAM", "禁言时长必须大于 0");
        }

        auto const self_member = database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }
        if(self_member->role != "OWNER") {
            return make_error_payload("FORBIDDEN", "仅群主可禁言成员");
        }

        auto const target_member = database::get_conversation_member(conv_id, target_id);
        if(!target_member.has_value()) {
            return make_error_payload("NOT_FOUND", "目标成员不存在");
        }
        if(target_member->role == "OWNER") {
            return make_error_payload("FORBIDDEN", "不能禁言群主");
        }

        auto const now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()
                            )
                                .count();
        auto const muted_until_ms = now_ms + duration * 1000;

        database::set_member_mute_until(conv_id, target_id, muted_until_ms);

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
            database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(server_ != nullptr) {
            server_->broadcast_system_message(conv_id, stored, sys_content);
            server_->send_conv_members(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["targetUserId"] = std::to_string(target_id);
        resp["mutedUntilMs"] = muted_until_ms;
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_unmute_member_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("conversationId") || !j.contains("targetUserId")) {
            return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto conv_id = i64{};
        auto target_id = i64{};
        try {
            conv_id = std::stoll(j.at("conversationId").get<std::string>());
            target_id = std::stoll(j.at("targetUserId").get<std::string>());
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "conversationId 或 targetUserId 非法");
        }
        if(conv_id <= 0 || target_id <= 0) {
            return make_error_payload("INVALID_PARAM", "参数非法");
        }

        auto const self_member = database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }
        if(self_member->role != "OWNER") {
            return make_error_payload("FORBIDDEN", "仅群主可解除禁言");
        }

        auto const target_member = database::get_conversation_member(conv_id, target_id);
        if(!target_member.has_value()) {
            return make_error_payload("NOT_FOUND", "目标成员不存在");
        }

        database::set_member_mute_until(conv_id, target_id, 0);

        auto const target_name = target_member->display_name;
        auto const sys_content =
            "已解除 " + target_name + " 的禁言";
        auto const stored =
            database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(server_ != nullptr) {
            server_->broadcast_system_message(conv_id, stored, sys_content);
            server_->send_conv_members(conv_id);
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["targetUserId"] = std::to_string(target_id);
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_leave_conv_req(std::string const& payload) -> std::string
{
    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("conversationId")) {
            return make_error_payload("INVALID_PARAM", "缺少 conversationId 字段");
        }

        auto const conv_str = j.at("conversationId").get<std::string>();
        auto conv_id = i64{};
        try {
            conv_id = std::stoll(conv_str);
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }
        if(conv_id <= 0) {
            return make_error_payload("INVALID_PARAM", "conversationId 非法");
        }

        // 默认“世界”会话不允许退出 / 解散。
        try {
            auto const world_id = database::get_world_conversation_id();
            if(conv_id == world_id) {
                return make_error_payload("FORBIDDEN", "无法退出默认会话");
            }
        } catch(std::exception const&) {
            // 若世界会话缺失，忽略该保护。
        }

        // 校验会话存在且为群聊。
        std::string conv_type;
        {
            auto conn = database::make_connection();
            pqxx::work tx{ conn };

            auto const query =
                "SELECT type FROM conversations WHERE id = " + tx.quote(conv_id) + " LIMIT 1";

            auto rows = tx.exec(query);
            if(rows.empty()) {
                return make_error_payload("NOT_FOUND", "会话不存在");
            }

            conv_type = rows[0][0].as<std::string>();
            tx.commit();
        }

        if(conv_type != "GROUP") {
            return make_error_payload("INVALID_PARAM", "仅支持群聊会话");
        }

        auto const self_member = database::get_conversation_member(conv_id, user_id_);
        if(!self_member.has_value()) {
            return make_error_payload("FORBIDDEN", "你不是该会话成员");
        }

        auto members = database::load_conversation_members(conv_id);
        auto const member_count = static_cast<i64>(members.size());

        auto const is_owner = (self_member->role == "OWNER");
        auto const is_dissolve = is_owner || member_count <= 2;

        // 规范化昵称用于系统消息。
        auto const leaver_name = Session::normalize_whitespace(self_member->display_name);

        if(!is_dissolve) {
            // 普通成员退出群聊，群继续存在。
            auto const sys_content = leaver_name + " 退出了群聊";
            auto const stored =
                database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

            if(server_ != nullptr) {
                server_->broadcast_system_message(conv_id, stored, sys_content);
            }

            database::remove_conversation_member(conv_id, user_id_);

            if(server_ != nullptr) {
                // 退出者的会话列表需要刷新；群内其他成员刷新成员列表。
                server_->send_conv_list_to(user_id_);
                server_->send_conv_members(conv_id);
            }

            json resp;
            resp["ok"] = true;
            resp["conversationId"] = std::to_string(conv_id);
            resp["isDissolved"] = false;
            resp["memberCountBefore"] = member_count;
            return resp.dump();
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
            database::append_text_message(conv_id, user_id_, sys_content, "SYSTEM");

        if(server_ != nullptr) {
            server_->broadcast_system_message(conv_id, stored, sys_content);
        }

        database::dissolve_conversation(conv_id);

        // 清除会话缓存
        if(server_ != nullptr) {
            server_->invalidate_conversation_cache(conv_id);
        }

        if(server_ != nullptr) {
            for(auto const uid : member_ids) {
                server_->send_conv_list_to(uid);
            }
        }

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["isDissolved"] = true;
        resp["memberCountBefore"] = member_count;
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}
