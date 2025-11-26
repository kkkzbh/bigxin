#include <database/auth.h>
#include <database/connection.h>
#include <utility.h>

#include <boost/mysql.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <tuple>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace database
{
    auto register_user(std::string const& account, std::string const& password)
        -> asio::awaitable<RegisterResult>
    {
        RegisterResult res{};
        auto handle = co_await acquire_handle();
        auto& conn = *handle;
        mysql::results rows;

        try {
            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT id, display_name FROM users WHERE account = {} LIMIT 1",
                    account),
                rows,
                asio::use_awaitable
            );
            if(!rows.rows().empty()) {
                res.ok = false;
                res.error_code = "ACCOUNT_EXISTS";
                res.error_msg = "账号已存在";
                co_return res;
            }

            auto display_name = generate_random_display_name();
            co_await conn.async_execute(
                mysql::with_params(
                    "INSERT INTO users (account, password_hash, display_name) "
                    "VALUES ({}, {}, {})",
                    account,
                    password,
                    display_name),
                rows,
                asio::use_awaitable
            );
            auto const user_id = static_cast<i64>(rows.last_insert_id());

            co_await conn.async_execute(
                mysql::with_params(
                    "INSERT IGNORE INTO conversation_members (conversation_id, user_id, role)"
                    " VALUES ((SELECT id FROM conversations WHERE type='GROUP' AND name='世界'"
                    " LIMIT 1), {}, 'MEMBER')",
                    user_id),
                rows,
                asio::use_awaitable
            );

            res.ok = true;
            res.user.id = user_id;
            res.user.account = account;
            res.user.display_name = display_name;
            co_return res;
        } catch(std::exception const& ex) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
            co_return res;
        }
    }

    auto update_display_name(i64 user_id, std::string const& new_name)
        -> asio::awaitable<UpdateDisplayNameResult>
    {
        UpdateDisplayNameResult res{};
        if(user_id <= 0 || new_name.empty()) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效参数";
            co_return res;
        }

        auto handle = co_await acquire_handle();
        auto& conn = *handle;
        mysql::results rows;

        try {
            co_await conn.async_execute(
                mysql::with_params(
                    "UPDATE users SET display_name = {} WHERE id = {}",
                    new_name,
                    user_id),
                rows,
                asio::use_awaitable
            );
            if(rows.affected_rows() == 0) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "用户不存在";
                co_return res;
            }

            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT id, account, display_name FROM users WHERE id = {} LIMIT 1",
                    user_id),
                rows,
                asio::use_awaitable
            );
            if(rows.rows().empty()) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "用户不存在";
                co_return res;
            }

            auto r = rows.rows().front();
            res.ok = true;
            res.user.id = r.at(0).as_int64();
            res.user.account = r.at(1).as_string();
            res.user.display_name = r.at(2).as_string();
            co_return res;
        } catch(std::exception const& ex) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
            co_return res;
        }
    }

    auto login_user(std::string const& account, std::string const& password)
        -> asio::awaitable<LoginResult>
    {
        LoginResult res{};
        auto handle = co_await acquire_handle();
        auto& conn = *handle;
        mysql::results rows;

        try {
            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT id, password_hash, display_name FROM users WHERE account = {}"
                    " LIMIT 1",
                    account),
                rows,
                asio::use_awaitable
            );

            if(rows.rows().empty()) {
                res.ok = false;
                res.error_code = "LOGIN_FAILED";
                res.error_msg = "账号不存在或密码错误";
                co_return res;
            }

            auto r = rows.rows().front();
            auto stored_password = r.at(1).as_string();
            if(stored_password != password) {
                res.ok = false;
                res.error_code = "LOGIN_FAILED";
                res.error_msg = "账号不存在或密码错误";
                co_return res;
            }

            auto user_id = r.at(0).as_int64();
            co_await conn.async_execute(
                mysql::with_params(
                    "UPDATE users SET last_login_at = CURRENT_TIMESTAMP WHERE id = {}",
                    user_id),
                rows,
                asio::use_awaitable
            );

            res.ok = true;
            res.user.id = user_id;
            res.user.account = account;
            res.user.display_name = r.at(2).as_string();
            co_return res;
        } catch(std::exception const& ex) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
            co_return res;
        }
    }
} // namespace database
