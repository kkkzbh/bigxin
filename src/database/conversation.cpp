#include <database/conversation.h>
#include <database/connection.h>
#include <utility.h>

#include <boost/mysql.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <tuple>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace database
{
    auto get_world_conversation_id() -> asio::awaitable<i64>
    {
        auto conn_h = co_await acquire_connection();

        mysql::results r;
        co_await conn_h->async_execute(
            "SELECT id FROM conversations WHERE type='GROUP' AND name='世界' LIMIT 1",
            r,
            asio::use_awaitable
        );

        if(r.rows().empty()) {
            throw std::runtime_error{ "世界会话不存在" };
        }

        co_return r.rows().front().at(0).as_int64();
    }

    auto get_or_create_single_conversation(i64 user1, i64 user2) -> asio::awaitable<i64>
    {
        if(user1 <= 0 || user2 <= 0 || user1 == user2) {
            throw std::runtime_error{ "无效的单聊会话参与者" };
        }

        auto a = std::min(user1, user2);
        auto b = std::max(user1, user2);

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

        // 1) 直接查是否已有
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT conversation_id FROM single_conversations"
                " WHERE user1_id={} AND user2_id={} LIMIT 1",
                a,
                b),
            r,
            asio::use_awaitable
        );
        if(!r.rows().empty()) {
            co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
            co_return r.rows().front().at(0).as_int64();
        }

        // 2) 创建 conversations 记录
        co_await conn_h->async_execute(
            mysql::with_params(
                "INSERT INTO conversations (type, name, owner_user_id)"
                " VALUES ('SINGLE', '', {})",
                user1),
            r,
            asio::use_awaitable
        );
        auto conv_id = static_cast<i64>(r.last_insert_id());

        // 3) 成员关系
        co_await conn_h->async_execute(
            mysql::with_params(
                "INSERT INTO conversation_members (conversation_id, user_id, role)"
                " VALUES ({}, {}, 'MEMBER'), ({}, {}, 'MEMBER')",
                conv_id,
                user1,
                conv_id,
                user2),
            r,
            asio::use_awaitable
        );

        // 4) 记录 single_conversations，利用唯一约束避免重复
        co_await conn_h->async_execute(
            mysql::with_params(
                "INSERT INTO single_conversations (user1_id, user2_id, conversation_id)"
                " VALUES ({}, {}, {})"
                " ON DUPLICATE KEY UPDATE conversation_id=VALUES(conversation_id)",
                a,
                b,
                conv_id),
            r,
            asio::use_awaitable
        );

        co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
        co_return conv_id;
    }

    auto create_group_conversation(i64 creator_id, std::vector<i64> member_ids, std::string name)
        -> asio::awaitable<i64>
    {
        if(creator_id <= 0) {
            throw std::runtime_error{ "无效的群主 ID" };
        }

        // 去重并移除 creator
        std::sort(member_ids.begin(), member_ids.end());
        member_ids.erase(std::unique(member_ids.begin(), member_ids.end()), member_ids.end());
        member_ids.erase(std::remove(member_ids.begin(), member_ids.end(), creator_id), member_ids.end());

        if(member_ids.size() < 2) {
            throw std::runtime_error{ "群成员不足（至少需要群主 + 2 位好友）" };
        }

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        auto pick_display_name = [&](i64 uid) -> asio::awaitable<std::string>
        {
            mysql::results local;
            co_await conn_h->async_execute(
                mysql::with_params(
                    "SELECT display_name FROM users WHERE id={} LIMIT 1",
                    uid),
                local,
                asio::use_awaitable
            );
            if(local.rows().empty()) co_return std::string{};
            co_return local.rows().front().at(0).as_string();
        };

        if(name.empty()) {
            std::vector<i64> order;
            order.reserve(member_ids.size() + 1);
            order.push_back(creator_id);
            order.insert(order.end(), member_ids.begin(), member_ids.end());

            std::vector<std::string> picked;
            for(size_t i = 0; i < order.size() && picked.size() < 3; ++i) {
                auto display = co_await pick_display_name(order[i]);
                if(!display.empty()) {
                    picked.push_back(std::move(display));
                }
            }

            std::string joined;
            for(size_t i = 0; i < picked.size(); ++i) {
                if(i > 0) joined += "、";
                joined += picked[i];
            }
            if(order.size() > 3) joined += "等";
            if(joined.empty()) joined = "群聊";
            name = std::move(joined);
        }

        co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

        // 创建群聊
        co_await conn_h->async_execute(
            mysql::with_params(
                "INSERT INTO conversations (type, name, owner_user_id)"
                " VALUES ('GROUP', {}, {})",
                name,
                creator_id),
            r,
            asio::use_awaitable
        );
        auto conv_id = static_cast<i64>(r.last_insert_id());

        // 插入群主
        co_await conn_h->async_execute(
            mysql::with_params(
                "INSERT INTO conversation_members (conversation_id, user_id, role)"
                " VALUES ({}, {}, 'OWNER')",
                conv_id,
                creator_id),
            r,
            asio::use_awaitable
        );

        // 插入成员
        for(auto uid : member_ids) {
            co_await conn_h->async_execute(
                mysql::with_params(
                    "INSERT INTO conversation_members (conversation_id, user_id, role)"
                    " VALUES ({}, {}, 'MEMBER')",
                    conv_id,
                    uid),
                r,
                asio::use_awaitable
            );
        }

        co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
        co_return conv_id;
    }

    auto load_user_conversations(i64 user_id) -> asio::awaitable<std::vector<ConversationInfo>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT c.id, c.type, c.name, peer.display_name AS peer_name,"
                " COALESCE(msg_stats.max_seq, 0) AS last_seq, COALESCE(msg_stats.max_time, 0) AS last_time "
                "FROM conversations c "
                "JOIN conversation_members cm ON cm.conversation_id = c.id "
                "LEFT JOIN ("
                "  SELECT cm2.conversation_id, u.display_name "
                "  FROM conversation_members cm2 "
                "  JOIN users u ON u.id = cm2.user_id "
                "  WHERE cm2.user_id <> {}"
                ") peer ON peer.conversation_id = c.id AND c.type = 'SINGLE' "
                "LEFT JOIN ("
                "  SELECT conversation_id, MAX(seq) AS max_seq, MAX(server_time_ms) AS max_time "
                "  FROM messages GROUP BY conversation_id"
                ") msg_stats ON msg_stats.conversation_id = c.id "
                "WHERE cm.user_id = {} ORDER BY c.id ASC",
                user_id,
                user_id),
            r,
            asio::use_awaitable
        );

        std::vector<ConversationInfo> result;
        result.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            ConversationInfo info{};
            info.id = row.at(0).as_int64();
            info.type = row.at(1).as_string();
            auto stored_name = row.at(2).as_string();
            if(info.type == "GROUP") {
                info.title = stored_name;
            } else if(info.type == "SINGLE") {
                if(!row.at(3).is_null()) {
                    info.title = row.at(3).as_string();
                } else {
                    info.title = stored_name;
                }
            } else {
                info.title = stored_name;
            }
            info.last_seq = row.at(4).as_int64();
            info.last_server_time_ms = row.at(5).as_int64();
            result.push_back(std::move(info));
        }
        co_return result;
    }

    auto get_conversation_member(i64 conversation_id, i64 user_id)
        -> asio::awaitable<std::optional<MemberInfo>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT cm.role, cm.muted_until_ms, u.display_name "
                "FROM conversation_members cm JOIN users u ON u.id = cm.user_id "
                "WHERE cm.conversation_id = {} AND cm.user_id = {} LIMIT 1",
                conversation_id,
                user_id),
            r,
            asio::use_awaitable
        );

        if(r.rows().empty()) {
            co_return std::nullopt;
        }

        auto row = r.rows().front();
        MemberInfo info{};
        info.user_id = user_id;
        info.role = row.at(0).as_string();
        info.muted_until_ms = row.at(1).as_int64();
        info.display_name = row.at(2).as_string();
        co_return info;
    }

    auto set_member_mute_until(i64 conversation_id, i64 user_id, i64 muted_until_ms)
        -> asio::awaitable<void>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "UPDATE conversation_members SET muted_until_ms = {}"
                " WHERE conversation_id = {} AND user_id = {}",
                muted_until_ms,
                conversation_id,
                user_id),
            r,
            asio::use_awaitable
        );
        co_return;
    }

    auto set_member_role(i64 conversation_id, i64 user_id, std::string const& role)
        -> asio::awaitable<void>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "UPDATE conversation_members SET role = {}"
                " WHERE conversation_id = {} AND user_id = {}",
                role,
                conversation_id,
                user_id),
            r,
            asio::use_awaitable
        );
        co_return;
    }

    auto load_conversation_members(i64 conversation_id) -> asio::awaitable<std::vector<MemberInfo>>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT cm.user_id, cm.role, cm.muted_until_ms, u.display_name "
                "FROM conversation_members cm JOIN users u ON u.id = cm.user_id "
                "WHERE cm.conversation_id = {} ORDER BY cm.user_id ASC",
                conversation_id),
            r,
            asio::use_awaitable
        );

        std::vector<MemberInfo> members;
        members.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            MemberInfo info{};
            info.user_id = row.at(0).as_int64();
            info.role = row.at(1).as_string();
            info.muted_until_ms = row.at(2).as_int64();
            info.display_name = row.at(3).as_string();
            members.push_back(std::move(info));
        }
        co_return members;
    }

    auto remove_conversation_member(i64 conversation_id, i64 user_id) -> asio::awaitable<void>
    {
        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM conversation_members WHERE conversation_id={} AND user_id={}",
                conversation_id,
                user_id),
            r,
            asio::use_awaitable
        );
        co_return;
    }

    auto dissolve_conversation(i64 conversation_id) -> asio::awaitable<void>
    {
        if(conversation_id <= 0) co_return;

        auto conn_h = co_await acquire_connection();
        mysql::results r;

        co_await conn_h->async_execute("START TRANSACTION", r, asio::use_awaitable);

        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM messages WHERE conversation_id={}",
                conversation_id),
            r,
            asio::use_awaitable
        );

        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM conversation_members WHERE conversation_id={}",
                conversation_id),
            r,
            asio::use_awaitable
        );

        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM single_conversations WHERE conversation_id={}",
                conversation_id),
            r,
            asio::use_awaitable
        );

        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM conversation_sequences WHERE conversation_id={}",
                conversation_id),
            r,
            asio::use_awaitable
        );

        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM conversations WHERE id={}",
                conversation_id),
            r,
            asio::use_awaitable
        );

        co_await conn_h->async_execute("COMMIT", r, asio::use_awaitable);
    }
} // namespace database
