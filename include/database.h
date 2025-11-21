#pragma once

#include <pqxx/pqxx>

#include <chrono>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>

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

    /// \brief 更新昵称结果。
    struct UpdateDisplayNameResult
    {
        /// \brief 是否更新成功。
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

    /// \brief 好友基础信息，用于通讯录联系人列表展示。
    struct FriendInfo
    {
        /// \brief 好友用户 ID。
        i64 id{};
        /// \brief 好友账号。
        std::string account{};
        /// \brief 好友昵称。
        std::string display_name{};
    };

    /// \brief 好友申请信息，用于“新的朋友”列表展示。
    struct FriendRequestInfo
    {
        /// \brief 申请 ID。
        i64 id{};
        /// \brief 申请发起者用户 ID。
        i64 from_user_id{};
        /// \brief 申请发起者账号。
        std::string account{};
        /// \brief 申请发起者昵称。
        std::string display_name{};
        /// \brief 当前状态：PENDING / ACCEPTED 等。
        std::string status{};
        /// \brief 打招呼信息或来源文案。
        std::string hello_msg{};
    };

    /// \brief 按账号搜索好友的结果。
    struct SearchFriendResult
    {
        /// \brief 是否查询成功。
        bool ok{};
        /// \brief 错误码（例如 NOT_FOUND）。
        std::string error_code{};
        /// \brief 错误信息。
        std::string error_msg{};
        /// \brief 是否找到对应账号。
        bool found{};
        /// \brief 是否为当前用户自己。
        bool is_self{};
        /// \brief 是否已经是好友。
        bool is_friend{};
        /// \brief 当 found 为 true 时的用户信息。
        UserInfo user{};
    };

    /// \brief 创建好友申请的结果。
    struct FriendRequestResult
    {
        /// \brief 是否操作成功。
        bool ok{};
        /// \brief 错误码，例如 ALREADY_FRIEND。
        std::string error_code{};
        /// \brief 错误信息。
        std::string error_msg{};
        /// \brief 成功时的申请 ID。
        i64 request_id{};
    };

    /// \brief 同意好友申请的结果。
    struct AcceptFriendRequestResult
    {
        /// \brief 是否操作成功。
        bool ok{};
        /// \brief 错误码。
        std::string error_code{};
        /// \brief 错误信息。
        std::string error_msg{};
        /// \brief 新增好友的基础信息。
        UserInfo friend_user{};
        /// \brief 为这对好友建立 / 复用的单聊会话 ID（如无则为 0）。
        i64 conversation_id{};
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

    /// \brief 会话成员信息。
    struct MemberInfo
    {
        /// \brief 成员用户 ID。
        i64 user_id{};
        /// \brief 显示昵称。
        std::string display_name{};
        /// \brief 角色：OWNER / MEMBER。
        std::string role{};
        /// \brief 禁言截止毫秒时间戳，0 表示未禁言。
        i64 muted_until_ms{};
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
        /// \brief 消息类型。
        std::string msg_type{};
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
        /// \brief 发送者显示昵称。
        std::string sender_display_name{};
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

        tx.commit();

        RegisterResult res{};
        res.ok = true;
        res.user.id = user_id;
        res.user.account = row[1].as<std::string>();
        res.user.display_name = row[2].as<std::string>();
        return res;
    }

    /// \brief 更新指定用户的显示昵称。
    /// \param user_id 目标用户 ID。
    /// \param new_name 新的昵称。
    /// \return 更新结果，包含错误信息或最新的用户信息。
    auto inline update_display_name(i64 user_id, std::string const& new_name)
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

    /// \brief 判断两个用户是否已经互为好友。
    /// \param user_id 当前用户 ID。
    /// \param peer_id 待检查的对端用户 ID。
    /// \return 当 friends 表中存在 (user_id, peer_id) 记录时返回 true。
    auto inline is_friend(i64 user_id, i64 peer_id) -> bool
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

    /// \brief 确保给定两个用户之间存在一个 SINGLE 会话，若不存在则创建。
    /// \param user1 第一个用户 ID。
    /// \param user2 第二个用户 ID。
    /// \return 单聊会话 ID。
    auto inline get_or_create_single_conversation(i64 user1, i64 user2) -> i64
    {
        if(user1 <= 0 || user2 <= 0 || user1 == user2) {
            throw std::runtime_error{ "无效的单聊会话参与者" };
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        // 为保证唯一性，将用户 ID 排序后写入 single_conversations。
        auto const a = std::min(user1, user2);
        auto const b = std::max(user1, user2);

        // 1. 尝试在辅助表中直接查已有单聊会话。
        auto const check_query =
            "SELECT conversation_id "
            "FROM single_conversations "
            "WHERE user1_id = " + tx.quote(a) + " AND user2_id = " + tx.quote(b) + " "
            "LIMIT 1";

        auto rows = tx.exec(check_query);
        if(!rows.empty()) {
            auto const conv_id = rows[0][0].as<i64>();
            tx.commit();
            return conv_id;
        }

        // 2. 不存在时创建新的 SINGLE 会话。
        auto const insert_conv =
            "INSERT INTO conversations (type, name, owner_user_id) "
            "VALUES (" + tx.quote(std::string{ "SINGLE" }) + ", '', " + tx.quote(user1) + ") "
            "RETURNING id";

        auto conv_rows = tx.exec(insert_conv);
        if(conv_rows.empty()) {
            throw std::runtime_error{ "创建单聊会话失败" };
        }
        auto const conv_id = conv_rows[0][0].as<i64>();

        // 3. 建立会话成员关系。
        auto const insert_members =
            "INSERT INTO conversation_members (conversation_id, user_id, role) "
            "VALUES (" + tx.quote(conv_id) + ", " + tx.quote(user1) + ", 'MEMBER'), ("
            + tx.quote(conv_id) + ", " + tx.quote(user2) + ", 'MEMBER') "
            "ON CONFLICT DO NOTHING";

        tx.exec(insert_members);

        // 4. 在 single_conversations 中记录该对用户，利用唯一约束防止并发重复创建。
        auto const insert_pair =
            "INSERT INTO single_conversations (user1_id, user2_id, conversation_id) "
            "VALUES (" + tx.quote(a) + ", " + tx.quote(b) + ", " + tx.quote(conv_id) + ") "
            "ON CONFLICT (user1_id, user2_id) DO UPDATE "
            "SET conversation_id = EXCLUDED.conversation_id "
            "RETURNING conversation_id";

        auto pair_rows = tx.exec(insert_pair);
        if(pair_rows.empty()) {
            throw std::runtime_error{ "记录单聊会话失败" };
        }

        auto const final_conv_id = pair_rows[0][0].as<i64>();
        tx.commit();
        return final_conv_id;
    }

    /// \brief 在指定会话中追加一条文本消息。
    /// \param conversation_id 目标会话 ID。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \return 已写入消息的简要信息。
    auto inline append_text_message(
        i64 conversation_id,
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type = std::string{ "TEXT" }
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

    /// \brief 创建群聊会话，返回会话 ID（不写入系统消息）。
    /// \param creator_id 群主用户 ID。
    /// \param member_ids 需要加入群聊的用户 ID 列表（不包含自己，函数内部会去重并加入 creator）。
    /// \param name 群名称，空字符串时按默认规则生成。
    /// \return 新建的群聊会话 ID。
    auto inline create_group_conversation(
        i64 creator_id,
        std::vector<i64> member_ids,
        std::string name
    ) -> i64
    {
        if(creator_id <= 0) {
            throw std::runtime_error{ "无效的群主 ID" };
        }

        // 去重并移除 creator
        std::sort(member_ids.begin(), member_ids.end());
        member_ids.erase(std::unique(member_ids.begin(), member_ids.end()), member_ids.end());
        member_ids.erase(
            std::remove(member_ids.begin(), member_ids.end(), creator_id),
            member_ids.end()
        );

        if(member_ids.size() < 2) {
            throw std::runtime_error{ "群成员不足（至少需要群主 + 2 位好友）" };
        }

        // 默认群名：取群主 + 前若干成员昵称（顺序为群主优先，再按 member_ids 排序），超过加“等”
        auto conn = make_connection();
        pqxx::work tx{ conn };

        if(name.empty()) {
            std::vector<std::string> picked_names;
            picked_names.reserve(3);

            // 组装用于命名的顺序：群主 + 剩余成员
            std::vector<i64> name_order;
            name_order.reserve(member_ids.size() + 1);
            name_order.push_back(creator_id);
            name_order.insert(name_order.end(), member_ids.begin(), member_ids.end());

            for(size_t i = 0; i < name_order.size() && picked_names.size() < 3; ++i) {
                auto const uid = name_order[i];
                auto const q =
                    "SELECT display_name FROM users WHERE id = " + tx.quote(uid) + " LIMIT 1";
                auto rows = tx.exec(q);
                if(!rows.empty()) {
                    picked_names.push_back(rows[0][0].as<std::string>());
                }
            }

            std::string joined;
            for(size_t i = 0; i < picked_names.size(); ++i) {
                if(i > 0) joined += "、";
                joined += picked_names[i];
            }
            if(name_order.size() > 3) {
                joined += "等";
            }
            if(joined.empty()) {
                joined = "群聊";
            }
            name = joined;
        }

        // 创建 conversations 记录
        auto const insert_conv =
            "INSERT INTO conversations (type, name, owner_user_id) "
            "VALUES (" + tx.quote(std::string{ "GROUP" }) + ", "
            + tx.quote(name) + ", "
            + tx.quote(creator_id) + ") "
            "RETURNING id";

        auto conv_rows = tx.exec(insert_conv);
        if(conv_rows.empty()) {
            throw std::runtime_error{ "创建群聊失败" };
        }
        auto const conv_id = conv_rows[0][0].as<i64>();

        // 插入成员：群主=OWNER，其余=MEMBER
        std::string values;
        values += "(" + tx.quote(conv_id) + ", " + tx.quote(creator_id) + ", 'OWNER')";
        for(auto const uid : member_ids) {
            values += ", (" + tx.quote(conv_id) + ", " + tx.quote(uid) + ", 'MEMBER')";
        }

        auto const insert_members =
            "INSERT INTO conversation_members (conversation_id, user_id, role) VALUES "
            + values
            + " ON CONFLICT DO NOTHING";

        tx.exec(insert_members);

        tx.commit();
        return conv_id;
    }
    /// \brief 在“世界”会话中追加一条文本消息。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \param msg_type 消息类型，默认 TEXT，可选 SYSTEM。
    /// \return 已写入消息的简要信息。
    auto inline append_world_text_message(
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type = std::string{ "TEXT" }
    ) -> StoredMessage
    {
        auto const conversation_id = get_world_conversation_id();
        return append_text_message(conversation_id, sender_id, content, msg_type);
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

    /// \brief 加载某个用户的好友列表。
    /// \param user_id 当前用户 ID。
    /// \return 好友信息列表。
    auto inline load_user_friends(i64 user_id) -> std::vector<FriendInfo>
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

    /// \brief 按账号搜索用户，并判断是否为当前用户或已是好友。
    /// \param current_user_id 当前登录用户 ID。
    /// \param account 要搜索的账号。
    /// \return 查询结果及好友关系标记。
    auto inline search_friend_by_account(i64 current_user_id, std::string const& account)
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

    /// \brief 创建一条好友申请，若已是好友或存在未处理申请则返回对应错误。
    /// \param from_user_id 申请发起者。
    /// \param to_user_id 申请接收者。
    /// \param source 申请来源说明，例如 search_account。
    /// \param hello_msg 打招呼信息。
    /// \return 操作结果以及申请 ID。
    auto inline create_friend_request(
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

    /// \brief 加载“别人加我”的好友申请列表。
    /// \param user_id 当前用户 ID。
    /// \return 好友申请列表。
    auto inline load_incoming_friend_requests(i64 user_id) -> std::vector<FriendRequestInfo>
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

    /// \brief 同意一条好友申请，并在必要时建立好友关系及单聊会话。
    /// \param request_id 好友申请 ID。
    /// \param current_user_id 当前登录用户 ID（必须为申请接收者）。
    /// \return 操作结果及新好友信息。
    auto inline accept_friend_request(i64 request_id, i64 current_user_id)
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

    /// \brief 查询会话成员信息。
    auto inline get_conversation_member(i64 conversation_id, i64 user_id)
        -> std::optional<MemberInfo>
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT cm.role, cm.muted_until_ms, u.display_name "
            "FROM conversation_members cm "
            "JOIN users u ON u.id = cm.user_id "
            "WHERE cm.conversation_id = " + tx.quote(conversation_id) + " "
            "AND cm.user_id = " + tx.quote(user_id) + " "
            "LIMIT 1";

        auto rows = tx.exec(query);
        tx.commit();

        if(rows.empty()) {
            return std::nullopt;
        }

        MemberInfo info{};
        info.user_id = user_id;
        info.role = rows[0][0].as<std::string>();
        info.muted_until_ms = rows[0][1].as<i64>();
        info.display_name = rows[0][2].as<std::string>();
        return info;
    }

    /// \brief 更新会话成员的禁言截止时间（毫秒时间戳，0 表示解禁）。
    auto inline set_member_mute_until(
        i64 conversation_id,
        i64 user_id,
        i64 muted_until_ms
    ) -> void
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const update =
            "UPDATE conversation_members "
            "SET muted_until_ms = " + tx.quote(muted_until_ms) + " "
            "WHERE conversation_id = " + tx.quote(conversation_id) + " "
            "AND user_id = " + tx.quote(user_id);

        tx.exec(update);
        tx.commit();
    }

    /// \brief 加载指定会话的全部成员（含角色与禁言状态）。
    auto inline load_conversation_members(i64 conversation_id) -> std::vector<MemberInfo>
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "SELECT cm.user_id, cm.role, cm.muted_until_ms, u.display_name "
            "FROM conversation_members cm "
            "JOIN users u ON u.id = cm.user_id "
            "WHERE cm.conversation_id = " + tx.quote(conversation_id) + " "
            "ORDER BY cm.user_id ASC";

        auto rows = tx.exec(query);

        std::vector<MemberInfo> members;
        members.reserve(rows.size());

        for(auto const& row : rows) {
            MemberInfo info{};
            info.user_id = row[0].as<i64>();
            info.role = row[1].as<std::string>();
            info.muted_until_ms = row[2].as<i64>();
            info.display_name = row[3].as<std::string>();
            members.push_back(std::move(info));
        }

        tx.commit();
        return members;
    }

    /// \brief 移除指定会话中的一名成员（用于主动退群等场景）。
    auto inline remove_conversation_member(i64 conversation_id, i64 user_id) -> void
    {
        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const query =
            "DELETE FROM conversation_members "
            "WHERE conversation_id = " + tx.quote(conversation_id) + " "
            "AND user_id = " + tx.quote(user_id);

        tx.exec(query);
        tx.commit();
    }

    /// \brief 解散会话：删除所有消息与成员关系，并删除会话本身。
    /// \details 当前用于群聊解散逻辑，调用者负责业务校验。
    auto inline dissolve_conversation(i64 conversation_id) -> void
    {
        if(conversation_id <= 0) {
            return;
        }

        auto conn = make_connection();
        pqxx::work tx{ conn };

        auto const delete_messages =
            "DELETE FROM messages WHERE conversation_id = " + tx.quote(conversation_id);
        tx.exec(delete_messages);

        auto const delete_members =
            "DELETE FROM conversation_members WHERE conversation_id = " + tx.quote(conversation_id);
        tx.exec(delete_members);

        auto const delete_conv =
            "DELETE FROM conversations WHERE id = " + tx.quote(conversation_id);
        tx.exec(delete_conv);

        tx.commit();
    }
} // namespace database
