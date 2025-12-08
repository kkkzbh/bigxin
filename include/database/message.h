#pragma once

#include <string>
#include <vector>
#include <boost/asio/awaitable.hpp>
#include <database/types.h>

/// \brief 消息相关的数据库操作。
namespace database
{
    /// \brief 在指定会话中追加一条文本消息。
    /// \param conversation_id 目标会话 ID。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \return 已写入消息的简要信息。
    auto append_text_message(
        i64 conversation_id,
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type = std::string{ "TEXT" }
    ) -> boost::asio::awaitable<StoredMessage>;

    /// \brief 在"世界"会话中追加一条文本消息。
    /// \param sender_id 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \param msg_type 消息类型，默认 TEXT，可选 SYSTEM。
    /// \return 已写入消息的简要信息。
    auto append_world_text_message(
        i64 sender_id,
        std::string const& content,
        std::string const& msg_type = std::string{ "TEXT" }
    ) -> boost::asio::awaitable<StoredMessage>;

    /// \brief 拉取指定会话的一批历史消息（向上翻旧消息）。
    /// \param conversation_id 会话 ID。
    /// \param before_seq 当为 0 或以下时表示从最新开始，否则拉取 seq 小于该值的消息。
    /// \param limit 最大返回条数，建议为正数。
    /// \return 按 seq 递增排序的消息列表。
    auto load_user_conversation_history(i64 conversation_id, i64 before_seq, i64 limit)
        -> boost::asio::awaitable<std::vector<LoadedMessage>>;

    /// \brief 拉取指定会话中 seq 大于给定值的一批新消息（用于增量同步）。
    /// \param conversation_id 会话 ID。
    /// \param after_seq 仅返回 seq 大于该值的消息。
    /// \param limit 最大返回条数，建议为正数。
    /// \return 按 seq 递增排序的消息列表。
    auto load_user_conversation_since(i64 conversation_id, i64 after_seq, i64 limit)
        -> boost::asio::awaitable<std::vector<LoadedMessage>>;

    /// \brief 拉取"世界"会话的一批历史消息。
    /// \param before_seq 当为 0 或以下时表示从最新开始，否则拉取 seq 小于该值的消息。
    /// \param limit 最大返回条数，建议为正数。
    /// \return 按 seq 递增排序的消息列表。
    auto load_world_history(i64 before_seq, i64 limit)
        -> boost::asio::awaitable<std::vector<LoadedMessage>>;

    /// \brief 撤回指定消息 (设置 is_recalled 标记)。
    /// \param message_id 消息 ID。
    /// \param recaller_id 撤回者用户 ID。
    /// \return 撤回操作结果。
    auto recall_message(i64 message_id, i64 recaller_id)
        -> boost::asio::awaitable<RecallMessageResult>;

    /// \brief 添加消息反应 (点赞/踩)。
    /// \param message_id 消息 ID。
    /// \param user_id 用户 ID。
    /// \param reaction_type 反应类型 ("LIKE" / "DISLIKE")。
    /// \return 操作结果，包含该消息的所有反应列表。
    auto add_message_reaction(i64 message_id, i64 user_id, std::string const& reaction_type)
        -> boost::asio::awaitable<MessageReactionResult>;

    /// \brief 移除消息反应。
    /// \param message_id 消息 ID。
    /// \param user_id 用户 ID。
    /// \param reaction_type 反应类型 ("LIKE" / "DISLIKE")。
    /// \return 操作结果，包含该消息的所有反应列表。
    auto remove_message_reaction(i64 message_id, i64 user_id, std::string const& reaction_type)
        -> boost::asio::awaitable<MessageReactionResult>;

    /// \brief 获取指定消息的所有反应。
    /// \param message_id 消息 ID。
    /// \return 反应列表。
    auto get_message_reactions(i64 message_id)
        -> boost::asio::awaitable<std::vector<MessageReaction>>;
} // namespace database
