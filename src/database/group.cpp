#include <database/group.h>
#include <database/connection.h>
#include <utility.h>

#include <boost/mysql.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <stdexcept>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace database
{
    auto search_group_by_id(i64 current_user_id, i64 group_id)
        -> asio::awaitable<SearchGroupResult>
    {
        SearchGroupResult res{};
        if(group_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "群号不能为空";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        // 查询群聊信息（仅 GROUP 类型）
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT c.id, c.name, "
                "(SELECT COUNT(*) FROM conversation_members WHERE conversation_id = c.id) as member_count "
                "FROM conversations c WHERE c.id = {} AND c.type = 'GROUP' LIMIT 1",
                group_id),
            r,
            asio::use_awaitable
        );

        if(r.rows().empty()) {
            res.ok = false;
            res.error_code = "NOT_FOUND";
            res.error_msg = "群聊不存在";
            co_return res;
        }

        auto row = r.rows().front();
        res.ok = true;
        res.group_id = row.at(0).as_int64();
        res.name = row.at(1).as_string();
        res.member_count = row.at(2).as_int64();

        // 检查当前用户是否已是群成员
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT 1 FROM conversation_members WHERE conversation_id = {} AND user_id = {} LIMIT 1",
                group_id,
                current_user_id),
            r,
            asio::use_awaitable
        );
        res.is_member = !r.rows().empty();

        co_return res;
    }

    auto create_group_join_request(
        i64 from_user_id,
        i64 group_id,
        std::string const& hello_msg
    ) -> asio::awaitable<GroupJoinRequestResult>
    {
        GroupJoinRequestResult res{};

        if(from_user_id <= 0 || group_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的入群申请参数";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        try {
            co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

            // 群聊存在性检查
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT 1 FROM conversations WHERE id = {} AND type = 'GROUP' LIMIT 1",
                    group_id),
                r,
                asio::use_awaitable
            );
            if(r.rows().empty()) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "群聊不存在";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 已是群成员
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT 1 FROM conversation_members WHERE conversation_id = {} AND user_id = {} LIMIT 1",
                    group_id,
                    from_user_id),
                r,
                asio::use_awaitable
            );
            if(!r.rows().empty()) {
                res.ok = false;
                res.error_code = "ALREADY_MEMBER";
                res.error_msg = "你已经是群成员";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 待处理申请（允许重复申请，但检查是否有 PENDING 状态的）
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT 1 FROM group_join_requests WHERE status = 'PENDING' AND "
                    "from_user_id = {} AND group_id = {} LIMIT 1",
                    from_user_id,
                    group_id),
                r,
                asio::use_awaitable
            );
            if(!r.rows().empty()) {
                res.ok = false;
                res.error_code = "ALREADY_PENDING";
                res.error_msg = "已存在待处理的入群申请";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 插入申请
            co_await conn_h->async_execute(
                mysql::with_params(
                    "INSERT INTO group_join_requests (from_user_id, group_id, status, hello_msg) "
                    "VALUES ({}, {}, 'PENDING', {})",
                    from_user_id,
                    group_id,
                    hello_msg),
                r,
                asio::use_awaitable
            );

            res.ok = true;
            res.request_id = static_cast<i64>(r.last_insert_id());

            co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
        } catch(std::exception const& ex) {
            // Note: Transaction will be rolled back automatically when connection is released
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
        }
        co_return res;
    }

    auto load_group_join_requests_for_admin(i64 user_id)
        -> asio::awaitable<std::vector<GroupJoinRequestInfo>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        // 查询当前用户作为群主或管理员的所有群聊的入群申请
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT gjr.id, gjr.from_user_id, u.account, u.display_name, "
                "gjr.group_id, c.name, gjr.status, COALESCE(gjr.hello_msg, ''), u.avatar_path "
                "FROM group_join_requests gjr "
                "JOIN users u ON u.id = gjr.from_user_id "
                "JOIN conversations c ON c.id = gjr.group_id "
                "WHERE gjr.group_id IN ("
                "  SELECT conversation_id FROM conversation_members "
                "  WHERE user_id = {} AND role IN ('OWNER', 'ADMIN')"
                ") AND gjr.status IN ('PENDING', 'ACCEPTED', 'REJECTED') "
                "ORDER BY gjr.created_at DESC",
                user_id),
            r,
            asio::use_awaitable
        );

        std::vector<GroupJoinRequestInfo> result;
        result.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            GroupJoinRequestInfo info{};
            info.id = row.at(0).as_int64();
            info.from_user_id = row.at(1).as_int64();
            info.account = row.at(2).as_string();
            info.display_name = row.at(3).as_string();
            info.group_id = row.at(4).as_int64();
            info.group_name = row.at(5).as_string();
            info.status = row.at(6).as_string();
            info.hello_msg = row.at(7).as_string();
            info.avatar_path = row.at(8).is_null() ? "" : std::string(row.at(8).as_string());
            result.push_back(std::move(info));
        }
        co_return result;
    }

    auto handle_group_join_request(i64 request_id, i64 handler_user_id, bool accept)
        -> asio::awaitable<AcceptGroupJoinResult>
    {
        AcceptGroupJoinResult res{};
        if(request_id <= 0 || handler_user_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的入群申请参数";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        i64 from_user_id{};
        i64 group_id{};
        std::string status;

        try {
            co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

            // 锁定申请
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT from_user_id, group_id, status FROM group_join_requests "
                    "WHERE id = {} FOR UPDATE",
                    request_id),
                r,
                asio::use_awaitable
            );
            if(r.rows().empty()) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "入群申请不存在";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            auto row = r.rows().front();
            from_user_id = row.at(0).as_int64();
            group_id = row.at(1).as_int64();
            status = row.at(2).as_string();

            // 检查处理人是否为群主或管理员
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT role FROM conversation_members "
                    "WHERE conversation_id = {} AND user_id = {} LIMIT 1",
                    group_id,
                    handler_user_id),
                r,
                asio::use_awaitable
            );
            if(r.rows().empty()) {
                res.ok = false;
                res.error_code = "NO_PERMISSION";
                res.error_msg = "你不是该群成员";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            auto handler_role = std::string{ r.rows().front().at(0).as_string() };
            if(handler_role != "OWNER" && handler_role != "ADMIN") {
                res.ok = false;
                res.error_code = "NO_PERMISSION";
                res.error_msg = "只有群主或管理员可以处理入群申请";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 检查申请状态
            if(status != "PENDING") {
                res.ok = false;
                res.error_code = "ALREADY_HANDLED";
                res.error_msg = "该申请已被处理";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 更新申请状态
            auto const new_status = accept ? "ACCEPTED" : "REJECTED";
            co_await conn_h->async_execute(
                mysql::with_params(
                    "UPDATE group_join_requests SET status = {}, handler_user_id = {}, "
                    "handled_at = CURRENT_TIMESTAMP WHERE id = {}",
                    new_status,
                    handler_user_id,
                    request_id),
                r,
                asio::use_awaitable
            );

            // 如果同意，添加为群成员
            if(accept) {
                co_await conn_h->async_execute(
                    mysql::with_params(
                        "INSERT INTO conversation_members (conversation_id, user_id, role) "
                        "VALUES ({}, {}, 'MEMBER')",
                        group_id,
                        from_user_id),
                    r,
                    asio::use_awaitable
                );
            }

            // 获取申请者信息
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT id, account, display_name FROM users WHERE id = {} LIMIT 1",
                    from_user_id),
                r,
                asio::use_awaitable
            );
            if(!r.rows().empty()) {
                auto user_row = r.rows().front();
                res.new_member.id = user_row.at(0).as_int64();
                res.new_member.account = user_row.at(1).as_string();
                res.new_member.display_name = user_row.at(2).as_string();
            }

            // 获取群名称
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT name FROM conversations WHERE id = {} LIMIT 1",
                    group_id),
                r,
                asio::use_awaitable
            );
            if(!r.rows().empty()) {
                res.group_name = r.rows().front().at(0).as_string();
            }

            res.ok = true;
            res.group_id = group_id;

            co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
        } catch(std::exception const& ex) {
            // Note: Transaction will be rolled back automatically when connection is released
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
        }
        co_return res;
    }

    auto get_group_admins(i64 group_id)
        -> asio::awaitable<std::vector<i64>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT user_id FROM conversation_members "
                "WHERE conversation_id = {} AND role IN ('OWNER', 'ADMIN')",
                group_id),
            r,
            asio::use_awaitable
        );

        std::vector<i64> result;
        result.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            result.push_back(row.at(0).as_int64());
        }
        co_return result;
    }
} // namespace database
