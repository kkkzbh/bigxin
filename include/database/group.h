#pragma once

#include <string>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <database/types.h>

/// \brief 群聊加入相关的数据库操作。
namespace database
{
    /// \brief 按群聊 ID 搜索群聊信息。
    /// \param current_user_id 当前登录用户 ID。
    /// \param group_id 要搜索的群聊 ID。
    /// \return 查询结果及成员状态标记。
    auto search_group_by_id(i64 current_user_id, i64 group_id)
        -> boost::asio::awaitable<SearchGroupResult>;

    /// \brief 创建一条入群申请，若已是成员或存在未处理申请则返回对应错误。
    /// \param from_user_id 申请发起者。
    /// \param group_id 目标群聊 ID。
    /// \param hello_msg 打招呼信息。
    /// \return 操作结果以及申请 ID。
    auto create_group_join_request(
        i64 from_user_id,
        i64 group_id,
        std::string const& hello_msg
    ) -> boost::asio::awaitable<GroupJoinRequestResult>;

    /// \brief 加载某个用户作为群主/管理员需要处理的入群申请列表。
    /// \param user_id 当前用户 ID。
    /// \return 入群申请列表。
    auto load_group_join_requests_for_admin(i64 user_id)
        -> boost::asio::awaitable<std::vector<GroupJoinRequestInfo>>;

    /// \brief 同意或拒绝一条入群申请。
    /// \param request_id 入群申请 ID。
    /// \param handler_user_id 处理人用户 ID（必须为群主或管理员）。
    /// \param accept 是否同意（true 同意，false 拒绝）。
    /// \return 操作结果。
    auto handle_group_join_request(i64 request_id, i64 handler_user_id, bool accept)
        -> boost::asio::awaitable<AcceptGroupJoinResult>;

    /// \brief 获取群聊的群主和所有管理员的用户 ID 列表。
    /// \param group_id 群聊 ID。
    /// \return 群主和管理员的用户 ID 列表。
    auto get_group_admins(i64 group_id)
        -> boost::asio::awaitable<std::vector<i64>>;
} // namespace database
