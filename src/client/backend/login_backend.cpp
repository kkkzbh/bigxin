#include "login.backend.h"
#include "network_manager.h"
#include "message_cache.h"
#include "protocol_handler.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

LoginBackend::LoginBackend(QObject* parent)
    : QObject(parent)
    , network_manager_(new NetworkManager(this))
    , message_cache_(new MessageCache())
    , protocol_handler_(new ProtocolHandler(network_manager_, message_cache_, this))
{
    // 连接网络管理器信号
    connect(network_manager_, &NetworkManager::connected, this, &LoginBackend::onNetworkConnected);
    connect(network_manager_, &NetworkManager::disconnected, this, &LoginBackend::onNetworkDisconnected);
    connect(network_manager_, &NetworkManager::errorOccurred, this, &LoginBackend::onNetworkError);

    // 连接协议处理器信号
    connect(protocol_handler_, &ProtocolHandler::loginSucceeded, this, &LoginBackend::onLoginSucceeded);
    connect(protocol_handler_, &ProtocolHandler::registrationSucceeded, this, [this](QString) {
        setBusy(false);
        pending_command_ = PendingCommand::None;
        emit registrationSucceeded(pending_account_);
    });
    connect(protocol_handler_, &ProtocolHandler::displayNameUpdated, this, &LoginBackend::onDisplayNameUpdated);
    connect(protocol_handler_, &ProtocolHandler::errorOccurred, this, &LoginBackend::onProtocolError);

    // 直接转发的信号
    connect(protocol_handler_, &ProtocolHandler::messageReceived, this, &LoginBackend::messageReceived);
    connect(protocol_handler_, &ProtocolHandler::conversationsReset, this, &LoginBackend::conversationsReset);
    connect(protocol_handler_, &ProtocolHandler::conversationMembersReady, this, &LoginBackend::conversationMembersReady);
    connect(protocol_handler_, &ProtocolHandler::friendsReset, this, &LoginBackend::friendsReset);
    connect(protocol_handler_, &ProtocolHandler::friendRequestsReset, this, &LoginBackend::friendRequestsReset);
    connect(protocol_handler_, &ProtocolHandler::friendSearchFinished, this, &LoginBackend::friendSearchFinished);
    connect(protocol_handler_, &ProtocolHandler::friendRequestSucceeded, this, &LoginBackend::friendRequestSucceeded);
    connect(protocol_handler_, &ProtocolHandler::singleConversationReady, this, &LoginBackend::singleConversationReady);
    connect(protocol_handler_, &ProtocolHandler::groupCreated, this, &LoginBackend::groupCreated);

    // 协议处理器请求的操作
    connect(protocol_handler_, &ProtocolHandler::needRequestConversationList, this, &LoginBackend::requestConversationList);
    connect(protocol_handler_, &ProtocolHandler::needRequestConversationMembers, this, &LoginBackend::requestConversationMembers);
    connect(protocol_handler_, &ProtocolHandler::needRequestFriendRequestList, this, &LoginBackend::requestFriendRequestList);
    connect(protocol_handler_, &ProtocolHandler::needRequestFriendList, this, &LoginBackend::requestFriendList);
}

LoginBackend::~LoginBackend()
{
    delete message_cache_;
}

auto LoginBackend::busy() const -> bool
{
    return busy_;
}

auto LoginBackend::errorMessage() const -> QString
{
    return error_message_;
}

auto LoginBackend::userId() const -> QString
{
    return user_id_;
}

auto LoginBackend::displayName() const -> QString
{
    return display_name_;
}

auto LoginBackend::worldConversationId() const -> QString
{
    return world_conversation_id_;
}

void LoginBackend::login(QString const& account, QString const& password)
{
    if(account.isEmpty() || password.isEmpty()) {
        setErrorMessage(QStringLiteral("账号和密码不能为空"));
        return;
    }
    if(busy_) {
        return;
    }

    pending_command_ = PendingCommand::Login;
    pending_account_ = account;
    pending_password_ = password;
    pending_confirm_.clear();

    setErrorMessage(QString{});
    setBusy(true);
    if(network_manager_->isConnected()) {
        // 已经有连接，直接发送登录命令，避免 busy 卡死
        sendCurrentCommand();
    } else {
        network_manager_->connectToServer(host_, port_);
    }
}

void LoginBackend::registerAccount(QString const& account, QString const& password, QString const& confirmPassword)
{
    if(account.isEmpty() || password.isEmpty() || confirmPassword.isEmpty()) {
        setErrorMessage(QStringLiteral("账号和密码不能为空"));
        return;
    }
    if(password != confirmPassword) {
        setErrorMessage(QStringLiteral("两次输入的密码不一致"));
        return;
    }
    if(busy_) {
        return;
    }

    pending_command_ = PendingCommand::Register;
    pending_account_ = account;
    pending_password_ = password;
    pending_confirm_ = confirmPassword;

    setErrorMessage(QString{});
    setBusy(true);
    if(network_manager_->isConnected()) {
        // 重用现有连接直接发送注册命令
        sendCurrentCommand();
    } else {
        network_manager_->connectToServer(host_, port_);
    }
}

void LoginBackend::updateDisplayName(QString const& newName)
{
    auto const trimmed = newName.trimmed();
    if(trimmed.isEmpty()) {
        setErrorMessage(QStringLiteral("昵称不能为空"));
        return;
    }
    if(user_id_.isEmpty()) {
        setErrorMessage(QStringLiteral("请先登录后再修改昵称"));
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("displayName"), trimmed);
    network_manager_->sendCommand(QStringLiteral("PROFILE_UPDATE"), obj);
}

void LoginBackend::clearError()
{
    setErrorMessage(QString{});
}

void LoginBackend::sendWorldTextMessage(QString const& text)
{
    sendMessage(world_conversation_id_, text);
}

void LoginBackend::sendMessage(QString const& conversationId, QString const& text)
{
    if(text.isEmpty()) {
        return;
    }
    if(user_id_.isEmpty()) {
        setErrorMessage(QStringLiteral("请先登录后再发送消息"));
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }
    if(conversationId.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择会话"));
        return;
    }

    QJsonObject obj;
    auto const client_id = QString::number(QDateTime::currentMSecsSinceEpoch());

    obj.insert(QStringLiteral("conversationId"), conversationId);
    obj.insert(QStringLiteral("conversationType"), QStringLiteral("GROUP"));
    obj.insert(QStringLiteral("senderId"), user_id_);
    obj.insert(QStringLiteral("clientMsgId"), client_id);
    obj.insert(QStringLiteral("msgType"), QStringLiteral("TEXT"));
    obj.insert(QStringLiteral("content"), text);

    network_manager_->sendCommand(QStringLiteral("SEND_MSG"), obj);
}

void LoginBackend::requestInitialWorldHistory()
{
    if(world_conversation_id_.isEmpty()) {
        return;
    }
    requestHistory(world_conversation_id_);
}

void LoginBackend::requestConversationList()
{
    if(!network_manager_->isConnected()) {
        return;
    }

    network_manager_->sendCommand(QStringLiteral("CONV_LIST_REQ"), QJsonObject{});
}

void LoginBackend::leaveConversation(QString const& conversationId)
{
    if(conversationId.isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversationId"), conversationId);
    network_manager_->sendCommand(QStringLiteral("LEAVE_CONV_REQ"), obj);
}

void LoginBackend::requestConversationMembers(QString const& conversationId)
{
    if(!network_manager_->isConnected()) {
        return;
    }
    if(conversationId.isEmpty()) {
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversationId"), conversationId);
    network_manager_->sendCommand(QStringLiteral("CONV_MEMBERS_REQ"), obj);
}

void LoginBackend::requestFriendList()
{
    if(!network_manager_->isConnected()) {
        return;
    }

    network_manager_->sendCommand(QStringLiteral("FRIEND_LIST_REQ"), QJsonObject{});
}

void LoginBackend::requestFriendRequestList()
{
    if(!network_manager_->isConnected()) {
        return;
    }

    network_manager_->sendCommand(QStringLiteral("FRIEND_REQ_LIST_REQ"), QJsonObject{});
}

void LoginBackend::searchFriendByAccount(QString const& account)
{
    auto const trimmed = account.trimmed();
    if(trimmed.isEmpty()) {
        QVariantMap result;
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("errorCode"), QStringLiteral("INVALID_PARAM"));
        result.insert(QStringLiteral("errorMsg"), QStringLiteral("账号不能为空"));
        emit friendSearchFinished(result);
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("account"), trimmed);
    network_manager_->sendCommand(QStringLiteral("FRIEND_SEARCH_REQ"), obj);
}

void LoginBackend::sendFriendRequest(QString const& peerUserId, QString const& helloMsg)
{
    if(peerUserId.trimmed().isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("peerUserId"), peerUserId.trimmed());
    obj.insert(QStringLiteral("source"), QStringLiteral("search_account"));
    if(!helloMsg.trimmed().isEmpty()) {
        obj.insert(QStringLiteral("helloMsg"), helloMsg.trimmed());
    }

    network_manager_->sendCommand(QStringLiteral("FRIEND_ADD_REQ"), obj);
}

void LoginBackend::createGroupConversation(QStringList const& memberUserIds, QString const& name)
{
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }
    if(memberUserIds.size() < 2) {
        setErrorMessage(QStringLiteral("请至少选择两位联系人"));
        return;
    }

    QJsonObject obj;
    QJsonArray arr;
    for(auto const& id : memberUserIds) {
        auto trimmed = id.trimmed();
        if(!trimmed.isEmpty()) {
            arr.append(trimmed);
        }
    }
    obj.insert(QStringLiteral("memberUserIds"), arr);
    if(!name.trimmed().isEmpty()) {
        obj.insert(QStringLiteral("name"), name.trimmed());
    }

    network_manager_->sendCommand(QStringLiteral("CREATE_GROUP_REQ"), obj);
}

void LoginBackend::acceptFriendRequest(QString const& requestId)
{
    if(requestId.trimmed().isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("requestId"), requestId.trimmed());
    network_manager_->sendCommand(QStringLiteral("FRIEND_ACCEPT_REQ"), obj);
}

void LoginBackend::openSingleConversation(QString const& peerUserId)
{
    if(peerUserId.trimmed().isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("peerUserId"), peerUserId.trimmed());
    network_manager_->sendCommand(QStringLiteral("OPEN_SINGLE_CONV_REQ"), obj);
}

void LoginBackend::muteMember(QString const& conversationId, QString const& targetUserId, qint64 durationSeconds)
{
    if(conversationId.isEmpty() || targetUserId.isEmpty()) {
        return;
    }
    if(durationSeconds <= 0) {
        setErrorMessage(QStringLiteral("禁言时长必须大于 0"));
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversationId"), conversationId);
    obj.insert(QStringLiteral("targetUserId"), targetUserId);
    obj.insert(QStringLiteral("durationSeconds"), durationSeconds);
    network_manager_->sendCommand(QStringLiteral("MUTE_MEMBER_REQ"), obj);
}

void LoginBackend::unmuteMember(QString const& conversationId, QString const& targetUserId)
{
    if(conversationId.isEmpty() || targetUserId.isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversationId"), conversationId);
    obj.insert(QStringLiteral("targetUserId"), targetUserId);
    network_manager_->sendCommand(QStringLiteral("UNMUTE_MEMBER_REQ"), obj);
}

void LoginBackend::setAdmin(QString const& conversationId, QString const& targetUserId, bool isAdmin)
{
    if(conversationId.isEmpty() || targetUserId.isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        setErrorMessage(QStringLiteral("与服务器的连接已断开"));
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversationId"), conversationId);
    obj.insert(QStringLiteral("targetUserId"), targetUserId);
    obj.insert(QStringLiteral("isAdmin"), isAdmin);
    network_manager_->sendCommand(QStringLiteral("SET_ADMIN_REQ"), obj);
}

void LoginBackend::requestHistory(QString const& conversationId)
{
    if(conversationId.isEmpty()) {
        return;
    }
    if(!network_manager_->isConnected()) {
        return;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("conversationId"), conversationId);
    obj.insert(QStringLiteral("beforeSeq"), 0);
    obj.insert(QStringLiteral("limit"), 50);
    network_manager_->sendCommand(QStringLiteral("HISTORY_REQ"), obj);
}

void LoginBackend::openConversation(QString const& conversationId)
{
    if(conversationId.isEmpty()) {
        return;
    }
    if(user_id_.isEmpty()) {
        return;
    }

    auto const loaded = protocol_handler_->loadConversationCache(conversationId);
    auto const local_seq = protocol_handler_->localLastSeq(conversationId);
    auto const server_seq = protocol_handler_->serverLastSeq(conversationId);

    if(!loaded) {
        // 没有本地缓存：拉最新一页历史。
        requestHistory(conversationId);
    } else if(server_seq > local_seq && network_manager_->isConnected()) {
        // 有本地缓存但落后：请求 afterSeq 增量历史。
        QJsonObject obj;
        obj.insert(QStringLiteral("conversationId"), conversationId);
        obj.insert(QStringLiteral("afterSeq"), local_seq);
        obj.insert(QStringLiteral("limit"), 100);
        network_manager_->sendCommand(QStringLiteral("HISTORY_REQ"), obj);
    }
}

void LoginBackend::onNetworkConnected()
{
    sendCurrentCommand();
}

void LoginBackend::onNetworkError(QString errorMessage)
{
    setBusy(false);
    setErrorMessage(QStringLiteral("无法连接服务器"));
}

void LoginBackend::onProtocolError(QString errorMessage)
{
    setBusy(false);
    setErrorMessage(errorMessage);
}

void LoginBackend::onLoginSucceeded(QString userId, QString displayName, QString worldConversationId)
{
    setBusy(false);
    pending_command_ = PendingCommand::None;

    if(user_id_ != userId) {
        user_id_ = userId;
        emit userIdChanged();
    }

    if(display_name_ != displayName) {
        display_name_ = displayName;
        emit displayNameChanged();
    }

    if(!worldConversationId.isEmpty()) {
        world_conversation_id_ = worldConversationId;
    }

    emit loginSucceeded();
}

void LoginBackend::onDisplayNameUpdated(QString displayName)
{
    if(display_name_ != displayName) {
        display_name_ = displayName;
        emit displayNameChanged();
    }
    setErrorMessage(QString{});
}

void LoginBackend::onNetworkDisconnected()
{
    if(!busy_) {
        return;
    }

    setBusy(false);
    setErrorMessage(QStringLiteral("与服务器的连接已断开"));
    pending_command_ = PendingCommand::None;
}

void LoginBackend::setBusy(bool value)
{
    if(busy_ == value) {
        return;
    }
    busy_ = value;
    emit busyChanged();
}

void LoginBackend::setErrorMessage(QString const& msg)
{
    if(error_message_ == msg) {
        return;
    }
    error_message_ = msg;
    emit errorMessageChanged();
}

void LoginBackend::sendCurrentCommand()
{
    if(pending_command_ == PendingCommand::None) {
        return;
    }

    QJsonObject obj;
    QString command;

    if(pending_command_ == PendingCommand::Login) {
        command = QStringLiteral("LOGIN");
        obj.insert(QStringLiteral("account"), pending_account_);
        obj.insert(QStringLiteral("password"), pending_password_);
    } else if(pending_command_ == PendingCommand::Register) {
        command = QStringLiteral("REGISTER");
        obj.insert(QStringLiteral("account"), pending_account_);
        obj.insert(QStringLiteral("password"), pending_password_);
        obj.insert(QStringLiteral("confirmPassword"), pending_confirm_);
    }

    network_manager_->sendCommand(command, obj);
}
