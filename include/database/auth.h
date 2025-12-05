#pragma once

#include <string>
#include <boost/asio/awaitable.hpp>
#include <database/types.h>

/// \brief 用户认证相关的数据库操作。
namespace database
{
    /// \brief 尝试注册新用户。
    /// \param account 登录账号，需唯一。
    /// \param password 密码，目前直接存储明文，后续可替换为哈希。
    /// \return 包含是否成功、错误信息以及成功时的用户信息。
    auto register_user(std::string const& account, std::string const& password)
        -> boost::asio::awaitable<RegisterResult>;

    /// \brief 更新指定用户的显示昵称。
    /// \param user_id 目标用户 ID。
    /// \param new_name 新的昵称。
    /// \return 更新结果，包含错误信息或最新的用户信息。
    auto update_display_name(i64 user_id, std::string const& new_name)
        -> boost::asio::awaitable<UpdateDisplayNameResult>;

    /// \brief 更新指定用户的头像路径。
    /// \param user_id 目标用户 ID。
    /// \param avatar_path 新的头像路径。
    /// \return 更新结果，包含错误信息或最新的用户信息。
    auto update_avatar(i64 user_id, std::string const& avatar_path)
        -> boost::asio::awaitable<UpdateDisplayNameResult>;

    /// \brief 尝试登录用户。
    /// \param account 登录账号。
    /// \param password 密码，当前为明文比较。
    /// \return 登录结果，包含错误信息或用户信息。
    auto login_user(std::string const& account, std::string const& password)
        -> boost::asio::awaitable<LoginResult>;
} // namespace database

