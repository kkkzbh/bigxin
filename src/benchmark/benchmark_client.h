#pragma once

#include "benchmark_config.h"
#include "protocol.h"

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <deque>
#include <unordered_map>

namespace benchmark
{
    namespace asio = boost::asio;
    using json = nlohmann::json;
    using tcp = asio::ip::tcp;

    /// \brief 响应回调类型
    using ResponseCallback = std::function<void(std::string const& command, json const& payload)>;

    /// \brief 消息发送状态
    enum class MessageStatus {
        Pending,    // 已发送，等待 ACK
        Confirmed,  // 已确认
        Failed      // 发送失败（超时或错误）
    };

    /// \brief 待确认消息信息
    struct PendingMessage {
        std::string client_msg_id;
        std::chrono::steady_clock::time_point send_time;
        MessageStatus status = MessageStatus::Pending;
    };

    /// \brief ACK 处理统计
    struct AckStats {
        std::atomic<std::size_t> total_sent{ 0 };       // 总发送数
        std::atomic<std::size_t> ack_received{ 0 };     // 收到 ACK 数
        std::atomic<std::size_t> ack_timeout{ 0 };      // 超时数
    };

    /// \brief 轻量级压测客户端，基于 Boost Asio 协程
    /// 采用"发送即成功"模式，后台异步处理 ACK
    class BenchmarkClient : public std::enable_shared_from_this<BenchmarkClient>
    {
    public:
        /// \brief 构造函数
        /// \param io io_context 引用
        explicit BenchmarkClient(asio::io_context& io);

        ~BenchmarkClient();

        // 禁止拷贝
        BenchmarkClient(BenchmarkClient const&) = delete;
        BenchmarkClient& operator=(BenchmarkClient const&) = delete;

        /// \brief 异步连接到服务器（协程）
        auto async_connect(std::string const& host, std::uint16_t port)
            -> asio::awaitable<bool>;

        /// \brief 异步发送注册请求（协程）
        auto async_register(std::string const& account, std::string const& password)
            -> asio::awaitable<bool>;

        /// \brief 异步发送登录请求（协程）
        /// \return 成功时返回 userId，失败返回空字符串
        auto async_login(std::string const& account, std::string const& password)
            -> asio::awaitable<std::string>;

        /// \brief 异步创建群聊（协程）
        /// \return 成功时返回 conversationId，失败返回空字符串
        auto async_create_group(std::string const& name, std::vector<std::string> const& member_ids)
            -> asio::awaitable<std::string>;

        /// \brief 发送消息（发送即成功模式）
        /// 立即返回，不等待 ACK，ACK 在后台异步处理
        auto send_message_fire_and_forget(std::string const& conversation_id, std::string const& content)
            -> asio::awaitable<void>;

        /// \brief 异步发送群聊消息（协程）- 保留旧接口用于 setup 阶段
        auto async_send_message(std::string const& conversation_id, std::string const& content)
            -> asio::awaitable<bool>;

        /// \brief 异步发送 PING（协程）
        auto async_ping() -> asio::awaitable<void>;

        /// \brief 关闭连接
        auto close() -> void;

        /// \brief 检查是否已连接
        [[nodiscard]] auto is_connected() const -> bool;

        /// \brief 获取当前用户 ID
        [[nodiscard]] auto user_id() const -> std::string const& { return user_id_; }
        /// \brief 获取世界会话 ID
        [[nodiscard]] auto world_conversation_id() const -> std::string const& { return world_conversation_id_; }

        /// \brief 设置响应回调（用于接收推送消息）
        auto set_response_callback(ResponseCallback callback) -> void;

        /// \brief 启动后台读取循环（协程）
        /// 处理 MSG_PUSH、SEND_ACK 等所有服务器消息
        auto start_background_reader() -> asio::awaitable<void>;

        /// \brief 获取 ACK 统计
        [[nodiscard]] auto ack_stats() const -> AckStats const& { return ack_stats_; }

        /// \brief 启动消息接收循环（协程）- 保留旧接口
        auto start_read_loop() -> asio::awaitable<void>;

    private:
        /// \brief 发送命令
        auto send_command(std::string const& command, json const& payload)
            -> asio::awaitable<void>;

        /// \brief 读取一行响应
        auto read_response() -> asio::awaitable<protocol::Frame>;

        /// \brief 等待特定命令的响应
        auto wait_for_response(std::string const& expected_command)
            -> asio::awaitable<json>;

        /// \brief 处理收到的 SEND_ACK
        auto handle_send_ack(json const& payload) -> void;

        /// \brief 生成客户端消息 ID
        auto generate_client_msg_id() -> std::string;

        asio::io_context& io_;
        tcp::socket socket_;
        asio::streambuf read_buffer_;

        std::string user_id_;
        std::string world_conversation_id_;
        ResponseCallback response_callback_;

        // 用于存储待处理的响应（setup 阶段使用）
        std::deque<protocol::Frame> pending_frames_;

        // 待确认消息队列（发送即成功模式）
        std::mutex pending_mutex_;
        std::unordered_map<std::string, PendingMessage> pending_messages_;

        // ACK 统计
        AckStats ack_stats_;

        // 消息 ID 计数器
        std::atomic<std::uint64_t> msg_id_counter_{ 0 };

        bool connected_ = false;
    };

} // namespace benchmark
