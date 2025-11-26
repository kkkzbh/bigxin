#pragma once

#include <boost/mysql.hpp>
#include <boost/asio.hpp>
#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>

/// \brief 数据库连接和工具函数。
namespace database
{
    using Connection = boost::mysql::tcp_connection;

    struct PoolConfig {
        std::string host = "127.0.0.1";
        std::uint16_t port = 3307;
        std::string user = "kkkzbh";
        std::string password = "kkkzbh";
        std::string database = "chatdb";
        std::size_t pool_size = 8;
    };

    /// \brief 初始化全局连接配置（可选）。
    auto set_config(PoolConfig cfg) -> void;

    /// \brief 初始化连接池（幂等）。
    auto init_pool(boost::asio::any_io_executor exec, PoolConfig cfg = {}) -> void;

    /// \brief 建立一个已连接的 MySQL 连接。
    auto connect(boost::asio::any_io_executor exec) -> boost::asio::awaitable<Connection>;

    /// \brief 从连接池获取连接（RAII 归还由调用方显式 co_await async_send）。
    auto acquire_connection() -> boost::asio::awaitable<std::shared_ptr<Connection>>;

    /// \brief RAII 归还连接的句柄。
    struct ConnectionHandle {
        std::shared_ptr<Connection> conn{};
        ConnectionHandle() = default;
        explicit ConnectionHandle(std::shared_ptr<Connection> c) : conn(std::move(c)) {}
        ConnectionHandle(ConnectionHandle&&) noexcept = default;
        ConnectionHandle& operator=(ConnectionHandle&&) noexcept = default;
        ~ConnectionHandle();
        Connection& operator*() const { return *conn; }
        Connection* operator->() const { return conn.get(); }
        explicit operator bool() const { return static_cast<bool>(conn); }
    };

    auto acquire_handle() -> boost::asio::awaitable<ConnectionHandle>;

    /// \brief 生成一个随机昵称，例如"微信用户123456"。
    /// \details 使用线程安全的内部随机数引擎。
    auto generate_random_display_name() -> std::string;
} // namespace database
