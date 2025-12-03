#include <session.h>
#include <server.h>

#include <database.h>

using nlohmann::json;

auto Session::handle_group_search_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("groupId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 groupId 字段");
        }

        auto const group_id_str = j.at("groupId").get<std::string>();
        auto group_id = i64{};
        try {
            group_id = std::stoll(group_id_str);
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "groupId 格式错误");
        }
        if(group_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "groupId 非法");
        }

        auto const result = co_await database::search_group_by_id(user_id_, group_id);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;

        json group;
        group["groupId"] = std::to_string(result.group_id);
        group["name"] = result.name;
        group["memberCount"] = result.member_count;

        resp["group"] = std::move(group);
        resp["isMember"] = result.is_member;
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_group_join_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("groupId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 groupId 字段");
        }

        auto const group_id_str = j.at("groupId").get<std::string>();
        auto group_id = i64{};
        try {
            group_id = std::stoll(group_id_str);
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "groupId 格式错误");
        }
        if(group_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "groupId 非法");
        }

        auto const hello_msg =
            j.contains("helloMsg") ? j.at("helloMsg").get<std::string>() : std::string{};

        auto const result =
            co_await database::create_group_join_request(user_id_, group_id, hello_msg);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;
        resp["requestId"] = std::to_string(result.request_id);

        // 推送给群主和所有管理员
        if(auto server = server_.lock()) {
            auto admins = co_await database::get_group_admins(group_id);
            for(auto const admin_id : admins) {
                server->send_group_join_request_list_to(admin_id);
            }
        }

        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_group_join_req_list_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            auto _ = json::parse(payload);
            (void)_;
        }

        auto const requests = co_await database::load_group_join_requests_for_admin(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& r : requests) {
            json obj;
            obj["requestId"] = std::to_string(r.id);
            obj["fromUserId"] = std::to_string(r.from_user_id);
            obj["account"] = r.account;
            obj["displayName"] = r.display_name;
            obj["groupId"] = std::to_string(r.group_id);
            obj["groupName"] = r.group_name;
            obj["status"] = r.status;
            obj["helloMsg"] = r.hello_msg;
            items.push_back(std::move(obj));
        }

        resp["requests"] = std::move(items);
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_group_join_accept_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("requestId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 requestId 字段");
        }

        auto const id_str = j.at("requestId").get<std::string>();
        auto request_id = i64{};
        try {
            request_id = std::stoll(id_str);
        } catch(std::exception const&) {
            co_return make_error_payload("INVALID_PARAM", "requestId 非法");
        }
        if(request_id <= 0) {
            co_return make_error_payload("INVALID_PARAM", "requestId 非法");
        }

        // 默认同意，可通过 accept 字段控制
        auto const accept = j.contains("accept") ? j.at("accept").get<bool>() : true;

        auto const result = co_await database::handle_group_join_request(request_id, user_id_, accept);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;

        if(accept) {
            json member;
            member["userId"] = std::to_string(result.new_member.id);
            member["account"] = result.new_member.account;
            member["displayName"] = result.new_member.display_name;
            resp["newMember"] = std::move(member);
        }

        resp["groupId"] = std::to_string(result.group_id);
        resp["groupName"] = result.group_name;

        if(auto server = server_.lock()) {
            // 清除缓存
            server->invalidate_conversation_cache(result.group_id);
            server->invalidate_member_list_cache(result.group_id);

            // 推送给所有群主/管理员刷新申请列表
            auto admins = co_await database::get_group_admins(result.group_id);
            for(auto const admin_id : admins) {
                server->send_group_join_request_list_to(admin_id);
            }

            if(accept) {
                // 推送会话列表给新成员
                server->send_conv_list_to(result.new_member.id);
                // 推送成员列表给所有群成员
                server->send_conv_members(result.group_id);

                // 广播系统消息
                auto const sys_content = result.new_member.display_name + " 加入了群聊";
                auto const stored = co_await database::append_text_message(
                    result.group_id,
                    result.new_member.id,
                    sys_content,
                    "SYSTEM"
                );
                server->broadcast_system_message(result.group_id, stored, sys_content);
            }
        }

        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}
