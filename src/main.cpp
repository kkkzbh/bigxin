

#include <stdexec/execution.hpp>
#include <execpools/asio/asio_thread_pool.hpp>
#include <asioexec/use_sender.hpp>
#include <thread>

#include <chrono>


#include <print>

namespace ex = stdexec;

auto main() -> int
{
    auto pool = execpools::asio_thread_pool{ std::thread::hardware_concurrency() };
    using timer = asioexec::asio_impl::system_timer;
    auto executor = pool.get_executor();
    using std::tuple;
    auto [t1,t2] = tuple{ timer{ executor }, timer{ executor } };
    using namespace std::chrono_literals;
    t1.expires_after(1ms);
    t2.expires_after(2ms);
    using asioexec::use_sender;
    using namespace ex;
    auto snd = when_all (
        t1.async_wait(use_sender)
        | then([](auto&&...) {
            std::print(", ");
        }),
        just()
        | then([](auto&&...) {
            std::print("Hello");
        }),
        t2.async_wait(use_sender)
        | then([](auto&&...) {
            std::print("World!\n");
        })
    );
    sync_wait(std::move(snd));
}
