#include <database/connection.h>

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/mysql.hpp>

#include <chrono>
#include <mutex>
#include <random>
#include <string>
#include <stdexcept>
#include <vector>

namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace database
{
    namespace
    {
        struct PoolState
        {
            PoolConfig cfg{};
            asio::any_io_executor exec{};
            std::size_t created{ 0 };
            bool initialized{ false };
            std::vector<std::shared_ptr<Connection>> idle;
            std::mutex mutex;
        };

        PoolState& state()
        {
            static PoolState s{};
            return s;
        }

        auto make_connection() -> asio::awaitable<std::shared_ptr<Connection>>
        {
            auto& st = state();
            mysql::handshake_params params{ st.cfg.user, st.cfg.password, st.cfg.database };

            auto conn = std::make_shared<Connection>(st.exec);
            asio::ip::tcp::resolver resolver{ st.exec };
            auto endpoints = co_await resolver.async_resolve(
                st.cfg.host, std::to_string(st.cfg.port), asio::use_awaitable);
            auto ep = endpoints.begin()->endpoint();
            co_await conn->async_connect(ep, params, asio::use_awaitable);
            co_return conn;
        }
    }

    auto set_config(PoolConfig c) -> void
    {
        auto& st = state();
        st.cfg = std::move(c);
    }

    auto init_pool(asio::any_io_executor exec, PoolConfig cfg) -> void
    {
        auto& st = state();
        if(st.initialized) return;
        st.exec = exec;
        st.cfg = std::move(cfg);
        st.created = 0;
        st.initialized = true;
    }

    auto connect(asio::any_io_executor exec) -> asio::awaitable<Connection>
    {
        auto& st = state();
        if(!st.initialized) {
            init_pool(exec, PoolConfig{});
        }
        auto conn_sp = co_await make_connection();
        co_return std::move(*conn_sp);
    }

    auto acquire_connection() -> asio::awaitable<std::shared_ptr<Connection>>
    {
        auto& st = state();
        if(!st.initialized) {
            throw std::runtime_error("pool not initialized");
        }

        // Fast path: reuse idle
        {
            std::lock_guard lock{ st.mutex };
            if(!st.idle.empty()) {
                auto conn = st.idle.back();
                st.idle.pop_back();
                co_return conn;
            }
            if(st.created < st.cfg.pool_size) {
                ++st.created;
                co_return co_await make_connection();
            }
        }
        // Pool saturated, create a temporary connection (could be tuned to wait instead).
        co_return co_await make_connection();
    }

    ConnectionHandle::~ConnectionHandle()
    {
        if(!conn) return;
        auto& st = state();
        if(!st.initialized) return;
        std::lock_guard lock{ st.mutex };
        if(st.idle.size() < st.cfg.pool_size) {
            st.idle.push_back(std::move(conn));
        }
    }

    auto acquire_handle() -> asio::awaitable<ConnectionHandle>
    {
        auto c = co_await acquire_connection();
        co_return ConnectionHandle{ std::move(c) };
    }

    auto generate_random_display_name() -> std::string
    {
        static std::mt19937_64 engine(
            static_cast<std::mt19937_64::result_type>(
                std::chrono::steady_clock::now().time_since_epoch().count()
            )
        );
        static std::mutex mutex;
        std::lock_guard lock{ mutex };

        auto number = std::uniform_int_distribution<int>{ 100000, 999999 }(engine);
        return "微信用户" + std::to_string(number);
    }
} // namespace database
