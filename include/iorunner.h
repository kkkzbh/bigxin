#pragma once

#include <asio.hpp>
#include <utility.h>
#include <ranges>
#include <thread>

/// \brief 负责统一管理 io_context 和工作线程。
/// \details 把单个 io_context 跑在多个 std::jthread 上。
struct IoRunner
{
    /// \brief 记录希望使用的工作线程数量。
    /// \param threads 计划启动的线程数，最少为 1。
    explicit IoRunner(std::size_t threads) : thread_count_(std::max(1uz, threads)) {}

    /// \brief 获取与内部 io_context 绑定的执行器。
    /// \return 可用于构造 Asio 对象的 any_io_executor。
    auto executor() noexcept -> asio::any_io_executor
    {
        return io_.get_executor();
    }

    /// \brief 启动工作线程并进入事件循环。
    /// \details 启动 N 个线程，其中 N-1 个在后台，当前线程也参与 run()。
    auto run() -> void
    {
        if(thread_count_ > 1) {
            threads_.reserve(thread_count_ - 1);
            for(auto i : std::views::iota(1uz, thread_count_)) {
                threads_.emplace_back (
                    [this] {
                        io_.run();
                    }
                );
            }
        }
        io_.run(); // 阻塞当前线程直至 stop()
    }

    /// \brief 请求停止 io_context。
    auto stop() noexcept -> void
    {
        io_.stop();
    }

    /// \brief 析构时确保已调用 stop()。
    ~IoRunner() noexcept
    {
        stop();
    }

private:
    asio::io_context io_{};
    std::vector<std::jthread> threads_{};
    std::size_t thread_count_{};
};
