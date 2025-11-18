
#include <print>
#include <iorunner.h>
#include <utility.h>
#include <server.core.h>

/// \brief 程序入口：启动 IoRunner 和 TCP 服务器，便于用 nc 调试协议。
/// \param argc 命令行参数个数。
/// \param argv 命令行参数数组，argv[1] 可选指定端口。
/// \return 进程退出码，正常情况下为 0。
auto main(int argc, char** argv) -> int
{
    auto port = u16(5555);
    if(argc > 1) {
        port = static_cast<unsigned short>(std::strtoul(argv[1], nullptr, 10));
    }

    auto runner = IoRunner{ 8 };
    start_server(runner.executor(), port);

    std::println("chat server listening on port {}", port);

    runner.run();
}
