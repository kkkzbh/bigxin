#include <database/auth.h>
#include <database/connection.h>
#include <pqxx/pqxx>
#include <utility.h>

namespace database
{
    auto register_user(std::string const& account, std::string const& password) -> RegisterResult
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

        auto const user_id = row[0].as<i64>();

        auto const world_query =
            "SELECT id FROM conversations "
            "WHERE type = " + tx.quote("GROUP") + " AND name = " + tx.quote("世界") + " "
            "LIMIT 1";

        auto world_rows = tx.exec(world_query);
        if(!world_rows.empty()) {
            auto const world_id = world_rows[0][0].as<i64>();

            auto const member_insert =
                "INSERT INTO conversation_members (conversation_id, user_id, role) "
                "VALUES (" + tx.quote(world_id) + ", " + tx.quote(user_id) + ", 'MEMBER') "
                "ON CONFLICT DO NOTHING";

            tx.exec(member_insert);
        }

        tx.commit();

        RegisterResult res{};
        res.ok = true;
        res.user.id = user_id;
        res.user.account = row[1].as<std::string>();
        res.user.display_name = row[2].as<std::string>();
        return res;
    }

    auto update_display_name(i64 user_id, std::string const& new_name)
        -> UpdateDisplayNameResult
    {
        UpdateDisplayNameResult res{};

        if(user_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的用户 ID";
            return res;
        }
        if(new_name.empty()) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "昵称不能为空";
            return res;
        }

        try {
            auto conn = make_connection();
            pqxx::work tx{ conn };

            auto const update =
                "UPDATE users SET display_name = " + tx.quote(new_name)
                + " WHERE id = " + tx.quote(user_id)
                + " RETURNING id, account, display_name";

            auto rows = tx.exec(update);
            if(rows.empty()) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "用户不存在";
                return res;
            }

            auto const& row = rows[0];
            tx.commit();

            res.ok = true;
            res.user.id = row[0].as<i64>();
            res.user.account = row[1].as<std::string>();
            res.user.display_name = row[2].as<std::string>();
            return res;
        } catch(std::exception const& ex) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
            return res;
        }
    }

    auto login_user(std::string const& account, std::string const& password) -> LoginResult
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
