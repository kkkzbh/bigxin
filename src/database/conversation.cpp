#include <database/conversation.h>
#include <database/connection.h>
#include <pqxx/pqxx>
#include <utility.h>
#include <stdexcept>
#include <algorithm>

namespace database
{
    auto get_world_conversation_id() -> i64
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

    auto get_or_create_single_conversation(i64 user1, i64 user2) -> i64
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

    auto create_group_conversation(
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

        // 默认群名：取群主 + 前若干成员昵称（顺序为群主优先，再按 member_ids 排序），超过加"等"
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

    auto load_user_conversations(i64 user_id) -> std::vector<ConversationInfo>
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

    auto get_conversation_member(i64 conversation_id, i64 user_id)
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

    auto set_member_mute_until(
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

    auto load_conversation_members(i64 conversation_id) -> std::vector<MemberInfo>
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

    auto remove_conversation_member(i64 conversation_id, i64 user_id) -> void
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

    auto dissolve_conversation(i64 conversation_id) -> void
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
