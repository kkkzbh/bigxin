#pragma once

#include <string>
#include <vector>
#include <optional>
#include <boost/asio/awaitable.hpp>
#include <database/types.h>

/// \brief 会话管理相关的数据库操作。
namespace database
{
    /// \brief 获取默认"世界"会话的 ID。
    /// \return conversations 表中 type='GROUP' 且 name='世界' 的会话 ID。
    /// \throws std::runtime_error 当会话不存在时抛出。
    auto get_world_conversation_id() -> boost::asio::awaitable<i64>;

    /// \brief 确保给定两个用户之间存在一个 SINGLE 会话，若不存在则创建。
    /// \param user1 第一个用户 ID。
    /// \param user2 第二个用户 ID。
    /// \return 单聊会话 ID。
    auto get_or_create_single_conversation(i64 user1, i64 user2)
        -> boost::asio::awaitable<i64>;

    /// \brief 创建群聊会话，返回会话 ID（不写入系统消息）。
    /// \param creator_id 群主用户 ID。
    /// \param member_ids 需要加入群聊的用户 ID 列表（不包含自己，函数内部会去重并加入 creator）。
    /// \param name 群名称，空字符串时按默认规则生成。
    /// \return 新建的群聊会话 ID。
    auto create_group_conversation(
        i64 creator_id,
        std::vector<i64> member_ids,
        std::string name
    ) -> boost::asio::awaitable<i64>;

    /// \brief 加载某个用户所属的全部会话（群聊 + 单聊）。
    /// \param user_id 当前用户 ID。
    /// \return 会话列表，包含类型和对当前用户的标题。
    auto load_user_conversations(i64 user_id)
        -> boost::asio::awaitable<std::vector<ConversationInfo>>;

    /// \brief 查询会话成员信息。
    auto get_conversation_member(i64 conversation_id, i64 user_id)
        -> boost::asio::awaitable<std::optional<MemberInfo>>;

    /// \brief 更新会话成员的禁言截止时间（毫秒时间戳，0 表示解禁）。
    auto set_member_mute_until(
        i64 conversation_id,
        i64 user_id,
        i64 muted_until_ms
    ) -> boost::asio::awaitable<void>;

    /// \brief 设置会话成员的角色（OWNER/ADMIN/MEMBER）。
    auto set_member_role(
        i64 conversation_id,
        i64 user_id,
        std::string const& role
    ) -> boost::asio::awaitable<void>;

    /// \brief 加载指定会话的全部成员（含角色与禁言状态）。
    auto load_conversation_members(i64 conversation_id)
        -> boost::asio::awaitable<std::vector<MemberInfo>>;

    /// \brief 移除指定会话中的一名成员（用于主动退群等场景）。
    auto remove_conversation_member(i64 conversation_id, i64 user_id)
        -> boost::asio::awaitable<void>;

    /// \brief 解散会话：删除所有消息与成员关系，并删除会话本身。
    /// \details 当前用于群聊解散逻辑，调用者负责业务校验。
    auto dissolve_conversation(i64 conversation_id)
        -> boost::asio::awaitable<void>;

    /// \brief 查找两个用户之间的单聊会话 ID。
    /// \param user1 第一个用户 ID。
    /// \param user2 第二个用户 ID。
    /// \return 单聊会话 ID，若不存在则返回 nullopt。
    auto find_single_conversation(i64 user1, i64 user2)
        -> boost::asio::awaitable<std::optional<i64>>;

    /// \brief 获取会话类型（GROUP/SINGLE）。
    /// \param conversation_id 会话 ID。
    /// \return 会话类型字符串，若不存在则返回空字符串。
    auto get_conversation_type(i64 conversation_id)
        -> boost::asio::awaitable<std::string>;

    /// \brief 获取单聊会话的对端用户 ID。
    /// \param conversation_id 会话 ID。
    /// \param current_user_id 当前用户 ID。
    /// \return 对端用户 ID，若不存在则返回 -1。
    auto get_single_peer_user_id(i64 conversation_id, i64 current_user_id)
        -> boost::asio::awaitable<i64>;

    /// \brief 更新群聊头像路径。
    /// \param conversation_id 会话 ID。
    /// \param avatar_path 头像文件路径。
    /// \return 是否更新成功。
    auto update_group_avatar(i64 conversation_id, std::string const& avatar_path)
        -> boost::asio::awaitable<bool>;
} // namespace database

