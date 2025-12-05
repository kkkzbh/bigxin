#include <database/friend.h>
#include <database/connection.h>
#include <database/conversation.h>
#include <utility.h>

#include <boost/mysql.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <stdexcept>
#include <tuple>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace database
{
    auto is_friend(i64 user_id, i64 peer_id) -> asio::awaitable<bool>
    {
        if(user_id <= 0 || peer_id <= 0 || user_id == peer_id) {
            co_return false;
        }
        auto conn_h = co_await acquire_connection();
        mysql::results r;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT 1 FROM friends WHERE user_id={} AND friend_user_id={} LIMIT 1",
                user_id,
                peer_id),
            r,
            asio::use_awaitable
        );
        co_return !r.rows().empty();
    }

    auto load_user_friends(i64 user_id) -> asio::awaitable<std::vector<FriendInfo>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT u.id, u.account, u.display_name, u.avatar_path FROM friends f"
                " JOIN users u ON u.id = f.friend_user_id"
                " WHERE f.user_id = {} ORDER BY u.id ASC",
                user_id),
            r,
            asio::use_awaitable
        );

        std::vector<FriendInfo> result;
        result.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            FriendInfo info{};
            info.id = row.at(0).as_int64();
            info.account = row.at(1).as_string();
            info.display_name = row.at(2).as_string();
            info.avatar_path = row.at(3).is_null() ? "" : std::string(row.at(3).as_string());
            result.push_back(std::move(info));
        }
        co_return result;
    }

    auto search_friend_by_account(i64 current_user_id, std::string const& account)
        -> asio::awaitable<SearchFriendResult>
    {
        SearchFriendResult res{};
        if(account.empty()) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "账号不能为空";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT id, account, display_name, avatar_path FROM users WHERE account={} LIMIT 1",
                account),
            r,
            asio::use_awaitable
        );

        if(r.rows().empty()) {
            res.ok = false;
            res.error_code = "NOT_FOUND";
            res.error_msg = "账号不存在";
            co_return res;
        }

        auto row = r.rows().front();
        auto target_id = row.at(0).as_int64();

        res.ok = true;
        res.found = true;
        res.user.id = target_id;
        res.user.account = row.at(1).as_string();
        res.user.display_name = row.at(2).as_string();
        res.user.avatar_path = row.at(3).is_null() ? "" : std::string(row.at(3).as_string());
        res.is_self = (current_user_id == target_id);
        res.is_friend = !res.is_self && co_await is_friend(current_user_id, target_id);
        co_return res;
    }

    auto create_friend_request(
        i64 from_user_id,
        i64 to_user_id,
        std::string const& source,
        std::string const& hello_msg
    ) -> asio::awaitable<FriendRequestResult>
    {
        FriendRequestResult res{};

        if(from_user_id <= 0 || to_user_id <= 0 || from_user_id == to_user_id) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的好友申请参数";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        try {
            co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

            // 用户存在性
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT 1 FROM users WHERE id={} LIMIT 1",
                    to_user_id),
                r,
                asio::use_awaitable
            );
            if(r.rows().empty()) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "目标用户不存在";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 已是好友
            if(co_await is_friend(from_user_id, to_user_id)) {
                res.ok = false;
                res.error_code = "ALREADY_FRIEND";
                res.error_msg = "已是好友";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 待处理申请
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT 1 FROM friend_requests WHERE status='PENDING' AND"
                    " ((from_user_id={} AND to_user_id={}) OR (from_user_id={} AND to_user_id={}))"
                    " LIMIT 1",
                    from_user_id,
                    to_user_id,
                    to_user_id,
                    from_user_id),
                r,
                asio::use_awaitable
            );
            if(!r.rows().empty()) {
                res.ok = false;
                res.error_code = "ALREADY_PENDING";
                res.error_msg = "已存在待处理的好友申请";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 插入申请
            co_await conn_h->async_execute(
                mysql::with_params(
                    "INSERT INTO friend_requests (from_user_id, to_user_id, status, source, hello_msg)"
                    " VALUES ({}, {}, 'PENDING', {}, {})",
                    from_user_id,
                    to_user_id,
                    source,
                    hello_msg),
                r,
                asio::use_awaitable
            );

            res.ok = true;
            res.request_id = static_cast<i64>(r.last_insert_id());

            co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
            co_return res;
        } catch(std::exception const& ex) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
            co_return res;
        }
    }

    auto load_incoming_friend_requests(i64 user_id) -> asio::awaitable<std::vector<FriendRequestInfo>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT fr.id, fr.from_user_id, u.account, u.display_name, fr.status,"
                " COALESCE(fr.hello_msg, ''), u.avatar_path "
                " FROM friend_requests fr JOIN users u ON u.id = fr.from_user_id"
                " WHERE fr.to_user_id = {} AND fr.status IN ('PENDING','ACCEPTED')"
                " ORDER BY fr.created_at DESC",
                user_id),
            r,
            asio::use_awaitable
        );

        std::vector<FriendRequestInfo> result;
        result.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            FriendRequestInfo info{};
            info.id = row.at(0).as_int64();
            info.from_user_id = row.at(1).as_int64();
            info.account = row.at(2).as_string();
            info.display_name = row.at(3).as_string();
            info.status = row.at(4).as_string();
            info.hello_msg = row.at(5).as_string();
            info.status = row.at(4).as_string();
            info.hello_msg = row.at(5).as_string();
            info.avatar_path = row.at(6).is_null() ? "" : std::string(row.at(6).as_string());
            result.push_back(std::move(info));
        }
        co_return result;
    }

    auto accept_friend_request(i64 request_id, i64 current_user_id)
        -> asio::awaitable<AcceptFriendRequestResult>
    {
        AcceptFriendRequestResult res{};
        if(request_id <= 0 || current_user_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的好友申请参数";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        i64 from_user_id{};
        i64 to_user_id{};
        std::string status;

        try {
            co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

            // 锁定申请
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT from_user_id, to_user_id, status FROM friend_requests"
                    " WHERE id={} FOR UPDATE",
                    request_id),
                r,
                asio::use_awaitable
            );

            if(r.rows().empty()) {
                res.ok = false;
                res.error_code = "NOT_FOUND";
                res.error_msg = "好友申请不存在";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            auto row = r.rows().front();
            from_user_id = row.at(0).as_int64();
            to_user_id = row.at(1).as_int64();
            status = row.at(2).as_string();

            if(to_user_id != current_user_id) {
                res.ok = false;
                res.error_code = "FORBIDDEN";
                res.error_msg = "无权处理该好友申请";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }
            if(status != "PENDING") {
                res.ok = false;
                res.error_code = "INVALID_STATE";
                res.error_msg = "好友申请状态已变更";
                co_await conn_h->async_execute("ROLLBACK", r, asio::use_awaitable);
                co_return res;
            }

            // 建立好友关系
            co_await conn_h->async_execute(
                mysql::with_params(
                    "INSERT IGNORE INTO friends (user_id, friend_user_id)"
                    " VALUES ({}, {}), ({}, {})",
                    from_user_id,
                    to_user_id,
                    to_user_id,
                    from_user_id),
                r,
                asio::use_awaitable
            );

            // 更新申请状态
            co_await conn_h->async_execute(
                mysql::with_params(
                    "UPDATE friend_requests SET status='ACCEPTED', handled_at=CURRENT_TIMESTAMP"
                    " WHERE id={}",
                    request_id),
                r,
                asio::use_awaitable
            );

            co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
        } catch(std::exception const& ex) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = ex.what();
            co_return res;
        }

        // 直接查 by id
        {
            mysql::results r2;
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT id, account, display_name, avatar_path FROM users WHERE id={} LIMIT 1",
                    from_user_id),
                r2,
                asio::use_awaitable
            );
            if(!r2.rows().empty()) {
                auto row = r2.rows().front();
                res.friend_user.id = row.at(0).as_int64();
                res.friend_user.account = row.at(1).as_string();
                res.friend_user.display_name = row.at(2).as_string();
                res.friend_user.avatar_path = row.at(3).is_null() ? "" : std::string(row.at(3).as_string());
            }
        }

        // 确保单聊会话
        res.conversation_id = co_await get_or_create_single_conversation(from_user_id, to_user_id);
        res.ok = true;
        co_return res;
    }

    auto reject_friend_request(i64 request_id, i64 current_user_id)
        -> asio::awaitable<RejectFriendRequestResult>
    {
        RejectFriendRequestResult res{};

        if(request_id <= 0 || current_user_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的请求参数";
            co_return res;
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        // 查询申请详情
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT from_user_id, to_user_id, status FROM friend_requests WHERE id={} LIMIT 1",
                request_id),
            r,
            asio::use_awaitable
        );
        if(r.rows().empty()) {
            res.ok = false;
            res.error_code = "NOT_FOUND";
            res.error_msg = "好友申请不存在";
            co_return res;
        }

        auto row = r.rows().front();
        auto const from_user_id = row.at(0).as_int64();
        auto const to_user_id = row.at(1).as_int64();
        auto const status = std::string{row.at(2).as_string()};

        if(to_user_id != current_user_id) {
            res.ok = false;
            res.error_code = "PERMISSION_DENIED";
            res.error_msg = "无权操作此申请";
            co_return res;
        }

        if(status != "PENDING") {
            res.ok = false;
            res.error_code = "INVALID_STATE";
            res.error_msg = "好友申请状态已变更";
            co_return res;
        }

        // 更新申请状态为 REJECTED
        co_await conn_h->async_execute(
            mysql::with_params(
                "UPDATE friend_requests SET status='REJECTED', handled_at=CURRENT_TIMESTAMP"
                " WHERE id={}",
                request_id),
            r,
            asio::use_awaitable
        );

        res.ok = true;
        res.from_user_id = from_user_id;
        co_return res;
    }
} // namespace database
