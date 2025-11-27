#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

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

public:
    explicit LoginBackend(QObject* parent = nullptr);
    ~LoginBackend() override;

    auto busy() const -> bool;
    auto errorMessage() const -> QString;
    auto userId() const -> QString;
    auto displayName() const -> QString;
    auto worldConversationId() const -> QString;

    Q_INVOKABLE void login(QString const& account, QString const& password);
    Q_INVOKABLE void registerAccount(QString const& account, QString const& password, QString const& confirmPassword);
    Q_INVOKABLE void updateDisplayName(QString const& newName);
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
    Q_INVOKABLE void openSingleConversation(QString const& peerUserId);
    Q_INVOKABLE void muteMember(QString const& conversationId, QString const& targetUserId, qint64 durationSeconds);
    Q_INVOKABLE void unmuteMember(QString const& conversationId, QString const& targetUserId);
    Q_INVOKABLE void requestHistory(QString const& conversationId);
    Q_INVOKABLE void openConversation(QString const& conversationId);

signals:
    void busyChanged();
    void errorMessageChanged();
    void userIdChanged();
    void displayNameChanged();
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
    void friendSearchFinished(QVariantMap result);
    void friendRequestSucceeded();
    void singleConversationReady(QString conversationId, QString conversationType);
    void groupCreated(QString conversationId, QString title);

private slots:
    void onNetworkConnected();
    void onNetworkError(QString errorMessage);
    void onProtocolError(QString errorMessage);
    void onLoginSucceeded(QString userId, QString displayName, QString worldConversationId);
    void onDisplayNameUpdated(QString displayName);
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
    QString world_conversation_id_;

    PendingCommand pending_command_{ PendingCommand::None };
    QString pending_account_;
    QString pending_password_;
    QString pending_confirm_;

    QString host_{ QStringLiteral("127.0.0.1") };
    quint16 port_{ 5555 };
};
