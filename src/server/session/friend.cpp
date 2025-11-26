#include <session.h>
#include <server.h>

#include <database.h>

using nlohmann::json;

auto Session::handle_friend_list_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            auto _ = json::parse(payload);
            (void)_;
        }

        auto const friends = co_await database::load_user_friends(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& f : friends) {
            json u;
            u["userId"] = std::to_string(f.id);
            u["account"] = f.account;
            u["displayName"] = f.display_name;
            u["region"] = "";
            u["signature"] = "";
            items.push_back(std::move(u));
        }

        resp["friends"] = std::move(items);
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_friend_search_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("account")) {
            co_return make_error_payload("INVALID_PARAM", "缺少 account 字段");
        }

        auto const account = j.at("account").get<std::string>();

        auto const result = co_await database::search_friend_by_account(user_id_, account);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;

        json user;
        user["userId"] = std::to_string(result.user.id);
        user["account"] = result.user.account;
        user["displayName"] = result.user.display_name;
        user["region"] = "";
        user["signature"] = "";

        resp["user"] = std::move(user);
        resp["isFriend"] = result.is_friend;
        resp["isSelf"] = result.is_self;
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_friend_add_req(std::string const& payload) -> asio::awaitable<std::string>
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

        auto const source =
            j.contains("source") ? j.at("source").get<std::string>() : std::string{ "search_account" };
        auto const hello_msg =
            j.contains("helloMsg") ? j.at("helloMsg").get<std::string>() : std::string{};

        auto const result =
            co_await database::create_friend_request(user_id_, peer_id, source, hello_msg);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;
        resp["requestId"] = std::to_string(result.request_id);

        if(server_ != nullptr) {
            server_->send_friend_request_list_to(peer_id);
        }

        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_friend_req_list_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            auto _ = json::parse(payload);
            (void)_;
        }

        auto const requests = co_await database::load_incoming_friend_requests(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& r : requests) {
            json obj;
            obj["requestId"] = std::to_string(r.id);
            obj["fromUserId"] = std::to_string(r.from_user_id);
            obj["account"] = r.account;
            obj["displayName"] = r.display_name;
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

auto Session::handle_friend_accept_req(std::string const& payload) -> asio::awaitable<std::string>
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

        auto const result = co_await database::accept_friend_request(request_id, user_id_);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;

        json f;
        f["userId"] = std::to_string(result.friend_user.id);
        f["account"] = result.friend_user.account;
        f["displayName"] = result.friend_user.display_name;
        resp["friend"] = std::move(f);

        if(result.conversation_id > 0) {
            resp["conversationId"] = std::to_string(result.conversation_id);
            resp["conversationType"] = "SINGLE";
        } else {
            resp["conversationId"] = "";
        }

        if(server_ != nullptr) {
            server_->send_friend_list_to(user_id_);
            server_->send_friend_list_to(result.friend_user.id);
            server_->send_friend_request_list_to(user_id_);
            server_->send_friend_request_list_to(result.friend_user.id);
            if(result.conversation_id > 0) {
                server_->send_conv_list_to(user_id_);
                server_->send_conv_list_to(result.friend_user.id);
            }
        }

        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}
