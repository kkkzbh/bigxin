module;

#include <boost/asio.hpp>

export module io.core;

import std;

namespace asio = boost::asio;
using asio::any_io_executor;
using asio::io_context;

// 运行 Asio 的统一入口：单 io_context + 多 std::jthread
export struct IoRunner
{

    explicit IoRunner(std::size_t threads) : thread_count_(std::max(1uz, threads)) {}

    auto executor() noexcept -> any_io_executor
    {
        return io_.get_executor();
    }

    // 启动 N 个线程，其中 N-1 个在后台，当前线程也参与 run()
    auto run() -> void
    {
        if (thread_count_ > 1) {
            threads_.reserve(thread_count_ - 1);
            for (auto i : std::views::iota(1uz, thread_count_)) {
                threads_.emplace_back([this] { io_.run(); });
            }
        }
        io_.run(); // 阻塞当前线程直至 stop()
    }

    auto stop() noexcept -> void
    {
        io_.stop();
    }

    ~IoRunner() noexcept
    {
        stop();
    }

private:
    io_context io_{};
    std::vector<std::jthread> threads_{};
    std::size_t thread_count_{};
};
