#pragma once

#include <cstdint>
#include <string>

namespace benchmark
{
    /// \brief 全局压测配置
    struct Config
    {
        // ==================== 服务器配置 ====================
        /// 服务器地址
        std::string server_host = "127.0.0.1";
        /// 服务器端口
        std::uint16_t server_port = 5555;

        // ==================== 账号配置 ====================
        /// 账号前缀（方便多次压测时区分）
        std::string account_prefix = "bench_";
        /// 观察者账号后缀
        std::string observer_suffix = "ob";
        /// 测试账号数量
        std::size_t account_count = 200; // 默认 200 人，便于单机压测
        /// 统一密码
        std::string password = "123456";

        // ==================== 群聊配置 ====================
        /// 群聊数量
        std::size_t group_count = 10;
        /// 群聊名称前缀
        std::string group_prefix = "bench_group_";

        // ==================== 连接压测配置 ====================
        /// 连接延迟最小值（毫秒）
        std::uint32_t connect_delay_min_ms = 0;
        /// 连接延迟最大值（毫秒）
        std::uint32_t connect_delay_max_ms = 4000;

        // ==================== 消息压测配置 ====================
        /// 消息发送间隔最小值（毫秒）
        std::uint32_t message_interval_min_ms = 2000;
        /// 消息发送间隔最大值（毫秒）
        std::uint32_t message_interval_max_ms = 6000;
        /// 压测持续时间（秒），0 表示无限
        std::uint32_t test_duration_seconds = 60;

        // ==================== 线程池配置 ====================
        /// 工作线程数量，0 表示使用硬件并发数
        std::size_t thread_count = 0;

        // ==================== 辅助函数 ====================

        /// 生成测试账号名
        [[nodiscard]] auto make_account_name(std::size_t index) const -> std::string
        {
            return account_prefix + std::to_string(index);
        }

        /// 生成观察者账号名
        [[nodiscard]] auto make_observer_account() const -> std::string
        {
            return account_prefix + observer_suffix;
        }

        /// 生成群聊名称
        [[nodiscard]] auto make_group_name(std::size_t index) const -> std::string
        {
            return group_prefix + std::to_string(index);
        }

        /// 计算账号所属的群聊索引
        [[nodiscard]] auto get_group_index(std::size_t account_index) const -> std::size_t
        {
            return account_index % group_count;
        }

        /// 获取每个群的成员数量
        [[nodiscard]] auto get_members_per_group() const -> std::size_t
        {
            return account_count / group_count;
        }
    };

    /// 全局默认配置
    inline Config g_config;

} // namespace benchmark
