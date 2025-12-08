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

        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

        // 使用原子性的 INSERT ... SELECT 避免并发 seq 冲突
        mysql::results r;
        co_await conn_h->async_execute(
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
        co_await conn_h->async_execute(
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
        mysql::results r;
        if(before_seq > 0) {
            co_await conn_h->async_execute(
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
            co_await conn_h->async_execute(
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
            
            // 加载该消息的反应
            msg.reactions = co_await get_message_reactions(msg.id);
            
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
        mysql::results r;
        if(after_seq > 0) {
            co_await conn_h->async_execute(
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
            co_await conn_h->async_execute(
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
            
            // 加载该消息的反应
            msg.reactions = co_await get_message_reactions(msg.id);
            
            messages.push_back(std::move(msg));
        }
        co_return messages;
    }

    auto load_world_history(i64 before_seq, i64 limit) -> asio::awaitable<std::vector<LoadedMessage>>
    {
        auto conversation_id = co_await get_world_conversation_id();
        co_return co_await load_user_conversation_history(conversation_id, before_seq, limit);
    }

    auto recall_message(i64 message_id, i64 recaller_id) -> asio::awaitable<RecallMessageResult>
    {
        auto conn_h = co_await acquire_connection();
        
        // 1. 查询消息是否存在及所属会话
        mysql::results r_msg;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT conversation_id, sender_id FROM messages WHERE id = {}",
                message_id),
            r_msg,
            asio::use_awaitable
        );

        if(r_msg.rows().empty()) {
            RecallMessageResult result{};
            result.ok = false;
            result.error_code = "MESSAGE_NOT_FOUND";
            result.error_msg = "消息不存在";
            co_return result;
        }

        auto row = r_msg.rows().at(0);
        auto conversation_id = row.at(0).as_int64();
        auto sender_id = row.at(1).as_int64();

        // 2. 查询撤回者昵称
        mysql::results r_user;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT display_name FROM users WHERE id = {}",
                recaller_id),
            r_user,
            asio::use_awaitable
        );

        std::string recaller_name = r_user.rows().empty() ? "" : r_user.rows().at(0).at(0).as_string();

        // 3. 设置消息的 is_recalled 标记
        mysql::results r_update;
        co_await conn_h->async_execute(
            mysql::with_params(
                "UPDATE messages SET is_recalled = TRUE WHERE id = {}",
                message_id),
            r_update,
            asio::use_awaitable
        );

        RecallMessageResult result{};
        result.ok = true;
        result.conversation_id = conversation_id;
        result.message_id = message_id;
        result.recaller_id = recaller_id;
        result.recaller_name = recaller_name;
        co_return result;
    }

    auto add_message_reaction(i64 message_id, i64 user_id, std::string const& reaction_type)
        -> asio::awaitable<MessageReactionResult>
    {
        auto conn_h = co_await acquire_connection();

        // 1. 查询消息所属会话
        mysql::results r_msg;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT conversation_id FROM messages WHERE id = {}",
                message_id),
            r_msg,
            asio::use_awaitable
        );

        if(r_msg.rows().empty()) {
            MessageReactionResult result{};
            result.ok = false;
            result.error_code = "MESSAGE_NOT_FOUND";
            result.error_msg = "消息不存在";
            co_return result;
        }

        auto conversation_id = r_msg.rows().at(0).at(0).as_int64();

        // 2. 插入或更新反应 (使用 INSERT ... ON DUPLICATE KEY UPDATE)
        mysql::results r_insert;
        co_await conn_h->async_execute(
            mysql::with_params(
                "INSERT INTO message_reactions (message_id, user_id, reaction_type) "
                "VALUES ({}, {}, {}) "
                "ON DUPLICATE KEY UPDATE reaction_type = {}",
                message_id,
                user_id,
                reaction_type,
                reaction_type),
            r_insert,
            asio::use_awaitable
        );

        // 3. 查询该消息的所有反应
        auto reactions = co_await get_message_reactions(message_id);

        MessageReactionResult result{};
        result.ok = true;
        result.conversation_id = conversation_id;
        result.message_id = message_id;
        result.reactions = std::move(reactions);
        co_return result;
    }

    auto remove_message_reaction(i64 message_id, i64 user_id, std::string const& reaction_type)
        -> asio::awaitable<MessageReactionResult>
    {
        auto conn_h = co_await acquire_connection();

        // 1. 查询消息所属会话
        mysql::results r_msg;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT conversation_id FROM messages WHERE id = {}",
                message_id),
            r_msg,
            asio::use_awaitable
        );

        if(r_msg.rows().empty()) {
            MessageReactionResult result{};
            result.ok = false;
            result.error_code = "MESSAGE_NOT_FOUND";
            result.error_msg = "消息不存在";
            co_return result;
        }

        auto conversation_id = r_msg.rows().at(0).at(0).as_int64();

        // 2. 删除反应
        mysql::results r_delete;
        co_await conn_h->async_execute(
            mysql::with_params(
                "DELETE FROM message_reactions WHERE message_id = {} AND user_id = {} AND reaction_type = {}",
                message_id,
                user_id,
                reaction_type),
            r_delete,
            asio::use_awaitable
        );

        // 3. 查询该消息的所有反应
        auto reactions = co_await get_message_reactions(message_id);

        MessageReactionResult result{};
        result.ok = true;
        result.conversation_id = conversation_id;
        result.message_id = message_id;
        result.reactions = std::move(reactions);
        co_return result;
    }

    auto get_message_reactions(i64 message_id) -> asio::awaitable<std::vector<MessageReaction>>
    {
        auto conn_h = co_await acquire_connection();

        mysql::results r;
        co_await conn_h->async_execute(
            mysql::with_params(
                "SELECT mr.id, mr.message_id, mr.user_id, mr.reaction_type, u.display_name "
                "FROM message_reactions mr "
                "JOIN users u ON u.id = mr.user_id "
                "WHERE mr.message_id = {} "
                "ORDER BY mr.id ASC",
                message_id),
            r,
            asio::use_awaitable
        );

        std::vector<MessageReaction> reactions;
        reactions.reserve(r.rows().size());
        for(auto const& row : r.rows()) {
            MessageReaction reaction{};
            reaction.id = row.at(0).as_int64();
            reaction.message_id = row.at(1).as_int64();
            reaction.user_id = row.at(2).as_int64();
            reaction.reaction_type = row.at(3).as_string();
            reaction.display_name = row.at(4).as_string();
            reactions.push_back(std::move(reaction));
        }
        co_return reactions;
    }
} // namespace database
