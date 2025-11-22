#pragma once

#include <string>
#include <utility.h>

/// \brief 与数据库相关的数据类型定义。
namespace database
{
    /// \brief 用户基础信息，用于登录 / 注册结果返回。
    struct UserInfo
    {
        /// \brief 数据库中的主键 ID。
        i64 id{};
        /// \brief 登录账号。
        std::string account{};
        /// \brief 显示昵称。
        std::string display_name{};
    };

    /// \brief 注册结果。
    struct RegisterResult
    {
        /// \brief 是否注册成功。
        bool ok{};
        /// \brief 错误码，失败时有效。
        std::string error_code{};
        /// \brief 错误信息，失败时有效。
        std::string error_msg{};
        /// \brief 成功时的用户信息。
        UserInfo user{};
    };

    /// \brief 更新昵称结果。
    struct UpdateDisplayNameResult
    {
        /// \brief 是否更新成功。
        bool ok{};
        /// \brief 错误码，失败时有效。
        std::string error_code{};
        /// \brief 错误信息，失败时有效。
        std::string error_msg{};
        /// \brief 成功时的用户信息。
        UserInfo user{};
    };

    /// \brief 登录结果。
    struct LoginResult
    {
        /// \brief 是否登录成功。
        bool ok{};
        /// \brief 错误码，失败时有效。
        std::string error_code{};
        /// \brief 错误信息，失败时有效。
        std::string error_msg{};
        /// \brief 成功时的用户信息。
        UserInfo user{};
    };

    /// \brief 好友基础信息，用于通讯录联系人列表展示。
    struct FriendInfo
    {
        /// \brief 好友用户 ID。
        i64 id{};
        /// \brief 好友账号。
        std::string account{};
        /// \brief 好友昵称。
        std::string display_name{};
    };

    /// \brief 好友申请信息，用于"新的朋友"列表展示。
    struct FriendRequestInfo
    {
        /// \brief 申请 ID。
        i64 id{};
        /// \brief 申请发起者用户 ID。
        i64 from_user_id{};
        /// \brief 申请发起者账号。
        std::string account{};
        /// \brief 申请发起者昵称。
        std::string display_name{};
        /// \brief 当前状态：PENDING / ACCEPTED 等。
        std::string status{};
        /// \brief 打招呼信息或来源文案。
        std::string hello_msg{};
    };

    /// \brief 按账号搜索好友的结果。
    struct SearchFriendResult
    {
        /// \brief 是否查询成功。
        bool ok{};
        /// \brief 错误码（例如 NOT_FOUND）。
        std::string error_code{};
        /// \brief 错误信息。
        std::string error_msg{};
        /// \brief 是否找到对应账号。
        bool found{};
        /// \brief 是否为当前用户自己。
        bool is_self{};
        /// \brief 是否已经是好友。
        bool is_friend{};
        /// \brief 当 found 为 true 时的用户信息。
        UserInfo user{};
    };

    /// \brief 创建好友申请的结果。
    struct FriendRequestResult
    {
        /// \brief 是否操作成功。
        bool ok{};
        /// \brief 错误码，例如 ALREADY_FRIEND。
        std::string error_code{};
        /// \brief 错误信息。
        std::string error_msg{};
        /// \brief 成功时的申请 ID。
        i64 request_id{};
    };

    /// \brief 同意好友申请的结果。
    struct AcceptFriendRequestResult
    {
        /// \brief 是否操作成功。
        bool ok{};
        /// \brief 错误码。
        std::string error_code{};
        /// \brief 错误信息。
        std::string error_msg{};
        /// \brief 新增好友的基础信息。
        UserInfo friend_user{};
        /// \brief 为这对好友建立 / 复用的单聊会话 ID（如无则为 0）。
        i64 conversation_id{};
    };

    /// \brief 会话基础信息，用于会话列表展示。
    struct ConversationInfo
    {
        /// \brief 会话 ID。
        i64 id{};
        /// \brief 会话类型：GROUP / SINGLE。
        std::string type{};
        /// \brief 对当前用户的显示标题。
        std::string title{};
        /// \brief 当前会话最新消息的 seq，若尚无消息则为 0。
        i64 last_seq{};
        /// \brief 当前会话最新消息的服务器时间戳（毫秒），若尚无消息则为 0。
        i64 last_server_time_ms{};
    };

    /// \brief 会话成员信息。
    struct MemberInfo
    {
        /// \brief 成员用户 ID。
        i64 user_id{};
        /// \brief 显示昵称。
        std::string display_name{};
        /// \brief 角色：OWNER / MEMBER。
        std::string role{};
        /// \brief 禁言截止毫秒时间戳，0 表示未禁言。
        i64 muted_until_ms{};
    };

    /// \brief 已持久化消息的简要信息。
    struct StoredMessage
    {
        /// \brief 所属会话 ID。
        i64 conversation_id{};
        /// \brief 消息主键 ID，作为 serverMsgId。
        i64 id{};
        /// \brief 会话内递增序号。
        i64 seq{};
        /// \brief 服务器时间戳（毫秒）。
        i64 server_time_ms{};
        /// \brief 消息类型。
        std::string msg_type{};
    };

    /// \brief 已加载消息的完整信息，用于构建历史消息响应。
    struct LoadedMessage
    {
        /// \brief 消息主键 ID。
        i64 id{};
        /// \brief 所属会话 ID。
        i64 conversation_id{};
        /// \brief 发送者用户 ID。
        i64 sender_id{};
        /// \brief 发送者显示昵称。
        std::string sender_display_name{};
        /// \brief 会话内递增序号。
        i64 seq{};
        /// \brief 消息类型，例如 TEXT。
        std::string msg_type{};
        /// \brief 消息内容。
        std::string content{};
        /// \brief 服务器时间戳（毫秒）。
        i64 server_time_ms{};
    };
} // namespace database
