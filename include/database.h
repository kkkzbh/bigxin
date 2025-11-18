#pragma once

#include <pqxx/pqxx>

#include <mutex>
#include <random>
#include <string>
#include <chrono>

#include <utility.h>

/// \brief 与数据库相关的简单工具封装。
/// \details 仅负责用户注册 / 登录所需的最小操作。
namespace database
{
    /// \brief 用户基础信息，用于登录 / 注册结果返回。
    struct UserInfo
    {
        /// \brief 数据库中的主键 ID。
        i64 id{};
        /// \brief 登录账号。
        std::string account{};
        /// \brief 显示昵称。
        std::string display_name{};
    };

    /// \brief 注册结果。
    struct RegisterResult
    {
        /// \brief 是否注册成功。
        bool ok{};
        /// \brief 错误码，失败时有效。
        std::string error_code{};
        /// \brief 错误信息，失败时有效。
        std::string error_msg{};
        /// \brief 成功时的用户信息。
        UserInfo user{};
    };

    /// \brief 登录结果。
    struct LoginResult
    {
        /// \brief 是否登录成功。
        bool ok{};
        /// \brief 错误码，失败时有效。
        std::string error_code{};
        /// \brief 错误信息，失败时有效。
        std::string error_msg{};
        /// \brief 成功时的用户信息。
        UserInfo user{};
    };

    /// \brief 创建到 PostgreSQL 的连接。
    /// \details 使用 pg_service.conf 中的 service=chatdb 约定。
    auto inline make_connection() -> pqxx::connection
    {
        return pqxx::connection{ "service=chatdb" };
    }

    /// \brief 生成一个随机昵称，例如“微信用户123456”。
    /// \details 使用线程安全的内部随机数引擎。
    auto inline generate_random_display_name() -> std::string
    {
        static std::mt19937_64 engine(
            static_cast<std::mt19937_64::result_type>(
                std::chrono::steady_clock::now().time_since_epoch().count()
            )
        );
        static std::mutex mutex;
        std::lock_guard lock{ mutex };

        auto number = std::uniform_int_distribution<int>{ 100000, 999999 }(engine);
        return "微信用户" + std::to_string(number);
    }

    /// \brief 尝试注册新用户。
    /// \param account 登录账号，需唯一。
    /// \param password 密码，目前直接存储明文，后续可替换为哈希。
    /// \return 包含是否成功、错误信息以及成功时的用户信息。
    auto inline register_user(std::string const& account, std::string const& password) -> RegisterResult
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT id, display_name FROM users WHERE account = " + tx.quote(account);

        auto rows = tx.exec(query);
        if(!rows.empty()) {
            RegisterResult res{};
            res.ok = false;
            res.error_code = "ACCOUNT_EXISTS";
            res.error_msg = "账号已存在";
            return res;
        }

        auto display_name = generate_random_display_name();

        auto const insert =
            "INSERT INTO users (account, password_hash, display_name) "
            "VALUES (" + tx.quote(account) + ", " + tx.quote(password) + ", "
            + tx.quote(display_name) + ") "
            "RETURNING id, account, display_name";

        auto result = tx.exec(insert);
        if(result.empty()) {
            RegisterResult res{};
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = "插入用户失败";
            return res;
        }
        auto row = result[0];
        tx.commit();

        RegisterResult res{};
        res.ok = true;
        res.user.id = row[0].as<i64>();
        res.user.account = row[1].as<std::string>();
        res.user.display_name = row[2].as<std::string>();
        return res;
    }

    /// \brief 尝试登录用户。
    /// \param account 登录账号。
    /// \param password 密码，当前为明文比较。
    /// \return 登录结果，包含错误信息或用户信息。
    auto inline login_user(std::string const& account, std::string const& password) -> LoginResult
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT id, password_hash, display_name "
            "FROM users WHERE account = " + tx.quote(account);

        auto rows = tx.exec(query);
        if(rows.empty()) {
            LoginResult res{};
            res.ok = false;
            res.error_code = "LOGIN_FAILED";
            res.error_msg = "账号不存在或密码错误";
            return res;
        }

        auto row = rows[0];
        auto stored_password = row[1].as<std::string>();
        if(stored_password != password) {
            LoginResult res{};
            res.ok = false;
            res.error_code = "LOGIN_FAILED";
            res.error_msg = "账号不存在或密码错误";
            return res;
        }

        auto const update =
            "UPDATE users SET last_login_at = now() WHERE id = " + tx.quote(row[0].as<i64>());
        tx.exec(update);
        tx.commit();

        LoginResult res{};
        res.ok = true;
        res.user.id = row[0].as<i64>();
        res.user.account = account;
        res.user.display_name = row[2].as<std::string>();
        return res;
    }
} // namespace database
