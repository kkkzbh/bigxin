
#include <print>
#include <charconv>
#include <cstring>
#include <thread>

#include <stdexec/execution.hpp>
#include <execpools/asio/asio_thread_pool.hpp>
#include <asioexec/use_sender.hpp>

#include <utility.h>
#include <session.h>
#include <server.h>
#include <database/connection.h>

/// \brief 程序入口：启动 IoRunner 和 TCP 服务器，便于用 nc 调试协议。
/// \param argc 命令行参数个数。
/// \param argv 命令行参数数组，argv[1] 可选指定端口。
/// \return 进程退出码，正常情况下为 0。
auto main(int argc, char** argv) -> int
{
    auto port = u16(5555);
    if(argc > 1) {
        auto _ = std::from_chars(argv[1], argv[1] + std::strlen(argv[1]), port, 10);
    }

    auto thread_count = std::thread::hardware_concurrency();
    auto pool = execpools::asio_thread_pool{ thread_count };
    auto exec = pool.get_executor();

    database::init_pool(exec);
    std::println("chat server listening on port {}, thread_count is {}", port, thread_count);

    // 使用 stdexec sender 模型启动并同步等待服务器协程结束
    auto server_sender = async_start_server(exec, port);
    stdexec::sync_wait(server_sender);

    return 0;
}
