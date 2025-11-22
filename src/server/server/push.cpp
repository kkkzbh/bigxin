#include <session.h>
#include <server.h>

#include <database.h>
#include <protocol.h>

#include <nlohmann/json.hpp>

#include <unordered_set>
#include <vector>

using nlohmann::json;

auto Server::send_friend_request_list_to(i64 target_user_id) -> void
{
    if(target_user_id <= 0) {
        return;
    }

    try {
        auto const requests = database::load_incoming_friend_requests(target_user_id);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& r : requests) {
            json obj;
            obj["requestId"] = std::to_string(r.id);
            obj["fromUserId"] = std::to_string(r.from_user_id);
            obj["account"] = r.account;
            obj["displayName"] = Session::normalize_whitespace(r.display_name);
            obj["status"] = r.status;
            obj["helloMsg"] = r.hello_msg;
            items.push_back(std::move(obj));
        }

        resp["requests"] = std::move(items);
        auto const line =
            protocol::make_line("FRIEND_REQ_LIST_RESP", resp.dump());

        for_user_sessions(target_user_id, [&line](std::shared_ptr<Session> const& s) {
            if(s->authenticated_) {
                s->send_text(line);
            }
        });
    } catch(std::exception const&) {
        // 失败时静默忽略，等待客户端下次主动拉取。
    }
}

auto Server::send_friend_list_to(i64 target_user_id) -> void
{
    if(target_user_id <= 0) {
        return;
    }

    try {
        auto const friends = database::load_user_friends(target_user_id);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& f : friends) {
            json u;
            u["userId"] = std::to_string(f.id);
            u["account"] = f.account;
            u["displayName"] = Session::normalize_whitespace(f.display_name);
            u["region"] = "";
            u["signature"] = "";
            items.push_back(std::move(u));
        }

        resp["friends"] = std::move(items);
        auto const line = protocol::make_line("FRIEND_LIST_RESP", resp.dump());

        for_user_sessions(target_user_id, [&line](std::shared_ptr<Session> const& s) {
            if(s->is_authenticated()) {
                s->send_text(line);
            }
        });
    } catch(std::exception const&) {
        // 忽略推送失败。
    }
}

auto Server::send_conv_list_to(i64 target_user_id) -> void
{
    if(target_user_id <= 0) {
        return;
    }

    try {
        auto const conversations = database::load_user_conversations(target_user_id);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& conv : conversations) {
            json c;
            c["conversationId"] = std::to_string(conv.id);
            c["conversationType"] = conv.type;
            c["title"] = conv.title;
            c["lastSeq"] = conv.last_seq;
            c["lastServerTimeMs"] = conv.last_server_time_ms;
            items.push_back(std::move(c));
        }

        resp["conversations"] = std::move(items);
        auto const line = protocol::make_line("CONV_LIST_RESP", resp.dump());

        for_user_sessions(target_user_id, [&line](std::shared_ptr<Session> const& s) {
            if(s->is_authenticated()) {
                s->send_text(line);
            }
        });
    } catch(std::exception const&) {
        // 忽略推送失败。
    }
}

auto Server::send_conv_members(i64 conversation_id, i64 only_user_id) -> void
{
    if(conversation_id <= 0) {
        return;
    }

    std::vector<database::MemberInfo> members;
    try {
        members = database::load_conversation_members(conversation_id);
    } catch(std::exception const&) {
        return;
    }

    json resp;
    resp["ok"] = true;
    resp["conversationId"] = std::to_string(conversation_id);

    json arr = json::array();
    for(auto const& m : members) {
        json obj;
        obj["userId"] = std::to_string(m.user_id);
        obj["displayName"] = m.display_name;
        obj["role"] = m.role;
        obj["mutedUntilMs"] = m.muted_until_ms;
        arr.push_back(std::move(obj));
    }
    resp["members"] = std::move(arr);

    auto const line = protocol::make_line("CONV_MEMBERS_RESP", resp.dump());
    std::unordered_set<i64> member_ids;
    member_ids.reserve(members.size());
    for(auto const& m : members) {
        member_ids.insert(m.user_id);
    }

    if(only_user_id > 0 && !member_ids.contains(only_user_id)) {
        return; // 指定用户不在会话成员内
    }

    auto const send_to = [&line, this](i64 uid) {
        for_user_sessions(uid, [&line](std::shared_ptr<Session> const& session) {
            if(session->is_authenticated()) {
                session->send_text(line);
            }
        });
    };

    if(only_user_id > 0) {
        send_to(only_user_id);
        return;
    }

    for(auto const uid : member_ids) {
        send_to(uid);
    }
}
