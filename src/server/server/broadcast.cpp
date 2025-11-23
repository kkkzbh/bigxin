#include <session.h>
#include <server.h>

#include <database.h>
#include <protocol.h>
#include <pqxx/pqxx>

#include <nlohmann/json.hpp>
#include <asioexec/use_sender.hpp>
#include <exec/task.hpp>

#include <algorithm>
#include <print>
#include <unordered_set>
#include <vector>

using nlohmann::json;
using asio::use_awaitable;

auto Server::run() -> asio::awaitable<void>
{
    try {
        while(true) {
            auto socket = co_await acceptor_.async_accept(use_awaitable);

            auto const endpoint = socket.remote_endpoint();
            auto const addr = endpoint.address().to_string();
            auto const port = endpoint.port();
            std::println("new connection from: {}:{}", addr, port);

            auto session = std::make_shared<Session>(std::move(socket), *this);
            sessions_.push_back(session);

            asio::co_spawn(
                acceptor_.get_executor(),
                [this, session]() -> asio::awaitable<void> {
                    co_await session->run();
                    remove_session(session.get());
                },
                asio::detached
            );
        }
    } catch(std::exception const& ex) {
        std::println("server accept loop error: {}", ex.what());
    }
}

auto Server::remove_session(Session* ptr) -> void
{
    auto it = std::remove_if(
        sessions_.begin(),
        sessions_.end(),
        [ptr](std::shared_ptr<Session> const& s) { return s.get() == ptr; }
    );
    sessions_.erase(it, sessions_.end());

    for(auto map_it = sessions_by_user_.begin(); map_it != sessions_by_user_.end(); ) {
        auto locked = map_it->second.lock();
        if(!locked || locked.get() == ptr) {
            map_it = sessions_by_user_.erase(map_it);
        } else {
            ++map_it;
        }
    }
}

auto Server::index_authenticated_session(std::shared_ptr<Session> const& session) -> void
{
    if(!session || !session->is_authenticated()) {
        return;
    }

    auto const uid = session->user_id();
    auto range = sessions_by_user_.equal_range(uid);
    for(auto it = range.first; it != range.second; ) {
        auto existing = it->second.lock();
        if(!existing || existing.get() == session.get()) {
            it = sessions_by_user_.erase(it);
        } else {
            ++it;
        }
    }

    sessions_by_user_.emplace(uid, session);
}

auto Server::broadcast_system_message(
    i64 conversation_id,
    database::StoredMessage const& stored,
    std::string const& content
) -> void
{
    json push;
    push["conversationId"] = std::to_string(conversation_id);
    push["conversationType"] = "GROUP";
    push["serverMsgId"] = std::to_string(stored.id);
    push["senderId"] = "0";
    push["senderDisplayName"] = "";
    push["msgType"] = stored.msg_type.empty() ? "TEXT" : stored.msg_type;
    push["serverTimeMs"] = stored.server_time_ms;
    push["seq"] = stored.seq;
    push["content"] = content;

    auto const line = protocol::make_line("MSG_PUSH", push.dump());

    std::vector<i64> member_ids;
    try {
        auto conn = database::make_connection();
        pqxx::work tx{ conn };
        auto const query =
            "SELECT user_id FROM conversation_members WHERE conversation_id = "
            + tx.quote(conversation_id);
        auto rows = tx.exec(query);
        member_ids.reserve(rows.size());
        for(auto const& row : rows) {
            member_ids.push_back(row[0].as<i64>());
        }
        tx.commit();
    } catch(std::exception const&) {
        member_ids.clear(); // broadcast to all authenticated if query failed.
    }

    auto const send_line = [&line](std::shared_ptr<Session> const& session) {
        if(session->is_authenticated()) {
            session->send_text(line);
        }
    };

    if(member_ids.empty()) {
        for_all_authenticated_sessions(send_line);
        return;
    }

    std::unordered_set<i64> member_set{ member_ids.begin(), member_ids.end() };
    for(auto const uid : member_set) {
        for_user_sessions(uid, send_line);
    }
}

auto Server::broadcast_world_message(
    database::StoredMessage const& stored,
    i64 sender_id,
    std::string const& content
) -> void
{
    json push;
    push["conversationId"] = std::to_string(stored.conversation_id);
    // 会话类型用于前端展示，不影响路由逻辑。
    std::string conv_type{ "GROUP" };
    {
        try {
            auto conn = database::make_connection();
            pqxx::work tx{ conn };
            auto const query =
                "SELECT type FROM conversations "
                "WHERE id = " + tx.quote(stored.conversation_id) + " LIMIT 1";
            auto rows = tx.exec(query);
            if(!rows.empty()) {
                conv_type = rows[0][0].as<std::string>();
            }
            tx.commit();
        } catch(std::exception const&) {
            // 保底使用 GROUP。
        }
    }
    std::string sender_name;
    if(sender_id > 0 && stored.msg_type != "SYSTEM") {
        try {
            auto conn = database::make_connection();
            pqxx::work tx{ conn };
            auto const query =
                "SELECT display_name FROM users WHERE id = " + tx.quote(sender_id)
                + " LIMIT 1";
            auto rows = tx.exec(query);
            if(!rows.empty()) {
                sender_name = rows[0][0].as<std::string>();
            }
            tx.commit();
        } catch(std::exception const&) {
            // 忽略查找失败。
        }
    }
    push["conversationType"] = conv_type;
    push["serverMsgId"] = std::to_string(stored.id);
    push["senderId"] = (stored.msg_type == "SYSTEM") ? std::string{ "0" }
                                                     : std::to_string(sender_id);
    push["senderDisplayName"] = sender_name;
    push["msgType"] = stored.msg_type.empty() ? "TEXT" : stored.msg_type;
    push["serverTimeMs"] = stored.server_time_ms;
    push["seq"] = stored.seq;
    push["content"] = content;

    auto const line = protocol::make_line("MSG_PUSH", push.dump());

    // 仅向会话成员中在线的用户广播。
    std::vector<i64> member_ids;
    try {
        auto conn = database::make_connection();
        pqxx::work tx{ conn };
        auto const query =
            "SELECT user_id FROM conversation_members WHERE conversation_id = "
            + tx.quote(stored.conversation_id);
        auto rows = tx.exec(query);
        member_ids.reserve(rows.size());
        for(auto const& row : rows) {
            member_ids.push_back(row[0].as<i64>());
        }
        tx.commit();
    } catch(std::exception const&) {
        // 如果查询失败，退化为广播给所有在线用户。
        member_ids.clear();
    }

    auto const send_line = [&line](std::shared_ptr<Session> const& session) {
        if(session->is_authenticated()) {
            session->send_text(line);
        }
    };

    if(member_ids.empty()) {
        for_all_authenticated_sessions(send_line);
        return;
    }

    std::unordered_set<i64> member_set{ member_ids.begin(), member_ids.end() };
    for(auto const uid : member_set) {
        for_user_sessions(uid, send_line);
    }
}
