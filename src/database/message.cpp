#include <database/message.h>
#include <database/connection.h>
#include <database/conversation.h>
#include <pqxx/pqxx>
#include <utility.h>
#include <chrono>
#include <stdexcept>

namespace database
{
    auto append_text_message(
        i64 conversation_id,
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type
    ) -> StoredMessage
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const seq_query =
            "SELECT COALESCE(MAX(seq), 0) + 1 FROM messages "
            "WHERE conversation_id = " + tx.quote(conversation_id);

        auto seq_rows = tx.exec(seq_query);
        auto const seq = seq_rows[0][0].as<i64>();

        auto const now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()
                            )
                                .count();

        auto const insert =
            "INSERT INTO messages (conversation_id, sender_id, seq, msg_type, content, server_time_ms) "
            "VALUES (" + tx.quote(conversation_id) + ", "
            + tx.quote(sender_id) + ", "
            + tx.quote(seq) + ", "
            + tx.quote(msg_type) + ", "
            + tx.quote(content) + ", "
            + tx.quote(now_ms) + ") "
            "RETURNING id";

        auto result = tx.exec(insert);
        if(result.empty()) {
            throw std::runtime_error{ "插入消息失败" };
        }

        auto const id = result[0][0].as<i64>();
        tx.commit();

        StoredMessage stored{};
        stored.conversation_id = conversation_id;
        stored.id = id;
        stored.seq = seq;
        stored.server_time_ms = now_ms;
        stored.msg_type = msg_type;
        return stored;
    }

    auto append_world_text_message(
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type
    ) -> StoredMessage
    {
        auto const conversation_id = get_world_conversation_id();
        return append_text_message(conversation_id, sender_id, content, msg_type);
    }

    auto load_user_conversation_history(i64 conversation_id, i64 before_seq, i64 limit)
        -> std::vector<LoadedMessage>
    {
        if(limit <= 0) {
            limit = 50;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        std::string query =
            "SELECT m.id, m.conversation_id, m.sender_id, u.display_name, m.seq, m.msg_type, m.content, m.server_time_ms "
            "FROM messages m "
            "JOIN users u ON u.id = m.sender_id "
            "WHERE m.conversation_id = " + tx.quote(conversation_id);

        if(before_seq > 0) {
            query += " AND seq < " + tx.quote(before_seq);
        }

        query += " ORDER BY m.seq DESC LIMIT " + tx.quote(limit);

        auto rows = tx.exec(query);

        std::vector<LoadedMessage> messages{};
        messages.reserve(rows.size());

        for(auto it = rows.rbegin(); it != rows.rend(); ++it) {
            auto const& row = *it;
            LoadedMessage msg{};
            msg.id = row[0].as<i64>();
            msg.conversation_id = row[1].as<i64>();
            msg.sender_id = row[2].as<i64>();
            msg.sender_display_name = row[3].as<std::string>();
            msg.seq = row[4].as<i64>();
            msg.msg_type = row[5].as<std::string>();
            msg.content = row[6].as<std::string>();
            msg.server_time_ms = row[7].as<i64>();
            if(msg.msg_type == "SYSTEM") {
                msg.sender_id = 0;
                msg.sender_display_name.clear();
            }
            messages.push_back(std::move(msg));
        }

        tx.commit();
        return messages;
    }

    auto load_user_conversation_since(i64 conversation_id, i64 after_seq, i64 limit)
        -> std::vector<LoadedMessage>
    {
        if(limit <= 0) {
            limit = 100;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        std::string query =
            "SELECT m.id, m.conversation_id, m.sender_id, u.display_name, m.seq, m.msg_type, m.content, m.server_time_ms "
            "FROM messages m "
            "JOIN users u ON u.id = m.sender_id "
            "WHERE m.conversation_id = " + tx.quote(conversation_id);

        if(after_seq > 0) {
            query += " AND seq > " + tx.quote(after_seq);
        }

        query += " ORDER BY m.seq ASC LIMIT " + tx.quote(limit);

        auto rows = tx.exec(query);

        std::vector<LoadedMessage> messages{};
        messages.reserve(rows.size());

        for(auto const& row : rows) {
            LoadedMessage msg{};
            msg.id = row[0].as<i64>();
            msg.conversation_id = row[1].as<i64>();
            msg.sender_id = row[2].as<i64>();
            msg.sender_display_name = row[3].as<std::string>();
            msg.seq = row[4].as<i64>();
            msg.msg_type = row[5].as<std::string>();
            msg.content = row[6].as<std::string>();
            msg.server_time_ms = row[7].as<i64>();
            if(msg.msg_type == "SYSTEM") {
                msg.sender_id = 0;
                msg.sender_display_name.clear();
            }
            messages.push_back(std::move(msg));
        }

        tx.commit();
        return messages;
    }

    auto load_world_history(i64 before_seq, i64 limit) -> std::vector<LoadedMessage>
    {
        auto const conversation_id = get_world_conversation_id();
        return load_user_conversation_history(conversation_id, before_seq, limit);
    }
} // namespace database
