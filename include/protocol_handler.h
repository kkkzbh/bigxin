#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>

class NetworkManager;
class MessageCache;

/// \brief 协议处理器，负责处理所有服务器响应和推送。
/// \details 接收 NetworkManager 的命令，分发给各个专项处理函数，并通过信号通知上层。
class ProtocolHandler : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolHandler(NetworkManager* networkManager, MessageCache* messageCache, QObject* parent = nullptr);

    /// \brief 设置当前用户 ID。
    auto setUserId(QString const& userId) -> void;

    /// \brief 获取当前用户 ID。
    auto userId() const -> QString;

    /// \brief 设置当前用户显示昵称。
    auto setDisplayName(QString const& displayName) -> void;

    /// \brief 获取当前用户显示昵称。
    auto displayName() const -> QString;

    /// \brief 设置世界会话 ID。
    auto setWorldConversationId(QString const& worldConversationId) -> void;

    /// \brief 获取世界会话 ID。
    auto worldConversationId() const -> QString;

    /// \brief 获取指定会话的本地最新 seq。
    auto localLastSeq(QString const& conversationId) const -> qint64;

    /// \brief 获取指定会话的服务器端最新 seq。
    auto serverLastSeq(QString const& conversationId) const -> qint64;

    /// \brief 从本地缓存加载会话消息。
    /// \param conversationId 会话 ID。
    /// \return 是否成功加载。
    auto loadConversationCache(QString const& conversationId) -> bool;

signals:
    /// \brief 登录成功。
    /// \param userId 用户 ID。
    /// \param displayName 用户显示昵称。
    /// \param avatarPath 用户头像路径。
    /// \param worldConversationId 世界会话 ID。
    void loginSucceeded(QString userId, QString displayName, QString avatarPath, QString worldConversationId);

    /// \brief 注册成功。
    /// \param account 注册的账号。
    void registrationSucceeded(QString account);

    /// \brief 昵称更新成功。
    /// \param displayName 新的显示昵称。
    void displayNameUpdated(QString displayName);

    /// \brief 头像更新成功。
    /// \param avatarPath 新的头像路径。
    void avatarUpdated(QString avatarPath);

    /// \brief 收到服务器推送的聊天消息。
    void messageReceived(
        QString conversationId,
        QString senderId,
        QString senderDisplayName,
        QString content,
        QString msgType,
        qint64 serverTimeMs,
        qint64 seq
    );

    /// \brief 会话列表就绪。
    void conversationsReset(QVariantList conversations);

    /// \brief 会话成员列表就绪。
    void conversationMembersReady(QString conversationId, QVariantList members);

    /// \brief 好友列表就绪。
    void friendsReset(QVariantList friends);

    /// \brief 好友申请列表就绪。
    void friendRequestsReset(QVariantList requests);

    /// \brief 入群申请列表就绪（给群主/管理员看的）。
    void groupJoinRequestsReset(QVariantList requests);

    /// \brief 好友搜索完成。
    void friendSearchFinished(QVariantMap result);

    /// \brief 好友申请发送成功。
    void friendRequestSucceeded();

    /// \brief 单聊会话就绪。
    void singleConversationReady(QString conversationId, QString conversationType);

    /// \brief 群聊创建成功。
    void groupCreated(QString conversationId, QString title);

    /// \brief 群聊搜索完成。
    void groupSearchFinished(QVariantMap result);

    /// \brief 入群申请发送成功。
    void groupJoinRequestSucceeded();

    /// \brief 处理失败，发出错误信息。
    void errorOccurred(QString errorMessage);

    /// \brief 需要请求会话列表。
    void needRequestConversationList();

    /// \brief 需要请求会话成员。
    void needRequestConversationMembers(QString conversationId);

    /// \brief 需要请求好友申请列表。
    void needRequestFriendRequestList();

    /// \brief 需要请求好友列表。
    void needRequestFriendList();

    /// \brief 需要请求入群申请列表。
    void needRequestGroupJoinRequestList();

private slots:
    void handleCommand(QString command, QJsonObject payload);

private:
    void handleLoginResponse(QJsonObject const& obj);
    void handleRegisterResponse(QJsonObject const& obj);
    void handleProfileUpdateResponse(QJsonObject const& obj);
    void handleAvatarUpdateResponse(QJsonObject const& obj);
    void handleMessagePush(QJsonObject const& obj);
    void handleHistoryResponse(QJsonObject const& obj);
    void handleConversationListResponse(QJsonObject const& obj);
    void handleConversationMembersResponse(QJsonObject const& obj);
    void handleLeaveConversationResponse(QJsonObject const& obj);
    void handleMuteMemberResponse(QJsonObject const& obj);
    void handleUnmuteMemberResponse(QJsonObject const& obj);
    void handleSetAdminResponse(QJsonObject const& obj);
    void handleErrorResponse(QJsonObject const& obj);
    void handleFriendListResponse(QJsonObject const& obj);
    void handleFriendRequestListResponse(QJsonObject const& obj);
    void handleFriendSearchResponse(QJsonObject const& obj);
    void handleFriendAddResponse(QJsonObject const& obj);
    void handleFriendAcceptResponse(QJsonObject const& obj);
    void handleFriendRejectResponse(QJsonObject const& obj);
    void handleOpenSingleConvResponse(QJsonObject const& obj);
    void handleCreateGroupResponse(QJsonObject const& obj);
    void handleGroupSearchResponse(QJsonObject const& obj);
    void handleGroupJoinResponse(QJsonObject const& obj);
    void handleGroupJoinRequestListResponse(QJsonObject const& obj);
    void handleGroupJoinAcceptResponse(QJsonObject const& obj);

    NetworkManager* network_manager_;
    MessageCache* message_cache_;

    QString user_id_;
    QString display_name_;
    QString avatar_path_;
    QString world_conversation_id_;
    QString pending_account_;

    /// \brief 每个会话在服务器端已知的最新 seq。
    QHash<QString, qint64> conv_last_seq_;
    /// \brief 本地缓存中每个会话的最新 seq。
    QHash<QString, qint64> local_last_seq_;
};
