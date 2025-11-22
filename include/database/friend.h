#pragma once

#include <string>
#include <vector>
#include <database/types.h>

/// \brief 好友关系相关的数据库操作。
namespace database
{
    /// \brief 判断两个用户是否已经互为好友。
    /// \param user_id 当前用户 ID。
    /// \param peer_id 待检查的对端用户 ID。
    /// \return 当 friends 表中存在 (user_id, peer_id) 记录时返回 true。
    auto is_friend(i64 user_id, i64 peer_id) -> bool;

    /// \brief 加载某个用户的好友列表。
    /// \param user_id 当前用户 ID。
    /// \return 好友信息列表。
    auto load_user_friends(i64 user_id) -> std::vector<FriendInfo>;

    /// \brief 按账号搜索用户，并判断是否为当前用户或已是好友。
    /// \param current_user_id 当前登录用户 ID。
    /// \param account 要搜索的账号。
    /// \return 查询结果及好友关系标记。
    auto search_friend_by_account(i64 current_user_id, std::string const& account)
        -> SearchFriendResult;

    /// \brief 创建一条好友申请，若已是好友或存在未处理申请则返回对应错误。
    /// \param from_user_id 申请发起者。
    /// \param to_user_id 申请接收者。
    /// \param source 申请来源说明，例如 search_account。
    /// \param hello_msg 打招呼信息。
    /// \return 操作结果以及申请 ID。
    auto create_friend_request(
        i64 from_user_id,
        i64 to_user_id,
        std::string const& source,
        std::string const& hello_msg
    ) -> FriendRequestResult;

    /// \brief 加载"别人加我"的好友申请列表。
    /// \param user_id 当前用户 ID。
    /// \return 好友申请列表。
    auto load_incoming_friend_requests(i64 user_id) -> std::vector<FriendRequestInfo>;

    /// \brief 同意一条好友申请，并在必要时建立好友关系及单聊会话。
    /// \param request_id 好友申请 ID。
    /// \param current_user_id 当前登录用户 ID（必须为申请接收者）。
    /// \return 操作结果及新好友信息。
    auto accept_friend_request(i64 request_id, i64 current_user_id)
        -> AcceptFriendRequestResult;
} // namespace database
