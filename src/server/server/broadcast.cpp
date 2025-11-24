#include <session.h>
#include <server.h>

#include <database.h>
#include <protocol.h>
#include <pqxx/pqxx>

#include <nlohmann/json.hpp>
#include <asioexec/use_sender.hpp>
#include <exec/task.hpp>

#include <optional>
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
            
            // 使用 strand 保证线程安全
            asio::co_spawn (
                strand_,
                [this, session]() -> asio::awaitable<void> {
                    sessions_[session.get()] = session;
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
    if(!ptr) {
        return;
    }

    // O(1) 从主 map 中删除
    auto it = sessions_.find(ptr);
    if(it == sessions_.end()) {
        return;
    }

    // 如果已认证,记录其 user_id 以便高效清理 sessions_by_user_
    std::optional<i64> uid;
    if(it->second && it->second->is_authenticated()) {
        uid = it->second->user_id();
    }

    sessions_.erase(it);

    // 仅在已认证的情况下清理 sessions_by_user_
    // 优化: 仅遍历该用户的会话 O(k), k 是该用户的设备数
    if(uid.has_value()) {
        auto range = sessions_by_user_.equal_range(uid.value());
        for(auto map_it = range.first; map_it != range.second; ) {
            auto locked = map_it->second.lock();
            if(!locked || locked.get() == ptr) {
                map_it = sessions_by_user_.erase(map_it);
            } else {
                ++map_it;
            }
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

    // 使用缓存获取成员列表,大幅减少数据库查询
    std::vector<i64> member_ids;
    if(auto cache = get_conversation_cache(conversation_id)) {
        member_ids = cache->member_ids;
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

auto Server::broadcast_world_message(database::StoredMessage const& stored,i64 sender_id,std::string const& content) -> void
{
    json push;
    push["conversationId"] = std::to_string(stored.conversation_id);
    
    // 优化：使用缓存获取会话信息,从 3 次数据库查询减少到 0-1 次
    std::string conv_type{ "GROUP" };
    std::string sender_name;
    std::vector<i64> member_ids;
    
    // 从缓存获取会话类型和成员列表
    if(auto cache = get_conversation_cache(stored.conversation_id)) {
        conv_type = cache->type;
        member_ids = cache->member_ids;
    }
    
    // 发送者昵称仅在需要时查询（暂不缓存用户信息）
    if(sender_id > 0 && stored.msg_type != "SYSTEM") {
        try {
            auto conn = database::make_connection();
            pqxx::work tx{ conn };
            auto const sender_query =
                "SELECT display_name FROM users WHERE id = " + tx.quote(sender_id)
                + " LIMIT 1";
            auto sender_rows = tx.exec(sender_query);
            if(!sender_rows.empty()) {
                sender_name = sender_rows[0][0].as<std::string>();
            }
            tx.commit();
        } catch(std::exception const&) {
            // 失败时使用空字符串
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
