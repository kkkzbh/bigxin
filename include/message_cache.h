#pragma once

#include <QString>
#include <QJsonArray>
#include <QHash>

/// \brief 消息缓存管理器，负责本地历史消息的读写。
/// \details 按用户和会话分目录存储 JSON 格式的消息历史。
class MessageCache
{
public:
    MessageCache() = default;

    /// \brief 设置当前用户 ID。
    /// \param userId 用户 ID。
    auto setUserId(QString const& userId) -> void;

    /// \brief 获取当前用户 ID。
    auto userId() const -> QString;

    /// \brief 将一页历史消息写入本地缓存文件（覆盖旧内容）。
    /// \param conversationId 会话 ID。
    /// \param messages 服务端 HISTORY_RESP 中的 messages 数组。
    /// \return 写入的最大 seq，失败时返回 0。
    auto writeMessages(QString const& conversationId, QJsonArray const& messages) -> qint64;

    /// \brief 追加一条消息到本地缓存。
    /// \param conversationId 会话 ID。
    /// \param senderId 发送者 ID。
    /// \param senderDisplayName 发送者显示昵称。
    /// \param content 消息文本。
    /// \param msgType 消息类型（TEXT / SYSTEM 等）。
    /// \param serverTimeMs 服务器时间戳（毫秒）。
    /// \param seq 会话内序号。
    auto appendMessage(
        QString const& conversationId,
        QString const& senderId,
        QString const& senderDisplayName,
        QString const& content,
        QString const& msgType,
        qint64 serverTimeMs,
        qint64 seq
    ) -> void;

    /// \brief 从本地缓存加载指定会话的消息。
    /// \param conversationId 会话 ID。
    /// \return 消息数组和最大 seq 的 pair，失败时返回空数组和 0。
    auto loadMessages(QString const& conversationId) -> QPair<QJsonArray, qint64>;

private:
    auto cacheBasePath() const -> QString;
    auto cacheFilePath(QString const& conversationId) const -> QString;

    QString user_id_;
};
