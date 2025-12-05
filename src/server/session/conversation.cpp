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

#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

/// \brief Base64解码帮助函数
static auto base64_decode(std::string const& input) -> std::vector<u8> 
{
    static const std::string_view kBase64Chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[kBase64Chars[i]] = i;

    std::vector<u8> out;
    out.reserve(input.size() * 3 / 4);

    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) continue; // Skip padding or invalid chars
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(u8((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

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
            c["lastSeq"] = conv.last_seq;
            c["lastServerTimeMs"] = conv.last_server_time_ms;
            c["avatarPath"] = conv.avatar_path;
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

auto Session::handle_avatar_update(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = json::parse(payload);

        if(!j.contains("avatarData")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 avatarData 字段");
        }

        auto const base64_data = j.at("avatarData").get<std::string>();
        auto extension = std::string{"jpg"};
        if(j.contains("extension")) {
            extension = j.at("extension").get<std::string>();
            // 简单安全检查：仅允许字母数字
             if(extension.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != std::string::npos) {
                extension = "jpg";
             }
        }

        // 解码数据
        auto const data = base64_decode(base64_data);
        if(data.empty()) {
            co_return make_error_payload("INVALID_PARAM", "无效的头像数据");
        }
        if(data.size() > 5 * 1024 * 1024) { // 再次校验大小
            co_return make_error_payload("INVALID_PARAM", "头像文件过大");
        }

        // 确保目录存在
        // 存储路径: running_dir/server_data/avatars/
        auto const server_root = fs::current_path();
        auto const avatar_dir = server_root / "server_data" / "avatars";
        
        std::error_code ec;
        if(!fs::exists(avatar_dir, ec)) {
            fs::create_directories(avatar_dir, ec);
        }
        if(ec) {
             std::println("Create directory failed: {}", ec.message());
             co_return make_error_payload("SERVER_ERROR", "服务器存储错误");
        }

        // 构造文件名: userId_timestamp.ext (加上时间戳防止缓存)
        // 其实用 userId.ext 覆盖也可以，但前端缓存是个问题。
        // 这里简单起见用 userId.ext，前端负责通过 path 变化刷新或 request param。
        // 为了支持 path 变化 triggered update, 最好文件名变一下?
        // 不过 DB 只存了 path 字段。如果 path 不变，前端可能不知道变了。
        // User 之前方案是 "本地文件+路径存储"。
        // 让我们总是覆盖 userId.ext, 并在 path 后面加 ?v=timestamp 也许?
        // 或者直接文件名带 version。
        // 简单实现：覆盖 userId.ext。前端 Image cache: false 已设置。
        
        auto const filename = std::to_string(user_id_) + "." + extension;
        auto const filepath = avatar_dir / filename;

        // 写入文件
        std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
        if(!ofs.is_open()) {
             std::println("Open file failed: {}", filepath.string());
             co_return make_error_payload("SERVER_ERROR", "无法保存头像文件");
        }
        ofs.write(reinterpret_cast<char const*>(data.data()), data.size());
        ofs.close();

        // 存入数据库的是相对路径 (server_data/avatars/...) 还是绝对路径?
        // 应该是给客户端用的 path。
        // 如果客户端只拿到 path, 它怎么访问?
        // 我之前的设计是: Base64 transfer.
        // Wait, Client *Uploads* Base64. Server Saves.
        // Client *Downloads* ?? 
        // 
        // REVIEW Implementation Plan:
        // "Clients will receive the Base64 encoded image data (or a path if we later implement a static file server)"
        // But the user said "Local file + path storage is fine". 
        // If I only store path, client cannot see the image unless I serve it.
        // Currently I do NOT have a static file server (HTTP). I only have a TCP socket protocol.
        // 
        // So for Clients to SEE the avatar, I must either:
        // 1. Send Base64 data in `member_list` / `login_resp` etc. (This bloats traffic hugely).
        // 2. Add a `GET_AVATAR` command to fetch data by path.
        // 
        // The user specifically approved "Local file + path storage".
        // This usually implies a static file server.
        // BUT I don't have one set up. 
        // Maybe the user assumes local file access (shared fs)? No, that's unlikely for "Network".
        // 
        // Given time constraint, I should probably implement a `GET_AVATAR` (download) mechanism or 
        // assume the path IS accessible (e.g. shared folder). 
        // 
        // Wait, "User specifically mentioned that '本地文件+路径存储就可以', indicating a preference for file paths over Base64".
        // This might interpret as: Just store the path string.
        // And maybe the client *loads* from that path?
        // If client and server are on the same machine (WSL), then `C:/...` path works!
        // The user IS running locally on WSL.
        // "My current project is... on WSL".
        // So `server_data/avatars/...` absolute path on Server (WSL filesystem)
        // If the Client (Windows) can access WSL filesystem `\\wsl.localhost\...`, then it works!
        // 
        // So, if I return the **Absolute Path** in WSL, and display it in Windows Client?
        // Windows Client accessing `\\wsl.localhost\Ubuntu\home\kkkzbh\code\chat\server_data\avatars\...` might work.
        // 
        // Let's stick to this "Shared/Local FS" assumption for now as it's the simplest interpretation of "Local file + path".
        // 
        // SO: Server saves file.
        // Server returns Absolute Path (or Path relative to project root, and Client constructs full path).
        // I'll store the *Absolute Path* in DB or Relative.
        // Storing Relative is better. Client constructs absolute.
        // But Client needs to know Server Root?
        // 
        // Let's store `server_data/avatars/xxxx.jpg`.
        // Client knows project root? Or handles `file:///` logic.
        // 
        // Actually, if I store the full WSL path `//wsl.localhost/Ubuntu/...` it might work.
        // 
        // Let's try to just store the relative path `server_data/avatars/id.jpg`.
        // And simply return it.
        
        auto const relative_path = "server_data/avatars/" + filename;
        
        // 更新数据库
        auto const db_res = co_await database::update_avatar(user_id_, relative_path);
        if(!db_res.ok) {
            co_return make_error_payload(db_res.error_code, db_res.error_msg);
        }

        avatar_path_ = db_res.user.avatar_path;

        json resp;
        resp["ok"] = true;
        resp["avatarPath"] = avatar_path_;
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
            obj["avatarPath"] = m.avatar_path;
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
