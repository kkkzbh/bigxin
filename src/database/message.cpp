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
    namespace
    {
        auto next_seq(mysql::tcp_connection& conn, i64 conversation_id) -> asio::awaitable<i64>
        {
            mysql::results r;
            co_await conn.async_execute(
                mysql::with_params(
                    "INSERT INTO conversation_sequences (conversation_id, next_seq) VALUES (?, 1)"
                    " ON DUPLICATE KEY UPDATE next_seq = LAST_INSERT_ID(next_seq + 1)",
                    conversation_id),
                r,
                asio::use_awaitable
            );

            co_await conn.async_execute("SELECT LAST_INSERT_ID()", r, asio::use_awaitable);
            if(r.rows().empty()) {
                throw std::runtime_error{"生成消息序列号失败"};
            }
            co_return r.rows().front().at(0).as_int64();
        }
    }

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

        auto seq = co_await next_seq(conn, conversation_id);

        mysql::results r;
        co_await conn.async_execute(
            mysql::with_params(
                "INSERT INTO messages (conversation_id, sender_id, seq, msg_type, content, server_time_ms)"
                " VALUES (?, ?, ?, ?, ?, ?)",
                conversation_id,
                sender_id,
                seq,
                msg_type,
                content,
                now_ms),
            r,
            asio::use_awaitable
        );

        StoredMessage stored{};
        stored.conversation_id = conversation_id;
        stored.id = static_cast<i64>(r.last_insert_id());
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
                    "WHERE m.conversation_id = ? AND m.seq < ? "
                    "ORDER BY m.seq DESC LIMIT ?",
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
                    "WHERE m.conversation_id = ? "
                    "ORDER BY m.seq DESC LIMIT ?",
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
                    "WHERE m.conversation_id = ? AND m.seq > ? "
                    "ORDER BY m.seq ASC LIMIT ?",
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
                    "WHERE m.conversation_id = ? "
                    "ORDER BY m.seq ASC LIMIT ?",
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
