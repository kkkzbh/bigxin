#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDir>

class NetworkManager;
class MessageCache;
class ProtocolHandler;

/// \brief 登录 / 注册相关的前端网络适配层（轻量门面）。
/// \details 保留 Q_PROPERTY 和 Q_INVOKABLE 接口以维持 QML 兼容性，
///          内部委托给 NetworkManager、ProtocolHandler 和 MessageCache。
class LoginBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString avatarPath READ avatarPath NOTIFY avatarPathChanged)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)

public:
    explicit LoginBackend(QObject* parent = nullptr);
    ~LoginBackend() override;

    auto busy() const -> bool;
    auto errorMessage() const -> QString;
    auto userId() const -> QString;
    auto displayName() const -> QString;
    auto avatarPath() const -> QString;
    auto avatarUrl() const -> QUrl;
    auto worldConversationId() const -> QString;

    Q_INVOKABLE QUrl resolveAvatarUrl(QString const& path);

    Q_INVOKABLE void login(QString const& account, QString const& password);
    Q_INVOKABLE void registerAccount(QString const& account, QString const& password, QString const& confirmPassword);
    Q_INVOKABLE void updateDisplayName(QString const& newName);
    Q_INVOKABLE void updateAvatar(QString const& avatarPath);
    Q_INVOKABLE void updateGroupAvatar(QString const& conversationId, QString const& avatarPath);
    Q_INVOKABLE void clearError();

    Q_INVOKABLE void sendWorldTextMessage(QString const& text);
    Q_INVOKABLE void sendMessage(QString const& conversationId, QString const& text);
    Q_INVOKABLE void requestInitialWorldHistory();
    Q_INVOKABLE void requestConversationList();
    Q_INVOKABLE void leaveConversation(QString const& conversationId);
    Q_INVOKABLE void requestConversationMembers(QString const& conversationId);
    Q_INVOKABLE void requestFriendList();
    Q_INVOKABLE void requestFriendRequestList();
    Q_INVOKABLE void searchFriendByAccount(QString const& account);
    Q_INVOKABLE void sendFriendRequest(QString const& peerUserId, QString const& helloMsg);
    Q_INVOKABLE void createGroupConversation(QStringList const& memberUserIds, QString const& name);
    Q_INVOKABLE void acceptFriendRequest(QString const& requestId);
    Q_INVOKABLE void rejectFriendRequest(QString const& requestId);
    Q_INVOKABLE void deleteFriend(QString const& friendUserId);
    Q_INVOKABLE void openSingleConversation(QString const& peerUserId);
    Q_INVOKABLE void muteMember(QString const& conversationId, QString const& targetUserId, qint64 durationSeconds);
    Q_INVOKABLE void unmuteMember(QString const& conversationId, QString const& targetUserId);
    Q_INVOKABLE void setAdmin(QString const& conversationId, QString const& targetUserId, bool isAdmin);
    Q_INVOKABLE void requestHistory(QString const& conversationId);
    Q_INVOKABLE void openConversation(QString const& conversationId);
    Q_INVOKABLE void searchGroupById(QString const& groupId);
    Q_INVOKABLE void sendGroupJoinRequest(QString const& groupId, QString const& helloMsg);
    Q_INVOKABLE void requestGroupJoinRequestList();
    Q_INVOKABLE void acceptGroupJoinRequest(QString const& requestId, bool accept = true);
    Q_INVOKABLE void renameGroup(QString const& conversationId, QString const& newName);

signals:
    void busyChanged();
    void errorMessageChanged();
    void userIdChanged();
    void displayNameChanged();
    void avatarPathChanged();
    void avatarUrlChanged();
    void loginSucceeded();
    void registrationSucceeded(QString account);

    void messageReceived(
        QString conversationId,
        QString senderId,
        QString senderDisplayName,
        QString content,
        QString msgType,
        qint64 serverTimeMs,
        qint64 seq
    );

    void conversationsReset(QVariantList conversations);
    void conversationMembersReady(QString conversationId, QVariantList members);
    void friendsReset(QVariantList friends);
    void friendRequestsReset(QVariantList requests);
    void groupJoinRequestsReset(QVariantList requests);
    void friendSearchFinished(QVariantMap result);
    void groupSearchFinished(QVariantMap result);
    void friendRequestSucceeded();
    void groupJoinRequestSucceeded();
    void singleConversationReady(QString conversationId, QString conversationType);
    void conversationOpened(QString conversationId);
    void groupCreated(QString conversationId, QString title);
    void messageSendFailed(QString conversationId, QString errorMessage);

private slots:
    void onNetworkConnected();
    void onNetworkError(QString errorMessage);
    void onProtocolError(QString errorMessage);
    void onLoginSucceeded(QString userId, QString displayName, QString avatarPath, QString worldConversationId);
    void onDisplayNameUpdated(QString displayName);
    void onAvatarUpdated(QString avatarPath);
    void onNetworkDisconnected();

private:
    void setBusy(bool value);
    void setErrorMessage(QString const& msg);
    void sendCurrentCommand();

    enum class PendingCommand
    {
        None,
        Login,
        Register
    };

    NetworkManager* network_manager_;
    MessageCache* message_cache_;
    ProtocolHandler* protocol_handler_;

    bool busy_{ false };
    QString error_message_;
    QString user_id_;
    QString display_name_;
    QString avatar_path_;
    QString world_conversation_id_;

    PendingCommand pending_command_{ PendingCommand::None };
    QString pending_account_;
    QString pending_password_;
    QString pending_confirm_;

    QString host_{ QStringLiteral("127.0.0.1") };
    quint16 port_{ 5555 };
};
