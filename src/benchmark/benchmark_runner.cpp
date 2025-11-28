#include "benchmark_runner.h"

#include <iostream>
#include <format>

namespace benchmark
{
    auto Statistics::print_report() const -> void
    {
        auto duration = duration_seconds();

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "           压测统计报告\n";
        std::cout << "========================================\n";
        std::cout << std::format("运行时长: {:.2f} 秒\n", duration);
        std::cout << "\n";

        std::cout << "--- 连接统计 ---\n";
        std::cout << std::format("总连接尝试: {}\n", total_connections.load());
        std::cout << std::format("成功连接: {}\n", successful_connections.load());
        std::cout << std::format("失败连接: {}\n", failed_connections.load());
        if(total_connections.load() > 0) {
            auto success_rate = 100.0 * successful_connections.load() / total_connections.load();
            std::cout << std::format("连接成功率: {:.2f}%\n", success_rate);
        }
        auto connect_window = connect_window_seconds();
        if(connect_window > 0) {
            auto connect_qps = successful_connections.load() / connect_window;
            std::cout << std::format("连接时间窗口: {:.2f} 秒\n", connect_window);
            std::cout << std::format("连接 QPS: {:.2f} 连接/秒\n", connect_qps);
        }
        std::cout << "\n";

        std::cout << "--- 消息统计 ---\n";
        std::cout << std::format("发送消息总数: {}\n", total_messages_sent.load());
        std::cout << std::format("ACK 确认数: {}\n", ack_confirmed.load());
        std::cout << std::format("ACK 超时数: {}\n", ack_timeout.load());
        std::cout << std::format("接收消息总数: {}\n", total_messages_received.load());

        if(total_messages_sent.load() > 0) {
            auto ack_rate = 100.0 * ack_confirmed.load() / total_messages_sent.load();
            std::cout << std::format("ACK 确认率: {:.2f}%\n", ack_rate);
        }

        if(duration > 0) {
            auto send_qps = total_messages_sent.load() / duration;
            auto recv_qps = total_messages_received.load() / duration;
            std::cout << std::format("发送 QPS: {:.2f}\n", send_qps);
            std::cout << std::format("接收 QPS: {:.2f}\n", recv_qps);
        }

        std::cout << "========================================\n\n";
    }

    auto Statistics::duration_seconds() const -> double
    {
        auto end = end_time.time_since_epoch().count() > 0 ? end_time : std::chrono::steady_clock::now();
        return std::chrono::duration<double>(end - start_time).count();
    }

    auto Statistics::connect_window_seconds() const -> double
    {
        auto first = first_connect_time_ns.load();
        auto last = last_connect_time_ns.load();
        if(first == 0 || last == 0 || last <= first) {
            return duration_seconds();
        }
        return static_cast<double>(last - first) / 1e9;
    }

    auto Statistics::record_connect_time() -> void
    {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();

        // 原子更新 first_connect_time（只在为0时更新）
        std::int64_t expected = 0;
        first_connect_time_ns.compare_exchange_strong(expected, now);

        // 原子更新 last_connect_time（总是更新为更大的值）
        auto current = last_connect_time_ns.load();
        while(now > current && !last_connect_time_ns.compare_exchange_weak(current, now)) {
            // 重试
        }
    }

    BenchmarkRunner::BenchmarkRunner(
        asio::io_context& io,
        Config const& config,
        AccountManager& account_manager
    )
        : io_{ io }
        , config_{ config }
        , account_manager_{ account_manager }
        , rng_{ std::random_device{}() }
    {
    }

    auto BenchmarkRunner::run_connection_benchmark() -> asio::awaitable<void>
    {
        std::cout << "\n[BenchmarkRunner] Starting connection benchmark...\n";

        // 计算实际使用的时间窗口（毫秒）
        auto window_ms = config_.connect_window_seconds > 0
            ? config_.connect_window_seconds * 1000
            : config_.connect_delay_max_ms;

        std::cout << std::format(
            "[BenchmarkRunner] {} accounts, connect window: {} ms\n",
            config_.account_count,
            window_ms
        );

        stats_.start_time = std::chrono::steady_clock::now();
        running_ = true;

        auto const& accounts = account_manager_.accounts();
        auto task_count = std::min(config_.account_count, accounts.size());

        // 使用 atomic 计数器跟踪完成的任务数
        auto completed = std::make_shared<std::atomic<std::size_t>>(0);

        // 并行启动所有客户端的连接任务（每个任务内部自带随机延迟）
        for(std::size_t i = 0; i < task_count && running_; ++i) {
            asio::co_spawn(
                io_,
                [this, i, completed, task_count]() -> asio::awaitable<void> {
                    co_await client_connect_task(i);
                    auto done = completed->fetch_add(1) + 1;
                    // 每完成 10% 输出一次进度
                    if(done % (task_count / 10 + 1) == 0 || done == task_count) {
                        std::cout << std::format(
                            "[BenchmarkRunner] Progress: {}/{} tasks completed\n",
                            done, task_count
                        );
                    }
                },
                asio::detached
            );
        }

        std::cout << std::format(
            "[BenchmarkRunner] Spawned {} connection tasks, waiting for completion...\n",
            task_count
        );

        // 等待所有任务完成或超时
        auto wait_seconds = (window_ms / 1000) + 15;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(wait_seconds);

        while(completed->load() < task_count && std::chrono::steady_clock::now() < deadline) {
            asio::steady_timer wait_timer{ io_ };
            wait_timer.expires_after(std::chrono::milliseconds(500));
            co_await wait_timer.async_wait(asio::use_awaitable);
        }

        stats_.end_time = std::chrono::steady_clock::now();
        running_ = false;

        std::cout << std::format(
            "[BenchmarkRunner] Connection benchmark completed. {}/{} tasks finished.\n",
            completed->load(), task_count
        );
        stats_.print_report();
    }

    auto BenchmarkRunner::run_message_benchmark() -> asio::awaitable<void>
    {
        std::cout << "\n[BenchmarkRunner] Starting message benchmark (fire-and-forget mode)...\n";
        std::cout << std::format(
            "[BenchmarkRunner] {} clients, message interval: {}-{} ms, duration: {} s\n",
            clients_.size(),
            config_.message_interval_min_ms,
            config_.message_interval_max_ms,
            config_.test_duration_seconds
        );

        // 如果没有已连接的客户端，先建立连接
        if(clients_.empty()) {
            std::cout << "[BenchmarkRunner] No connected clients, establishing connections first...\n";

            auto const& accounts = account_manager_.accounts();
            clients_.reserve(accounts.size());

            for(std::size_t i = 0; i < accounts.size(); ++i) {
                auto client = std::make_shared<BenchmarkClient>(io_);

                if(co_await client->async_connect(config_.server_host, config_.server_port)) {
                    auto const& account = accounts[i];
                    auto user_id = co_await client->async_login(account.account, config_.password);

                    if(!user_id.empty()) {
                        // 设置消息接收回调（统计 MSG_PUSH）
                        client->set_response_callback(
                            [this](std::string const& cmd, json const&) {
                                if(cmd == "MSG_PUSH") {
                                    ++stats_.total_messages_received;
                                }
                            }
                        );
                        clients_.push_back(client);
                        ++stats_.successful_connections;
                    }
                }

                // 每 50 个输出进度
                if((i + 1) % 50 == 0) {
                    std::cout << std::format(
                        "[BenchmarkRunner] Connected {}/{} clients\n",
                        clients_.size(), accounts.size()
                    );
                }
            }

            std::cout << std::format(
                "[BenchmarkRunner] {} clients connected\n",
                clients_.size()
            );
        }

        // 为每个客户端启动后台读取协程（处理 ACK 和 MSG_PUSH）
        std::cout << "[BenchmarkRunner] Starting background readers...\n";
        for(auto& client : clients_) {
            asio::co_spawn(
                io_,
                client->start_background_reader(),
                asio::detached
            );
        }

        stats_.start_time = std::chrono::steady_clock::now();
        running_ = true;

        std::cout << "[BenchmarkRunner] Starting message sender tasks...\n";

        // 为每个客户端启动消息发送任务
        for(std::size_t i = 0; i < clients_.size(); ++i) {
            asio::co_spawn(
                io_,
                client_message_task(clients_[i], i),
                asio::detached
            );
        }

        // 运行指定时长
        if(config_.test_duration_seconds > 0) {
            asio::steady_timer duration_timer{ io_ };
            duration_timer.expires_after(std::chrono::seconds(config_.test_duration_seconds));
            co_await duration_timer.async_wait(asio::use_awaitable);
        }

        running_ = false;

        // 等待一小段时间让剩余的 ACK 到达
        std::cout << "[BenchmarkRunner] Waiting for remaining ACKs...\n";
        asio::steady_timer ack_wait_timer{ io_ };
        ack_wait_timer.expires_after(std::chrono::seconds(2));
        co_await ack_wait_timer.async_wait(asio::use_awaitable);

        // 汇总所有客户端的 ACK 统计
        for(auto const& client : clients_) {
            auto const& ack_stats = client->ack_stats();
            stats_.ack_confirmed += ack_stats.ack_received.load();
            stats_.ack_timeout += ack_stats.ack_timeout.load();
        }

        // 等待 5 秒后再断开连接，给服务端足够时间处理完所有数据库操作
        std::cout << "[BenchmarkRunner] Waiting 5 seconds before disconnecting...\n";
        asio::steady_timer disconnect_wait_timer{ io_ };
        disconnect_wait_timer.expires_after(std::chrono::seconds(5));
        co_await disconnect_wait_timer.async_wait(asio::use_awaitable);

        stop();

        stats_.end_time = std::chrono::steady_clock::now();
        std::cout << "[BenchmarkRunner] Message benchmark completed.\n";
        stats_.print_report();
    }

    auto BenchmarkRunner::run_world_benchmark() -> asio::awaitable<void>
    {
        std::cout << "\n[BenchmarkRunner] Starting WORLD benchmark (all accounts -> world)...\n";

        // 如果没有已连接的客户端，先建立连接并登录
        if(clients_.empty()) {
            std::cout << "[BenchmarkRunner] Connecting and logging in clients...\n";

            auto const& accounts = account_manager_.accounts();
            clients_.reserve(accounts.size());

            for(std::size_t i = 0; i < accounts.size(); ++i) {
                auto client = std::make_shared<BenchmarkClient>(io_);

                if(co_await client->async_connect(config_.server_host, config_.server_port)) {
                    auto const& account = accounts[i];
                    auto user_id = co_await client->async_login(account.account, config_.password);

                    if(!user_id.empty()) {
                        client->set_response_callback(
                            [this](std::string const& cmd, json const&) {
                                if(cmd == "MSG_PUSH") {
                                    ++stats_.total_messages_received;
                                }
                            }
                        );
                        clients_.push_back(client);
                        ++stats_.successful_connections;
                    }
                }

                if((i + 1) % 50 == 0) {
                    std::cout << std::format(
                        "[BenchmarkRunner] Connected {}/{} clients\n",
                        clients_.size(), accounts.size()
                    );
                }
            }

            std::cout << std::format(
                "[BenchmarkRunner] {} clients connected\n",
                clients_.size()
            );
        }

        // 启动后台读取协程
        std::cout << "[BenchmarkRunner] Starting background readers...\n";
        for(auto& client : clients_) {
            asio::co_spawn(io_, client->start_background_reader(), asio::detached);
        }

        stats_.start_time = std::chrono::steady_clock::now();
        running_ = true;

        std::cout << "[BenchmarkRunner] Starting world message sender tasks...\n";

        for(std::size_t i = 0; i < clients_.size(); ++i) {
            asio::co_spawn(
                io_,
                client_world_message_task(clients_[i], i),
                asio::detached
            );
        }

        if(config_.test_duration_seconds > 0) {
            asio::steady_timer duration_timer{ io_ };
            duration_timer.expires_after(std::chrono::seconds(config_.test_duration_seconds));
            co_await duration_timer.async_wait(asio::use_awaitable);
        }

        running_ = false;

        std::cout << "[BenchmarkRunner] Waiting for remaining ACKs...\n";
        asio::steady_timer ack_wait_timer{ io_ };
        ack_wait_timer.expires_after(std::chrono::seconds(2));
        co_await ack_wait_timer.async_wait(asio::use_awaitable);

        for(auto const& client : clients_) {
            auto const& ack_stats = client->ack_stats();
            stats_.ack_confirmed += ack_stats.ack_received.load();
            stats_.ack_timeout += ack_stats.ack_timeout.load();
        }

        // 等待 5 秒后再断开连接，给服务端足够时间处理完所有数据库操作
        std::cout << "[BenchmarkRunner] Waiting 5 seconds before disconnecting...\n";
        asio::steady_timer disconnect_wait_timer{ io_ };
        disconnect_wait_timer.expires_after(std::chrono::seconds(5));
        co_await disconnect_wait_timer.async_wait(asio::use_awaitable);

        stop();

        stats_.end_time = std::chrono::steady_clock::now();
        std::cout << "[BenchmarkRunner] WORLD benchmark completed.\n";
        stats_.print_report();
    }

    auto BenchmarkRunner::run_full_benchmark() -> asio::awaitable<void>
    {
        std::cout << "\n[BenchmarkRunner] Starting full benchmark...\n";

        // 先执行连接压测
        co_await run_connection_benchmark();

        // 等待一小段时间
        asio::steady_timer gap_timer{ io_ };
        gap_timer.expires_after(std::chrono::seconds(2));
        co_await gap_timer.async_wait(asio::use_awaitable);

        // 再执行消息压测
        co_await run_message_benchmark();
    }

    auto BenchmarkRunner::stop() -> void
    {
        running_ = false;

        // 关闭所有客户端连接
        for(auto& client : clients_) {
            if(client) {
                client->close();
            }
        }
    }

    auto BenchmarkRunner::client_connect_task(std::size_t account_index)
        -> asio::awaitable<void>
    {
        auto const& accounts = account_manager_.accounts();
        if(account_index >= accounts.size()) {
            co_return;
        }

        // 计算实际使用的时间窗口（毫秒）
        auto window_ms = config_.connect_window_seconds > 0
            ? config_.connect_window_seconds * 1000
            : config_.connect_delay_max_ms;

        // 每个任务内部自己处理随机延迟，实现并行的随机延迟连接
        auto delay = random_delay_ms(config_.connect_delay_min_ms, window_ms);
        asio::steady_timer timer{ io_ };
        timer.expires_after(std::chrono::milliseconds(delay));
        co_await timer.async_wait(asio::use_awaitable);

        if(!running_) {
            co_return;
        }

        auto const& account = accounts[account_index];
        ++stats_.total_connections;

        auto client = std::make_shared<BenchmarkClient>(io_);

        if(!co_await client->async_connect(config_.server_host, config_.server_port)) {
            ++stats_.failed_connections;
            co_return;
        }

        auto user_id = co_await client->async_login(account.account, config_.password);
        if(user_id.empty()) {
            ++stats_.failed_connections;
            client->close();
            co_return;
        }

        ++stats_.successful_connections;
        stats_.record_connect_time();  // 记录连接完成时间

        // 设置消息接收回调
        client->set_response_callback(
            [this](std::string const& cmd, json const&) {
                if(cmd == "MSG_PUSH") {
                    ++stats_.total_messages_received;
                }
            }
        );

        {
            std::lock_guard lock{ clients_mutex_ };
            clients_.push_back(client);
        }
    }

    auto BenchmarkRunner::client_message_task(
        std::shared_ptr<BenchmarkClient> client,
        std::size_t account_index
    ) -> asio::awaitable<void>
    {
        auto conversation_id = account_manager_.get_conversation_id(account_index);
        if(conversation_id.empty()) {
            std::cerr << std::format(
                "[BenchmarkRunner] No conversation_id for account index {}\n",
                account_index
            );
            co_return;
        }

        std::size_t msg_count = 0;
        while(running_ && client->is_connected()) {
            // 随机延迟
            auto delay = random_delay_ms(
                config_.message_interval_min_ms,
                config_.message_interval_max_ms
            );

            asio::steady_timer timer{ io_ };
            timer.expires_after(std::chrono::milliseconds(delay));
            co_await timer.async_wait(asio::use_awaitable);

            if(!running_ || !client->is_connected()) {
                break;
            }

            // 发送消息（发送即成功模式）
            ++stats_.total_messages_sent;
            auto content = std::format(
                "[Benchmark] Account {} Message #{}",
                account_index, ++msg_count
            );

            try {
                // 使用 fire-and-forget 模式，不等待 ACK
                co_await client->send_message_fire_and_forget(conversation_id, content);
                // ACK 统计由后台读取协程更新
            }
            catch(std::exception const&) {
                // 发送失败（网络错误等）
            }
        }

        // ACK 统计由 run_message_benchmark 统一汇总
    }

    auto BenchmarkRunner::client_world_message_task(
        std::shared_ptr<BenchmarkClient> client,
        std::size_t account_index
    ) -> asio::awaitable<void>
    {
        auto const& conversation_id = client->world_conversation_id();
        if(conversation_id.empty()) {
            std::cerr << std::format(
                "[BenchmarkRunner] No worldConversationId for account index {}\n",
                account_index
            );
            co_return;
        }

        std::size_t msg_count = 0;
        while(running_ && client->is_connected()) {
            auto delay = random_delay_ms(
                config_.message_interval_min_ms,
                config_.message_interval_max_ms
            );

            asio::steady_timer timer{ io_ };
            timer.expires_after(std::chrono::milliseconds(delay));
            co_await timer.async_wait(asio::use_awaitable);

            if(!running_ || !client->is_connected()) {
                break;
            }

            ++stats_.total_messages_sent;
            auto content = std::format(
                "[World Benchmark] Account {} Message #{}",
                account_index, ++msg_count
            );

            try {
                co_await client->send_message_fire_and_forget(conversation_id, content);
            } catch(std::exception const&) {
                // ignore send failures
            }
        }
    }

    auto BenchmarkRunner::random_delay_ms(std::uint32_t min_ms, std::uint32_t max_ms)
        -> std::uint32_t
    {
        if(min_ms >= max_ms) {
            return min_ms;
        }
        std::uniform_int_distribution<std::uint32_t> dist{ min_ms, max_ms };
        return dist(rng_);
    }

} // namespace benchmark
