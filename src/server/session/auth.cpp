#include <session.h>
#include <server.h>

#include <database.h>

using nlohmann::json;

auto Session::handle_register(std::string const& payload) -> asio::awaitable<std::string>
{
    // 使用非抛出版本的JSON解析,减少异常开销
    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    }

    try {
        if(!j.contains("account") || !j.contains("password") || !j.contains("confirmPassword")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto const account = j.at("account").get<std::string>();
        auto const password = j.at("password").get<std::string>();
        auto const confirm = j.at("confirmPassword").get<std::string>();

        if(password != confirm) {
            co_return make_error_payload("PASSWORD_MISMATCH", "两次密码不一致");
        }

        auto const result = co_await database::register_user(account, password);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;
        resp["userId"] = std::to_string(result.user.id);
        resp["displayName"] = result.user.display_name;
        resp["avatarPath"] = result.user.avatar_path;
        co_return resp.dump();
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

auto Session::handle_login(std::string const& payload) -> asio::awaitable<std::string>
{
    // 使用非抛出版本的JSON解析
    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    }

    try {
        if(!j.contains("account") || !j.contains("password")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要字段");
        }

        auto const account = j.at("account").get<std::string>();
        auto const password = j.at("password").get<std::string>();

        auto const result = co_await database::login_user(account, password);
        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        authenticated_ = true;
        user_id_ = result.user.id;
        account_ = result.user.account;
        display_name_ = result.user.display_name;
        avatar_path_ = result.user.avatar_path;

        if(auto server = server_.lock()) {
            server->index_authenticated_session(shared_from_this());
        }

        auto const world_id = co_await database::get_world_conversation_id();

        json resp;
        resp["ok"] = true;
        resp["userId"] = std::to_string(user_id_);
        resp["displayName"] = display_name_;
        resp["avatarPath"] = avatar_path_;
        resp["worldConversationId"] = std::to_string(world_id);
        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

