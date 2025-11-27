#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/redis.hpp>
#include <database/types.h>

#include <string>
#include <vector>
#include <chrono>

namespace asio = boost::asio;

namespace redis
{
    struct Config {
        std::string host = "127.0.0.1";
        std::string port = "6379";
        std::size_t pool_size = 4;
        std::chrono::milliseconds connect_timeout{ 2000 };
    };

    /// \brief 初始化全局 Redis 连接池（幂等）。
    auto init_pool(asio::any_io_executor exec, Config cfg = {}) -> void;

    /// \brief 优雅关闭连接池，取消后台 async_run，等待健康检查协程退出。
    /// \note 进程退出前调用可避免 Boost.Redis 在析构阶段访问已销毁的对象。
    auto shutdown_pool() -> void;

    /// \brief 生成全局唯一的消息 ID（基于 Redis INCR）。
    auto next_message_id() -> asio::awaitable<i64>;

    /// \brief 为指定会话生成下一个递增 seq。
    auto next_conversation_seq(i64 conversation_id) -> asio::awaitable<i64>;

    /// \brief 将消息写入 Redis 的有序集合，供历史查询与实时消费。
    auto write_message(
        database::StoredMessage const& stored,
        i64 sender_id,
        std::string const& sender_display_name,
        std::string const& content
    ) -> asio::awaitable<void>;

    /// \brief 从 Redis 拉取历史消息，优先使用 after_seq，否则使用 before_seq/最新。
    auto load_history(
        i64 conversation_id,
        i64 after_seq,
        i64 before_seq,
        i64 limit
    ) -> asio::awaitable<std::vector<database::LoadedMessage>>;
}
