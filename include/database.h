#pragma once

#include <pqxx/pqxx>

#include <chrono>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

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

    /// \brief 会话基础信息，用于会话列表展示。
    struct ConversationInfo
    {
        /// \brief 会话 ID。
        i64 id{};
        /// \brief 会话类型：GROUP / SINGLE。
        std::string type{};
        /// \brief 对当前用户的显示标题。
        std::string title{};
        /// \brief 当前会话最新消息的 seq，若尚无消息则为 0。
        i64 last_seq{};
        /// \brief 当前会话最新消息的服务器时间戳（毫秒），若尚无消息则为 0。
        i64 last_server_time_ms{};
    };

    /// \brief 已持久化消息的简要信息。
    struct StoredMessage
    {
        /// \brief 所属会话 ID。
        i64 conversation_id{};
        /// \brief 消息主键 ID，作为 serverMsgId。
        i64 id{};
        /// \brief 会话内递增序号。
        i64 seq{};
        /// \brief 服务器时间戳（毫秒）。
        i64 server_time_ms{};
    };

    /// \brief 已加载消息的完整信息，用于构建历史消息响应。
    struct LoadedMessage
    {
        /// \brief 消息主键 ID。
        i64 id{};
        /// \brief 所属会话 ID。
        i64 conversation_id{};
        /// \brief 发送者用户 ID。
        i64 sender_id{};
        /// \brief 会话内递增序号。
        i64 seq{};
        /// \brief 消息类型，例如 TEXT。
        std::string msg_type{};
        /// \brief 消息内容。
        std::string content{};
        /// \brief 服务器时间戳（毫秒）。
        i64 server_time_ms{};
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

        // 注册成功后，尝试自动与账号 "kkkzbh" 建立单聊会话。
        auto const friend_query =
            "SELECT id FROM users WHERE account = " + tx.quote(std::string{ "kkkzbh" }) + " "
            "LIMIT 1";

        auto friend_rows = tx.exec(friend_query);
        if(!friend_rows.empty()) {
            auto const friend_id = friend_rows[0][0].as<i64>();
            if(friend_id != user_id) {
                // 在同一事务中确保存在一个 SINGLE 会话。
                auto const conv_check =
                    "SELECT c.id "
                    "FROM conversations c "
                    "JOIN conversation_members m1 ON m1.conversation_id = c.id AND m1.user_id = "
                    + tx.quote(user_id) + " "
                    "JOIN conversation_members m2 ON m2.conversation_id = c.id AND m2.user_id = "
                    + tx.quote(friend_id) + " "
                    "WHERE c.type = " + tx.quote("SINGLE") + " "
                    "LIMIT 1";

                auto conv_rows = tx.exec(conv_check);
                if(conv_rows.empty()) {
                    auto const insert_conv =
                        "INSERT INTO conversations (type, name, owner_user_id) "
                        "VALUES (" + tx.quote("SINGLE") + ", '', " + tx.quote(user_id) + ") "
                        "RETURNING id";

                    auto conv_result = tx.exec(insert_conv);
                    if(!conv_result.empty()) {
                        auto const conv_id = conv_result[0][0].as<i64>();

                        auto const insert_members =
                            "INSERT INTO conversation_members (conversation_id, user_id, role) "
                            "VALUES (" + tx.quote(conv_id) + ", " + tx.quote(user_id)
                            + ", 'MEMBER'), ("
                            + tx.quote(conv_id) + ", " + tx.quote(friend_id)
                            + ", 'MEMBER') "
                            "ON CONFLICT DO NOTHING";

                        tx.exec(insert_members);
                    }
                }
            }
        }

        tx.commit();

        RegisterResult res{};
        res.ok = true;
        res.user.id = user_id;
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

    /// \brief 获取默认“世界”会话的 ID。
    /// \return conversations 表中 type='GROUP' 且 name='世界' 的会话 ID。
    /// \throws std::runtime_error 当会话不存在时抛出。
    auto inline get_world_conversation_id() -> i64
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT id FROM conversations "
            "WHERE type = " + tx.quote("GROUP") + " AND name = " + tx.quote("世界") + " "
            "LIMIT 1";

        auto rows = tx.exec(query);
        if(rows.empty()) {
            throw std::runtime_error{ "世界会话不存在" };
        }

        auto const id = rows[0][0].as<i64>();
        tx.commit();
        return id;
    }

    /// \brief 在指定会话中追加一条文本消息。
    /// \param conversation_id 目标会话 ID。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \return 已写入消息的简要信息。
    auto inline append_text_message(
        i64 conversation_id,
        i64 sender_id,
        std::string const& content
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
            + tx.quote(std::string{ "TEXT" }) + ", "
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
        return stored;
    }
    /// \brief 在“世界”会话中追加一条文本消息。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \return 已写入消息的简要信息。
    auto inline append_world_text_message(i64 sender_id, std::string const& content) -> StoredMessage
    {
        auto const conversation_id = get_world_conversation_id();
        return append_text_message(conversation_id, sender_id, content);
    }

    /// \brief 拉取指定会话的一批历史消息（向上翻旧消息）。
    /// \param conversation_id 会话 ID。
    /// \param before_seq 当为 0 或以下时表示从最新开始，否则拉取 seq 小于该值的消息。
    /// \param limit 最大返回条数，建议为正数。
    /// \return 按 seq 递增排序的消息列表。
    auto inline load_user_conversation_history(i64 conversation_id, i64 before_seq, i64 limit)
        -> std::vector<LoadedMessage>
    {
        if(limit <= 0) {
            limit = 50;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        std::string query =
            "SELECT id, conversation_id, sender_id, seq, msg_type, content, server_time_ms "
            "FROM messages WHERE conversation_id = " + tx.quote(conversation_id);

        if(before_seq > 0) {
            query += " AND seq < " + tx.quote(before_seq);
        }

        query += " ORDER BY seq DESC LIMIT " + tx.quote(limit);

        auto rows = tx.exec(query);

        std::vector<LoadedMessage> messages{};
        messages.reserve(rows.size());

        for(auto it = rows.rbegin(); it != rows.rend(); ++it) {
            auto const& row = *it;
            LoadedMessage msg{};
            msg.id = row[0].as<i64>();
            msg.conversation_id = row[1].as<i64>();
            msg.sender_id = row[2].as<i64>();
            msg.seq = row[3].as<i64>();
            msg.msg_type = row[4].as<std::string>();
            msg.content = row[5].as<std::string>();
            msg.server_time_ms = row[6].as<i64>();
            messages.push_back(std::move(msg));
        }

        tx.commit();
        return messages;
    }

    /// \brief 拉取指定会话中 seq 大于给定值的一批新消息（用于增量同步）。
    /// \param conversation_id 会话 ID。
    /// \param after_seq 仅返回 seq 大于该值的消息。
    /// \param limit 最大返回条数，建议为正数。
    /// \return 按 seq 递增排序的消息列表。
    auto inline load_user_conversation_since(i64 conversation_id, i64 after_seq, i64 limit)
        -> std::vector<LoadedMessage>
    {
        if(limit <= 0) {
            limit = 100;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        std::string query =
            "SELECT id, conversation_id, sender_id, seq, msg_type, content, server_time_ms "
            "FROM messages WHERE conversation_id = " + tx.quote(conversation_id);

        if(after_seq > 0) {
            query += " AND seq > " + tx.quote(after_seq);
        }

        query += " ORDER BY seq ASC LIMIT " + tx.quote(limit);

        auto rows = tx.exec(query);

        std::vector<LoadedMessage> messages{};
        messages.reserve(rows.size());

        for(auto const& row : rows) {
            LoadedMessage msg{};
            msg.id = row[0].as<i64>();
            msg.conversation_id = row[1].as<i64>();
            msg.sender_id = row[2].as<i64>();
            msg.seq = row[3].as<i64>();
            msg.msg_type = row[4].as<std::string>();
            msg.content = row[5].as<std::string>();
            msg.server_time_ms = row[6].as<i64>();
            messages.push_back(std::move(msg));
        }

        tx.commit();
        return messages;
    }

    /// \brief 拉取“世界”会话的一批历史消息。
    /// \param before_seq 当为 0 或以下时表示从最新开始，否则拉取 seq 小于该值的消息。
    /// \param limit 最大返回条数，建议为正数。
    /// \return 按 seq 递增排序的消息列表。
    auto inline load_world_history(i64 before_seq, i64 limit) -> std::vector<LoadedMessage>
    {
        auto const conversation_id = get_world_conversation_id();
        return load_user_conversation_history(conversation_id, before_seq, limit);
    }

    /// \brief 加载某个用户所属的全部会话（群聊 + 单聊）。
    /// \param user_id 当前用户 ID。
    /// \return 会话列表，包含类型和对当前用户的标题。
    auto inline load_user_conversations(i64 user_id) -> std::vector<ConversationInfo>
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT c.id, c.type, c.name "
            "FROM conversations c "
            "JOIN conversation_members cm ON cm.conversation_id = c.id "
            "WHERE cm.user_id = " + tx.quote(user_id) + " "
            "ORDER BY c.id ASC";

        auto rows = tx.exec(query);

        std::vector<ConversationInfo> result{};
        result.reserve(rows.size());

        for(auto const& row : rows) {
            ConversationInfo info{};
            info.id = row[0].as<i64>();
            info.type = row[1].as<std::string>();
            auto const stored_name = row[2].as<std::string>();

            if(info.type == "GROUP") {
                info.title = stored_name;
            } else if(info.type == "SINGLE") {
                auto const conv_id = info.id;

                auto const peer_query =
                    "SELECT u.display_name "
                    "FROM conversation_members cm "
                    "JOIN users u ON u.id = cm.user_id "
                    "WHERE cm.conversation_id = " + tx.quote(conv_id) + " "
                    "AND cm.user_id <> " + tx.quote(user_id) + " "
                    "LIMIT 1";

                auto peer_rows = tx.exec(peer_query);
                if(!peer_rows.empty()) {
                    info.title = peer_rows[0][0].as<std::string>();
                } else {
                    info.title = stored_name;
                }
            } else {
                info.title = stored_name;
            }

            // 加载该会话当前最新消息的 seq 和时间戳。
            {
                auto const msg_query =
                    "SELECT COALESCE(MAX(seq), 0), COALESCE(MAX(server_time_ms), 0) "
                    "FROM messages WHERE conversation_id = " + tx.quote(info.id);

                auto msg_rows = tx.exec(msg_query);
                if(!msg_rows.empty()) {
                    info.last_seq = msg_rows[0][0].as<i64>();
                    info.last_server_time_ms = msg_rows[0][1].as<i64>();
                }
            }

            result.push_back(std::move(info));
        }

        tx.commit();
        return result;
    }
} // namespace database
