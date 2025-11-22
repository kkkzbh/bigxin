#include <database/friend.h>
#include <database/connection.h>
#include <database/conversation.h>
#include <pqxx/pqxx>
#include <utility.h>

namespace database
{
    auto is_friend(i64 user_id, i64 peer_id) -> bool
    {
        if(user_id <= 0 || peer_id <= 0 || user_id == peer_id) {
            return false;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT 1 FROM friends "
            "WHERE user_id = " + tx.quote(user_id)
            + " AND friend_user_id = " + tx.quote(peer_id)
            + " LIMIT 1";

        auto rows = tx.exec(query);
        tx.commit();
        return !rows.empty();
    }

    auto load_user_friends(i64 user_id) -> std::vector<FriendInfo>
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT u.id, u.account, u.display_name "
            "FROM friends f "
            "JOIN users u ON u.id = f.friend_user_id "
            "WHERE f.user_id = " + tx.quote(user_id) + " "
            "ORDER BY u.id ASC";

        auto rows = tx.exec(query);

        std::vector<FriendInfo> result{};
        result.reserve(rows.size());

        for(auto const& row : rows) {
            FriendInfo info{};
            info.id = row[0].as<i64>();
            info.account = row[1].as<std::string>();
            info.display_name = row[2].as<std::string>();
            result.push_back(std::move(info));
        }

        tx.commit();
        return result;
    }

    auto search_friend_by_account(i64 current_user_id, std::string const& account)
        -> SearchFriendResult
    {
        SearchFriendResult res{};

        if(account.empty()) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "账号不能为空";
            return res;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT id, account, display_name "
            "FROM users WHERE account = " + tx.quote(account) + " "
            "LIMIT 1";

        auto rows = tx.exec(query);
        if(rows.empty()) {
            res.ok = false;
            res.error_code = "NOT_FOUND";
            res.error_msg = "账号不存在";
            return res;
        }

        auto const& row = rows[0];
        auto const target_id = row[0].as<i64>();

        res.ok = true;
        res.found = true;
        res.user.id = target_id;
        res.user.account = row[1].as<std::string>();
        res.user.display_name = row[2].as<std::string>();

        res.is_self = (current_user_id == target_id);
        res.is_friend = !res.is_self && is_friend(current_user_id, target_id);

        tx.commit();
        return res;
    }

    auto create_friend_request(
        i64 from_user_id,
        i64 to_user_id,
        std::string const& source,
        std::string const& hello_msg
    ) -> FriendRequestResult
    {
        FriendRequestResult res{};

        if(from_user_id <= 0 || to_user_id <= 0 || from_user_id == to_user_id) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的好友申请参数";
            return res;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        // 确认接收者存在。
        auto const user_query =
            "SELECT 1 FROM users WHERE id = " + tx.quote(to_user_id) + " LIMIT 1";

        auto user_rows = tx.exec(user_query);
        if(user_rows.empty()) {
            res.ok = false;
            res.error_code = "NOT_FOUND";
            res.error_msg = "目标用户不存在";
            return res;
        }

        // 已是好友则直接返回。
        auto const friend_query =
            "SELECT 1 FROM friends "
            "WHERE user_id = " + tx.quote(from_user_id)
            + " AND friend_user_id = " + tx.quote(to_user_id)
            + " LIMIT 1";

        auto friend_rows = tx.exec(friend_query);
        if(!friend_rows.empty()) {
            res.ok = false;
            res.error_code = "ALREADY_FRIEND";
            res.error_msg = "已是好友";
            return res;
        }

        // 检查是否已经存在任意方向的未处理申请。
        auto const pending_query =
            "SELECT 1 FROM friend_requests "
            "WHERE status = " + tx.quote(std::string{ "PENDING" }) + " "
            "AND ("
            "(from_user_id = " + tx.quote(from_user_id)
            + " AND to_user_id = " + tx.quote(to_user_id) + ") OR "
            "(from_user_id = " + tx.quote(to_user_id)
            + " AND to_user_id = " + tx.quote(from_user_id) + ")"
            ") "
            "LIMIT 1";

        auto pending_rows = tx.exec(pending_query);
        if(!pending_rows.empty()) {
            res.ok = false;
            res.error_code = "ALREADY_PENDING";
            res.error_msg = "已存在待处理的好友申请";
            return res;
        }

        auto const insert =
            "INSERT INTO friend_requests (from_user_id, to_user_id, status, source, hello_msg) "
            "VALUES (" + tx.quote(from_user_id) + ", "
            + tx.quote(to_user_id) + ", "
            + tx.quote(std::string{ "PENDING" }) + ", "
            + tx.quote(source) + ", "
            + tx.quote(hello_msg) + ") "
            "RETURNING id";

        auto insert_rows = tx.exec(insert);
        if(insert_rows.empty()) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = "创建好友申请失败";
            return res;
        }

        res.ok = true;
        res.request_id = insert_rows[0][0].as<i64>();
        tx.commit();
        return res;
    }

    auto load_incoming_friend_requests(i64 user_id) -> std::vector<FriendRequestInfo>
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT fr.id, fr.from_user_id, u.account, u.display_name, fr.status, "
            "COALESCE(fr.hello_msg, '') "
            "FROM friend_requests fr "
            "JOIN users u ON u.id = fr.from_user_id "
            "WHERE fr.to_user_id = " + tx.quote(user_id) + " "
            "AND fr.status IN ("
            + tx.quote(std::string{ "PENDING" }) + ", "
            + tx.quote(std::string{ "ACCEPTED" }) + ") "
            "ORDER BY fr.created_at DESC";

        auto rows = tx.exec(query);

        std::vector<FriendRequestInfo> result{};
        result.reserve(rows.size());

        for(auto const& row : rows) {
            FriendRequestInfo info{};
            info.id = row[0].as<i64>();
            info.from_user_id = row[1].as<i64>();
            info.account = row[2].as<std::string>();
            info.display_name = row[3].as<std::string>();
            info.status = row[4].as<std::string>();
            info.hello_msg = row[5].as<std::string>();
            result.push_back(std::move(info));
        }

        tx.commit();
        return result;
    }

    auto accept_friend_request(i64 request_id, i64 current_user_id)
        -> AcceptFriendRequestResult
    {
        AcceptFriendRequestResult res{};

        if(request_id <= 0 || current_user_id <= 0) {
            res.ok = false;
            res.error_code = "INVALID_PARAM";
            res.error_msg = "无效的好友申请参数";
            return res;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT from_user_id, to_user_id, status "
            "FROM friend_requests "
            "WHERE id = " + tx.quote(request_id) + " "
            "FOR UPDATE";

        auto rows = tx.exec(query);
        if(rows.empty()) {
            res.ok = false;
            res.error_code = "NOT_FOUND";
            res.error_msg = "好友申请不存在";
            return res;
        }

        auto const from_user_id = rows[0][0].as<i64>();
        auto const to_user_id = rows[0][1].as<i64>();
        auto const status = rows[0][2].as<std::string>();

        if(to_user_id != current_user_id) {
            res.ok = false;
            res.error_code = "FORBIDDEN";
            res.error_msg = "无权处理该好友申请";
            return res;
        }
        if(status != "PENDING") {
            res.ok = false;
            res.error_code = "INVALID_STATE";
            res.error_msg = "好友申请状态已变更";
            return res;
        }

        // 建立双向好友关系（幂等）。
        auto const insert_friends =
            "INSERT INTO friends (user_id, friend_user_id) "
            "VALUES (" + tx.quote(from_user_id) + ", " + tx.quote(to_user_id) + "), ("
            + tx.quote(to_user_id) + ", " + tx.quote(from_user_id) + ") "
            "ON CONFLICT DO NOTHING";

        tx.exec(insert_friends);

        auto const update_req =
            "UPDATE friend_requests "
            "SET status = " + tx.quote(std::string{ "ACCEPTED" })
            + ", handled_at = now() "
            + "WHERE id = " + tx.quote(request_id);

        tx.exec(update_req);

        // 加载好友用户基础信息。
        auto const user_query =
            "SELECT id, account, display_name FROM users "
            "WHERE id = " + tx.quote(from_user_id) + " LIMIT 1";

        auto user_rows = tx.exec(user_query);
        if(user_rows.empty()) {
            res.ok = false;
            res.error_code = "SERVER_ERROR";
            res.error_msg = "好友用户数据不存在";
            return res;
        }

        auto const& user_row = user_rows[0];
        res.friend_user.id = user_row[0].as<i64>();
        res.friend_user.account = user_row[1].as<std::string>();
        res.friend_user.display_name = user_row[2].as<std::string>();

        // 确保存在单聊会话。
        res.conversation_id = get_or_create_single_conversation(from_user_id, to_user_id);

        res.ok = true;
        tx.commit();
        return res;
    }
} // namespace database
