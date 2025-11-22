#pragma once

#include <pqxx/pqxx>
#include <string>

/// \brief 数据库连接和工具函数。
namespace database
{
    /// \brief 创建到 PostgreSQL 的连接。
    /// \details 使用 pg_service.conf 中的 service=chatdb 约定。
    auto make_connection() -> pqxx::connection;

    /// \brief 生成一个随机昵称，例如"微信用户123456"。
    /// \details 使用线程安全的内部随机数引擎。
    auto generate_random_display_name() -> std::string;
} // namespace database
