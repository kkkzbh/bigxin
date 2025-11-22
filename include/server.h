#pragma once

#include <asio.hpp>
#include <stdexec/execution.hpp>
#include <asioexec/use_sender.hpp>

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <utility.h>

namespace database
{
    struct StoredMessage;
} // namespace database

/// \brief 与聊天服务相关的服务器组件。
/// \details 使用 asio::awaitable / use_awaitable 实现的协程式服务器。
struct Session;

/// \brief 简单 TCP 服务器：监听端口并为每个连接创建一个 Session。
struct Server
{
    /// \brief 使用给定执行器和端口构造服务器。
    /// \param exec 关联的 Asio 执行器。
    /// \param port 要监听的本地端口。
    Server(asio::any_io_executor exec, u16 port)
        : acceptor_(exec, asio::ip::tcp::endpoint{ asio::ip::tcp::v4(), port })
    {}

    /// \brief 启动异步 accept 协程。
    auto start_accept() -> void
    {
        asio::co_spawn(
            acceptor_.get_executor(),
            run(),
            asio::detached
        );
    }

    /// \brief 接收连接并为每个连接启动一个 Session 协程。
    /// \return 协程完成时返回 void（通常不会返回）。
    auto run() -> asio::awaitable<void>;

private:
    friend struct Session;

    /// \brief 从会话列表中移除一个已经结束的 Session。
    auto remove_session(Session* ptr) -> void;

    /// \brief 将已鉴权的会话加入 user_id 索引。
    auto index_authenticated_session(std::shared_ptr<Session> const& session) -> void;

    /// \brief 遍历指定用户的在线会话（自动清理过期 weak_ptr）。
    template<class Fn>
    auto for_user_sessions(i64 user_id, Fn&& fn) -> void
    {
        auto range = sessions_by_user_.equal_range(user_id);
        for(auto it = range.first; it != range.second; ) {
            if(auto s = it->second.lock()) {
                std::forward<Fn>(fn)(s);
                ++it;
            } else {
                it = sessions_by_user_.erase(it);
            }
        }
    }

    /// \brief 遍历所有已鉴权会话，过程中过滤空指针并清理失效 weak_ptr 索引。
    template<class Fn>
    auto for_all_authenticated_sessions(Fn&& fn) -> void
    {
        for(auto it = sessions_.begin(); it != sessions_.end(); ) {
            if(!(*it)) {
                it = sessions_.erase(it);
                continue;
            }
            if(!(*it)->is_authenticated()) {
                ++it;
                continue;
            }
            std::forward<Fn>(fn)(*it);
            ++it;
        }

        for(auto map_it = sessions_by_user_.begin(); map_it != sessions_by_user_.end(); ) {
            if(map_it->second.expired()) {
                map_it = sessions_by_user_.erase(map_it);
            } else {
                ++map_it;
            }
        }
    }

    /// \brief 主动向指定用户推送一份最新的“新的朋友”列表。
    /// \details 用 FRIEND_REQ_LIST_RESP 的形式下发，复用现有前端处理逻辑。
    auto send_friend_request_list_to(i64 target_user_id) -> void;

    /// \brief 主动向指定用户推送一份最新的好友列表。
    /// \details 复用 FRIEND_LIST_RESP 结构，便于前端直接重建模型。
    auto send_friend_list_to(i64 target_user_id) -> void;

    /// \brief 主动下发会话列表给指定用户，结构同 CONV_LIST_RESP。
    auto send_conv_list_to(i64 target_user_id) -> void;

    /// \brief 推送指定会话的成员列表给会话成员（或指定用户）。
    /// \param conversation_id 会话 ID。
    /// \param only_user_id 若大于 0，则仅向该用户推送；为 0 时向所有在线群成员推送。
    auto send_conv_members(i64 conversation_id, i64 only_user_id = 0) -> void;

    /// \brief 将系统消息以 MSG_PUSH 形式广播给指定会话成员。
    auto broadcast_system_message(
        i64 conversation_id,
        database::StoredMessage const& stored,
        std::string const& content
    ) -> void;

    /// \brief 在指定会话中广播一条消息（群聊 / 单聊通用）。
    /// \param stored 已持久化的消息信息。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    auto broadcast_world_message(
        database::StoredMessage const& stored,
        i64 sender_id,
        std::string const& content
    ) -> void;

    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::shared_ptr<Session>> sessions_{};
    /// \brief 按 user_id 建立的在线会话索引，一位多连时存多条 weak_ptr。
    std::unordered_multimap<i64, std::weak_ptr<Session>> sessions_by_user_{};
};

/// \brief 方便 main 调用的启动入口，返回服务器运行协程。
/// \param exec 关联的 Asio 执行器。
/// \param port 要监听的本地端口。
auto inline start_server(asio::any_io_executor exec, u16 port) -> void
{
    auto snd = asio::co_spawn (
        exec,
        [exec, port] -> asio::awaitable<void> {
            auto server = Server{ exec, port };
            co_await server.run();
        },
        asioexec::use_sender
    );
    stdexec::sync_wait(snd);
}
