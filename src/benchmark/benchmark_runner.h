#pragma once

#include "benchmark_config.h"
#include "benchmark_client.h"
#include "account_manager.h"

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <vector>

namespace benchmark
{
    namespace asio = boost::asio;

    /// \brief 压测统计数据
    struct Statistics
    {
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;

        // 连接统计
        std::atomic<std::size_t> total_connections{ 0 };
        std::atomic<std::size_t> successful_connections{ 0 };
        std::atomic<std::size_t> failed_connections{ 0 };

        // 连接时间窗口（第一个和最后一个连接的时间）
        std::atomic<std::int64_t> first_connect_time_ns{ 0 };
        std::atomic<std::int64_t> last_connect_time_ns{ 0 };

        // 消息统计（发送即成功模式）
        std::atomic<std::size_t> total_messages_sent{ 0 };    // 已发送数
        std::atomic<std::size_t> ack_confirmed{ 0 };          // ACK 确认数
        std::atomic<std::size_t> ack_timeout{ 0 };            // ACK 超时数
        std::atomic<std::size_t> total_messages_received{ 0 }; // 收到的 MSG_PUSH 数

        /// \brief 记录一次连接完成的时间
        auto record_connect_time() -> void;

        /// \brief 打印统计报告
        auto print_report() const -> void;

        /// \brief 获取运行时长（秒）
        [[nodiscard]] auto duration_seconds() const -> double;

        /// \brief 获取实际连接时间窗口（秒）
        [[nodiscard]] auto connect_window_seconds() const -> double;
    };

    /// \brief 压测执行器
    class BenchmarkRunner
    {
    public:
        /// \brief 构造函数
        BenchmarkRunner(
            asio::io_context& io,
            Config const& config,
            AccountManager& account_manager
        );

        /// \brief 执行连接压测
        /// 所有账号以随机间隔（0-4s）发起连接
        auto run_connection_benchmark() -> asio::awaitable<void>;

        /// \brief 执行消息压测
        /// 所有已连接账号以随机间隔（2-6s）发送消息
        auto run_message_benchmark() -> asio::awaitable<void>;

        /// \brief 执行完整压测（连接 + 消息）
        auto run_full_benchmark() -> asio::awaitable<void>;

        /// \brief 世界频道压测（所有账号向世界频道发消息）
        auto run_world_benchmark() -> asio::awaitable<void>;

        /// \brief 停止压测
        auto stop() -> void;

        /// \brief 获取统计数据
        [[nodiscard]] auto statistics() const -> Statistics const& { return stats_; }

    private:
        /// \brief 单个客户端的连接任务
        auto client_connect_task(std::size_t account_index)
            -> asio::awaitable<void>;

        /// \brief 单个客户端的消息发送任务
        auto client_message_task(
            std::shared_ptr<BenchmarkClient> client,
            std::size_t account_index
        ) -> asio::awaitable<void>;

        /// \brief 单个客户端的世界频道消息发送任务
        auto client_world_message_task(
            std::shared_ptr<BenchmarkClient> client,
            std::size_t account_index
        ) -> asio::awaitable<void>;

        /// \brief 生成随机延迟（毫秒）
        auto random_delay_ms(std::uint32_t min_ms, std::uint32_t max_ms)
            -> std::uint32_t;

        asio::io_context& io_;
        Config const& config_;
        AccountManager& account_manager_;

        std::vector<std::shared_ptr<BenchmarkClient>> clients_;
        std::mutex clients_mutex_;  // 保护 clients_ 的互斥锁
        Statistics stats_;

        std::mt19937 rng_;
        std::atomic<bool> running_{ false };
    };

} // namespace benchmark
