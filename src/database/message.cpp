#include <database/message.h>
#include <database/connection.h>
#include <database/conversation.h>
#include <utility.h>

#include <boost/mysql.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <stdexcept>
#include <algorithm>
namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace database
{
    auto append_text_message(
        i64 conversation_id,
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type
    ) -> asio::awaitable<StoredMessage>
    {
        auto conn_h = co_await acquire_connection();
        auto& conn = *conn_h;

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

        // 使用原子性的 INSERT ... SELECT 避免并发 seq 冲突
        mysql::results r;
        co_await conn.async_execute(
            mysql::with_params(
                "INSERT INTO messages (conversation_id, sender_id, seq, msg_type, content, server_time_ms)"
                " SELECT {}, {}, COALESCE(MAX(seq), 0) + 1, {}, {}, {}"
                " FROM messages WHERE conversation_id = {}",
                conversation_id,
                sender_id,
                msg_type,
                content,
                now_ms,
                conversation_id),
            r,
            asio::use_awaitable
        );

        auto msg_id = static_cast<i64>(r.last_insert_id());

        // 查询实际分配的 seq
        co_await conn.async_execute(
            mysql::with_params(
                "SELECT seq FROM messages WHERE id = {}",
                msg_id),
            r,
            asio::use_awaitable
        );

        i64 seq = 1;
        if(!r.rows().empty()) {
            auto const& fv = r.rows().front().at(0);
            if(fv.is_int64()) {
                seq = fv.as_int64();
            } else if(fv.is_uint64()) {
                seq = static_cast<i64>(fv.as_uint64());
            }
        }

        StoredMessage stored{};
        stored.conversation_id = conversation_id;
        stored.id = msg_id;
        stored.seq = seq;
        stored.server_time_ms = now_ms;
        stored.msg_type = msg_type;
        co_return stored;
    }

    auto append_world_text_message(
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type
    ) -> asio::awaitable<StoredMessage>
    {
        auto conversation_id = co_await get_world_conversation_id();
        co_return co_await append_text_message(conversation_id, sender_id, content, msg_type);
    }

    auto load_user_conversation_history(i64 conversation_id, i64 before_seq, i64 limit)
        -> asio::awaitable<std::vector<LoadedMessage>>
    {
        if(limit <= 0) limit = 50;

        auto conn_h = co_await acquire_connection();
        auto& conn = *conn_h;
        mysql::results r;
        if(before_seq > 0) {
            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT m.id, m.conversation_id, m.sender_id, u.display_name, m.seq, m.msg_type,"
                    " m.content, m.server_time_ms "
                    "FROM messages m JOIN users u ON u.id = m.sender_id "
                    "WHERE m.conversation_id = {} AND m.seq < {} "
                    "ORDER BY m.seq DESC LIMIT {}",
                    conversation_id,
                    before_seq,
                    limit),
                r,
                asio::use_awaitable
            );
        } else {
            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT m.id, m.conversation_id, m.sender_id, u.display_name, m.seq, m.msg_type,"
                    " m.content, m.server_time_ms "
                    "FROM messages m JOIN users u ON u.id = m.sender_id "
                    "WHERE m.conversation_id = {} "
                    "ORDER BY m.seq DESC LIMIT {}",
                    conversation_id,
                    limit),
                r,
                asio::use_awaitable
            );
        }

        std::vector<LoadedMessage> messages;
        messages.reserve(r.rows().size());

        for(auto const& row : r.rows()) {
            LoadedMessage msg{};
            msg.id = row.at(0).as_int64();
            msg.conversation_id = row.at(1).as_int64();
            msg.sender_id = row.at(2).as_int64();
            msg.sender_display_name = row.at(3).as_string();
            msg.seq = row.at(4).as_int64();
            msg.msg_type = row.at(5).as_string();
            msg.content = row.at(6).as_string();
            msg.server_time_ms = row.at(7).as_int64();
            messages.push_back(std::move(msg));
        }

        // We queried in DESC order; return results sorted by seq ascending.
        std::reverse(messages.begin(), messages.end());

        co_return messages;
    }

    auto load_user_conversation_since(i64 conversation_id, i64 after_seq, i64 limit)
        -> asio::awaitable<std::vector<LoadedMessage>>
    {
        if(limit <= 0) limit = 100;

        auto conn_h = co_await acquire_connection();
        auto& conn = *conn_h;
        mysql::results r;
        if(after_seq > 0) {
            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT m.id, m.conversation_id, m.sender_id, u.display_name, m.seq, m.msg_type,"
                    " m.content, m.server_time_ms "
                    "FROM messages m JOIN users u ON u.id = m.sender_id "
                    "WHERE m.conversation_id = {} AND m.seq > {} "
                    "ORDER BY m.seq ASC LIMIT {}",
                    conversation_id,
                    after_seq,
                    limit),
                r,
                asio::use_awaitable
            );
        } else {
            co_await conn.async_execute(
                mysql::with_params(
                    "SELECT m.id, m.conversation_id, m.sender_id, u.display_name, m.seq, m.msg_type,"
                    " m.content, m.server_time_ms "
                    "FROM messages m JOIN users u ON u.id = m.sender_id "
                    "WHERE m.conversation_id = {} "
                    "ORDER BY m.seq ASC LIMIT {}",
                    conversation_id,
                    limit),
                r,
                asio::use_awaitable
            );
        }

        std::vector<LoadedMessage> messages;
        messages.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            LoadedMessage msg{};
            msg.id = row.at(0).as_int64();
            msg.conversation_id = row.at(1).as_int64();
            msg.sender_id = row.at(2).as_int64();
            msg.sender_display_name = row.at(3).as_string();
            msg.seq = row.at(4).as_int64();
            msg.msg_type = row.at(5).as_string();
            msg.content = row.at(6).as_string();
            msg.server_time_ms = row.at(7).as_int64();
            messages.push_back(std::move(msg));
        }
        co_return messages;
    }

    auto load_world_history(i64 before_seq, i64 limit) -> asio::awaitable<std::vector<LoadedMessage>>
    {
        auto conversation_id = co_await get_world_conversation_id();
        co_return co_await load_user_conversation_history(conversation_id, before_seq, limit);
    }
} // namespace database
