#include <session.h>
#include <server.h>

#include <database.h>
#include <protocol.h>

#include <nlohmann/json.hpp>
#include <asioexec/use_sender.hpp>
#include <exec/task.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
namespace asio = boost::asio;

#include <optional>
#include <print>
#include <unordered_set>
#include <vector>
#include <chrono>

using nlohmann::json;
using asio::use_awaitable;

// ---------------------------------------------------------------------------
// 本文件负责：
//   - 管理服务器上所有在线 Session 的生命周期与索引（sessions_ / sessions_by_user_）
//   - 根据会话（conversation）及用户维度高效查找在线 Session
//   - 构造并向相关在线客户端广播系统消息 / 普通消息
//
// 并发约定：
//   - 所有对 sessions_ 与 sessions_by_user_ 的读写都应在同一个 asio strand 上执行，
//     本文件中的接口默认在该 strand 上被调用，以避免手动加锁。
//   - Session::send_text 本身是非阻塞的，只负责将数据投递到底层写协程。
// ---------------------------------------------------------------------------

/**
 * @brief 启动服务器主协程，持续接受来自客户端的新连接。
 *
 * 使用 `acceptor_` 异步接受 TCP 连接，为每个连接创建 `Session`，
 * 在 `strand_` 上 `co_spawn` 会话协程以保证对共享状态的串行访问，
 * 会话结束时通过 `remove_session` 进行清理。
 *
 * @return `asio::awaitable<void>` 可被上层 `co_spawn` 或 `co_await`。
 */
auto Server::run() -> asio::awaitable<void>
{
    std::println("Server::run enter");
    auto self = shared_from_this();

    while(true) {
        boost::system::error_code ec;
        auto socket = co_await acceptor_.async_accept(asio::redirect_error(use_awaitable, ec));

        if(ec) {
            if(ec == asio::error::operation_aborted) {
                std::println("accept loop stopped: {}", ec.message());
                break;
            }
            std::println("accept error: {} ({})", ec.message(), ec.value());
            asio::steady_timer timer{ acceptor_.get_executor() };
            timer.expires_after(std::chrono::milliseconds{ 50 });
            co_await timer.async_wait(use_awaitable);
            continue;
        }

        auto const endpoint = socket.remote_endpoint(ec);
        if(!ec) {
            std::println("new connection from: {}:{}", endpoint.address().to_string(), endpoint.port());
        }

        auto session = std::make_shared<Session>(std::move(socket), self);
        
        // 在 Server 的 strand 上注册 session
        asio::dispatch(strand_, [self, session]() {
            self->sessions_[session.get()] = session;
        });
        
        // Session 的 run() 在自己的 strand 上执行，避免阻塞 Server strand
        asio::co_spawn(
            session->strand_,
            [weak = std::weak_ptr<Server>(self), session]() -> asio::awaitable<void> {
                co_await session->run();
                // 清理时需要回到 Server 的 strand
                if(auto srv = weak.lock()) {
                    asio::dispatch(srv->strand_, [srv, raw_ptr = session.get()]() {
                        srv->remove_session(raw_ptr);
                    });
                }
            },
            asio::detached
        );
    }
    std::println("Server::run exit");
}

/**
 * @brief 从在线会话索引中移除指定的 Session。
 *
 * 仅在 Session 存在于 `sessions_` 中时执行删除；若会话已认证，
 * 则进一步在 `sessions_by_user_` 中按用户维度清理对应条目。
 *
 * @param ptr 需要移除的 Session 裸指针，允许为 `nullptr` 或已失效指针。
 */
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

/**
 * @brief 在会话通过鉴权后，按用户维度建立索引。
 *
 * 若会话为空或尚未认证，则不会执行任何操作。
 * 会先清理该用户下已失效或重复的弱引用，再插入最新的会话条目。
 *
 * @param session 已通过鉴权的会话智能指针。
 */
auto Server::index_authenticated_session(std::shared_ptr<Session> const& session) -> void
{
    dispatch_on_strand([this, session]() {
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
    });
}

/**
 * @brief 向指定会话的所有成员广播一条系统消息。
 *
 * 系统消息的 `senderId` 固定为 "0"，`senderDisplayName` 为空，
 * `conversationType` 固定为 "GROUP"。
 * 优先从会话缓存中获取成员列表；若缓存缺失或成员列表为空，则退化为
 * 向所有已认证会话广播。
 *
 * @param conversation_id 目标会话（群/频道）的唯一标识。
 * @param stored 已持久化的消息元信息（ID、类型、时间戳、序列号等）。
 * @param content 要推送给客户端的消息正文内容（JSON 或纯文本）。
 */
auto Server::broadcast_system_message(
    i64 conversation_id,
    database::StoredMessage const& stored,
    std::string const& content
) -> void
{
    dispatch_on_strand([=, this]() {
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
    });
}

/**
 * @brief 向指定会话的在线成员广播一条普通消息（用户消息或系统消息）。
 *
 * 会话类型和成员列表优先从本地缓存读取，必要时才访问数据库。
 * 对于非 SYSTEM 消息，会按需查询发送者的 `display_name`，查询失败时降级为空字符串。
 * 系统消息通过 msg_type 区分，不再依赖 senderId=0。
 * 当成员列表为空时退化为向所有已认证会话广播，否则仅向目标会话成员的在线设备推送。
 *
 * @param stored 数据库中已持久化的消息记录，包含会话 ID、消息类型等。
 * @param sender_id 发送者用户 ID；系统消息同样保留真实 sender_id，由 msg_type 区分。
 * @param content 消息正文，由上层业务构造。
 */
auto Server::broadcast_world_message(database::StoredMessage const& stored,i64 sender_id,std::string const& content,std::string const& sender_display_name) -> void
{
    dispatch_on_strand([=, this]() {
        json push;
        push["conversationId"] = std::to_string(stored.conversation_id);
        
        // 优化：使用缓存获取会话信息,从 3 次数据库查询减少到 0-1 次
        std::string conv_type{ "GROUP" };
        std::string sender_name = sender_display_name;
        std::vector<i64> member_ids;
        
        // 从缓存获取会话类型和成员列表
        if(auto cache = get_conversation_cache(stored.conversation_id)) {
            conv_type = cache->type;
            member_ids = cache->member_ids;
        }
        
        // 若缺失昵称则留空，客户端可自行降级展示
        
        push["conversationType"] = conv_type;
        push["serverMsgId"] = std::to_string(stored.id);
        push["senderId"] = std::to_string(sender_id);
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
    });
}
