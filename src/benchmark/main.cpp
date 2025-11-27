#include "benchmark_config.h"
#include "account_manager.h"
#include "benchmark_runner.h"

#include <boost/asio.hpp>

#include <iostream>
#include <format>
#include <thread>
#include <vector>
#include <string>
#include <cstring>

namespace asio = boost::asio;

/// \brief 打印使用帮助
auto print_usage(char const* program_name) -> void
{
    std::cout << "用法: " << program_name << " <模式> [选项]\n";
    std::cout << "\n";
    std::cout << "模式:\n";
    std::cout << "  setup     - 设置阶段：注册账号、登录、创建群聊（只需执行一次）\n";
    std::cout << "  connect   - 连接压测：测试大量连接的处理能力（需先 setup）\n";
    std::cout << "  message   - 消息压测：测试群聊消息的处理能力（需先 setup）\n";
    std::cout << "  world     - 世界频道压测：所有账号往世界频道发消息（需先 setup）\n";
    std::cout << "  full      - 完整压测：先连接压测，再消息压测（需先 setup）\n";
    std::cout << "\n";
    std::cout << "选项:\n";
    std::cout << "  --host <addr>       服务器地址 (默认: 127.0.0.1)\n";
    std::cout << "  --port <port>       服务器端口 (默认: 5555)\n";
    std::cout << "  --prefix <prefix>   账号前缀 (默认: bench_)\n";
    std::cout << "  --accounts <num>    账号数量 (默认: 200)\n";
    std::cout << "  --groups <num>      群聊数量 (默认: 10)\n";
    std::cout << "  --duration <sec>    压测持续时间秒 (默认: 60)\n";
    std::cout << "  --threads <num>     线程数量 (默认: 硬件并发数)\n";
    std::cout << "  --help              显示帮助信息\n";
    std::cout << "\n";
    std::cout << "典型流程:\n";
    std::cout << "  1. " << program_name << " setup --prefix test1_      # 首次执行，创建账号和群聊\n";
    std::cout << "  2. " << program_name << " connect --prefix test1_    # 连接压测\n";
    std::cout << "  3. " << program_name << " message --prefix test1_    # 消息压测\n";
    std::cout << "\n";
    std::cout << "注意: setup 会将数据保存到 <prefix>benchmark_data.json 文件\n";
}

/// \brief 解析命令行参数
auto parse_args(int argc, char* argv[], benchmark::Config& config, std::string& mode)
    -> bool
{
    if(argc < 2) {
        print_usage(argv[0]);
        return false;
    }

    mode = argv[1];

    if(mode == "--help" || mode == "-h") {
        print_usage(argv[0]);
        return false;
    }

    if(mode != "setup" && mode != "connect" && mode != "message" && mode != "world" && mode != "full") {
        std::cerr << "错误: 未知模式 '" << mode << "'\n";
        print_usage(argv[0]);
        return false;
    }

    for(int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if(arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        }
        else if(arg == "--host" && i + 1 < argc) {
            config.server_host = argv[++i];
        }
        else if(arg == "--port" && i + 1 < argc) {
            config.server_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if(arg == "--prefix" && i + 1 < argc) {
            config.account_prefix = argv[++i];
        }
        else if(arg == "--accounts" && i + 1 < argc) {
            config.account_count = static_cast<std::size_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--groups" && i + 1 < argc) {
            config.group_count = static_cast<std::size_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--duration" && i + 1 < argc) {
            config.test_duration_seconds = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        }
        else if(arg == "--threads" && i + 1 < argc) {
            config.thread_count = static_cast<std::size_t>(std::stoul(argv[++i]));
        }
        else {
            std::cerr << "警告: 未知参数 '" << arg << "'\n";
        }
    }

    return true;
}

/// \brief 主函数
auto main(int argc, char* argv[]) -> int
{
    benchmark::Config config;
    std::string mode;

    if(!parse_args(argc, argv, config, mode)) {
        return 1;
    }

    // 确定线程数量
    auto thread_count = config.thread_count;
    if(thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if(thread_count == 0) {
            thread_count = 4;  // 默认值
        }
    }

    std::cout << "========================================\n";
    std::cout << "        聊天服务器压测工具\n";
    std::cout << "========================================\n";
    std::cout << std::format("服务器: {}:{}\n", config.server_host, config.server_port);
    std::cout << std::format("账号前缀: {}\n", config.account_prefix);
    std::cout << std::format("账号数量: {}\n", config.account_count);
    std::cout << std::format("群聊数量: {}\n", config.group_count);
    std::cout << std::format("线程数量: {}\n", thread_count);
    std::cout << std::format("压测模式: {}\n", mode);
    std::cout << "========================================\n\n";

    // 创建 io_context
    asio::io_context io;

    // 创建工作守卫，防止 io_context 在没有任务时退出
    auto work_guard = asio::make_work_guard(io);

    // 创建线程池
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for(std::size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back([&io]() {
            io.run();
        });
    }

    // 创建账号管理器
    benchmark::AccountManager account_manager{ io, config };

    // 创建压测执行器
    benchmark::BenchmarkRunner runner{ io, config, account_manager };

    // 根据模式执行相应的压测
    if(mode == "setup") {
        asio::co_spawn(
            io,
            [&]() -> asio::awaitable<void> {
                co_await account_manager.setup();
                work_guard.reset();
            },
            asio::detached
        );
    }
    else if(mode == "connect") {
        // 从文件加载数据
        if(!account_manager.load_from_file()) {
            std::cerr << "错误: 请先执行 setup 模式创建账号和群聊\n";
            std::cerr << "示例: " << argv[0] << " setup --prefix " << config.account_prefix << "\n";
            work_guard.reset();
        } else {
            asio::co_spawn(
                io,
                [&]() -> asio::awaitable<void> {
                    co_await runner.run_connection_benchmark();
                    work_guard.reset();
                },
                asio::detached
            );
        }
    }
    else if(mode == "message") {
        // 从文件加载数据
        if(!account_manager.load_from_file()) {
            std::cerr << "错误: 请先执行 setup 模式创建账号和群聊\n";
            std::cerr << "示例: " << argv[0] << " setup --prefix " << config.account_prefix << "\n";
            work_guard.reset();
        } else {
            asio::co_spawn(
                io,
                [&]() -> asio::awaitable<void> {
                    co_await runner.run_message_benchmark();
                    work_guard.reset();
                },
                asio::detached
            );
        }
    }
    else if(mode == "world") {
        if(!account_manager.load_from_file()) {
            std::cerr << "错误: 请先执行 setup 模式创建账号和群聊\n";
            std::cerr << "示例: " << argv[0] << " setup --prefix " << config.account_prefix << "\n";
            work_guard.reset();
        } else {
            asio::co_spawn(
                io,
                [&]() -> asio::awaitable<void> {
                    co_await runner.run_world_benchmark();
                    work_guard.reset();
                },
                asio::detached
            );
        }
    }
    else if(mode == "full") {
        // 从文件加载数据
        if(!account_manager.load_from_file()) {
            std::cerr << "错误: 请先执行 setup 模式创建账号和群聊\n";
            std::cerr << "示例: " << argv[0] << " setup --prefix " << config.account_prefix << "\n";
            work_guard.reset();
        } else {
            asio::co_spawn(
                io,
                [&]() -> asio::awaitable<void> {
                    co_await runner.run_full_benchmark();
                    work_guard.reset();
                },
                asio::detached
            );
        }
    }

    // 等待所有线程完成
    for(auto& t : threads) {
        if(t.joinable()) {
            t.join();
        }
    }

    std::cout << "\n压测结束。\n";
    return 0;
}
