#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/co_spawn.hpp>
namespace asio = boost::asio;
#include <stdexec/execution.hpp>
#include <asioexec/use_sender.hpp>
#include <exec/task.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <ranges>
#include <vector>
#include <mutex>
#include <chrono>

#include <utility.h>
#include <database/conversation.h>

namespace database
{
    struct StoredMessage;
} // namespace database

/// \brief 与聊天服务相关的服务器组件。
/// \details 使用 asio::awaitable / use_awaitable 实现的协程式服务器。
struct Session;

/// \brief 简单 TCP 服务器：监听端口并为每个连接创建一个 Session。
struct Server : std::enable_shared_from_this<Server>
{
    /// rief 使用给定执行器和端口构造服务器。
    /// \param exec 关联的 Asio 执行器。
    /// \param port 要监听的本地端口。
    Server(asio::any_io_executor exec, u16 port) 
        : acceptor_(exec, asio::ip::tcp::endpoint{ asio::ip::tcp::v4(), port })
        , strand_(exec)
    {}

    /// \brief 接收连接并为每个连接启动一个 Session 协程。
    /// \return 协程完成时返回 void（通常不会返回）。
    auto run() -> asio::awaitable<void>;

private:
    template<typename Fn>
    void dispatch_on_strand(Fn&& fn)
    {
        if(strand_.running_in_this_thread()) {
            fn();
        } else {
            asio::dispatch(strand_, std::forward<Fn>(fn));
        }
    }

    friend struct Session;

    /// \brief 从会话列表中移除一个已经结束的 Session。
    auto remove_session(Session* ptr) -> void;

    /// \brief 将已鉴权的会话加入 user_id 索引。
    auto index_authenticated_session(std::shared_ptr<Session> const& session) -> void;

    /// \brief 遍历指定用户的在线会话。
    template<typename Fn>
    auto for_user_sessions(i64 user_id, Fn fn) -> void
    {
        for(auto [it,end] = sessions_by_user_.equal_range(user_id); it != end; ) {
            if(auto s = it->second.lock()) {
                fn(s);
                ++it;
            } else {
                it = sessions_by_user_.erase(it);
            }
        }
    }

    /// \brief 遍历所有已鉴权会话，过程中过滤空指针并清理失效 weak_ptr 索引。
    template<typename Fn>
    auto for_all_authenticated_sessions(Fn fn) -> void
    {
        for(auto const& session_ptr : sessions_ | std::views::values) {
            if(not session_ptr or not session_ptr->is_authenticated()) {
                continue;
            }
            fn(session_ptr);
        }

        std::erase_if (
            sessions_by_user_,
            [](auto const& p) {
                auto const& [_,wptr] = p;
                return wptr.expired();
            }
        );
    }

    /// \brief 主动向指定用户推送一份最新的“新的朋友”列表。
    /// \details 用 FRIEND_REQ_LIST_RESP 的形式下发，复用现有前端处理逻辑。
    auto send_friend_request_list_to(i64 target_user_id) -> void;

    /// \brief 主动向指定用户推送一份最新的好友列表。
    /// \details 复用 FRIEND_LIST_RESP 结构，便于前端直接重建模型。
    auto send_friend_list_to(i64 target_user_id) -> void;

    /// \brief 主动下发会话列表给指定用户，结构同 CONV_LIST_RESP。
    auto send_conv_list_to(i64 target_user_id) -> void;

    /// \brief 主动向指定用户推送一份最新的入群申请列表。
    /// \details 用 GROUP_JOIN_REQ_LIST_RESP 的形式下发。
    auto send_group_join_request_list_to(i64 target_user_id) -> void;

    /// \brief 推送指定会话的成员列表给会话成员（或指定用户）。
    /// \param conversation_id 会话 ID。
    /// \param only_user_id 若大于 0，则仅向该用户推送；为 0 时向所有在线群成员推送。
    auto send_conv_members(i64 conversation_id, i64 only_user_id = 0) -> void;

    /// \brief 将系统消息以 MSG_PUSH 形式广播给指定会话成员。
    auto broadcast_system_message(i64 conversation_id,database::StoredMessage const& stored,std::string const& content) -> void;

    /// \brief 在指定会话中广播一条消息（群聊 / 单聊通用）。
    /// \param stored 已持久化的消息信息。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    auto broadcast_world_message(database::StoredMessage const& stored,i64 sender_id,std::string const& content,std::string const& sender_display_name = {}) -> void;

    asio::ip::tcp::acceptor acceptor_;
    
    /// \brief strand 保证 sessions_ 和 sessions_by_user_ 的线程安全访问。
    asio::strand<asio::any_io_executor> strand_;
    
    /// \brief 按 Session* 存储会话,支持 O(1) 删除。
    std::unordered_map<Session*, std::shared_ptr<Session>> sessions_{};
    
    /// \brief 按 user_id 建立的在线会话索引,一位多连时存多条 weak_ptr。
    std::unordered_multimap<i64, std::weak_ptr<Session>> sessions_by_user_{};

public:
    /// \brief 会话缓存条目,包含成员列表和类型。
    struct ConversationCache {
        std::vector<i64> member_ids;          ///< 会话成员ID列表
        std::string type;                      ///< 会话类型 ("SINGLE" 或 "GROUP")
        std::chrono::steady_clock::time_point last_access; ///< 最后访问时间
    };

    /// \brief 成员列表缓存，包含完整 MemberInfo，供成员列表分页查询使用。
    struct MemberListCache {
        std::vector<database::MemberInfo> members;
        std::chrono::steady_clock::time_point last_access;
    };

    /// \brief 获取会话缓存(带自动加载和更新)。
    /// \param conversation_id 会话ID。
    /// \return 可选的缓存条目,失败时返回空。
    auto get_conversation_cache(i64 conversation_id) -> std::optional<ConversationCache>;

    /// \brief 清除指定会话的缓存(当成员变动时调用)。
    /// \param conversation_id 会话ID。
    auto invalidate_conversation_cache(i64 conversation_id) -> void;

    /// \brief 清理过期缓存(超过5分钟未访问)。
    auto cleanup_expired_cache() -> void;

    /// \brief 获取成员列表缓存(分页查询复用)。
    auto get_member_list_cache(i64 conversation_id) -> std::optional<MemberListCache>;

    /// \brief 写入/更新成员列表缓存。
    auto set_member_list_cache(i64 conversation_id, std::vector<database::MemberInfo> members) -> void;

    /// \brief 使成员列表缓存失效。
    auto invalidate_member_list_cache(i64 conversation_id) -> void;

private:
    /// \brief 会话成员列表缓存。
    std::unordered_map<i64, ConversationCache> conv_cache_{};
    /// \brief 成员详情缓存。
    std::unordered_map<i64, MemberListCache> member_cache_{};
    /// \brief 保护缓存的互斥锁。
    std::mutex cache_mutex_{};
    /// \brief 缓存过期时间(5分钟)。
    static constexpr auto CACHE_EXPIRE_DURATION = std::chrono::minutes(5);
};

/// \brief 方便 main 调用的启动入口，返回服务器运行协程。
/// \param exec 关联的 Asio 执行器。
/// \param port 要监听的本地端口。
auto inline async_start_server(asio::any_io_executor exec, u16 port) -> stdexec::sender auto
{
    return asio::co_spawn (
        exec,
        [exec, port] -> asio::awaitable<void> {
            auto server = std::make_shared<Server>(exec, port);
            co_await server->run();
        },
        asioexec::use_sender
    );
}
