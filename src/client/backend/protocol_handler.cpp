#include "protocol_handler.h"
#include "network_manager.h"
#include "message_cache.h"

#include <QJsonArray>
#include <algorithm>

ProtocolHandler::ProtocolHandler(NetworkManager* networkManager, MessageCache* messageCache, QObject* parent)
    : QObject(parent)
    , network_manager_(networkManager)
    , message_cache_(messageCache)
{
    connect(network_manager_, &NetworkManager::commandReceived, this, &ProtocolHandler::handleCommand);
}

auto ProtocolHandler::setUserId(QString const& userId) -> void
{
    user_id_ = userId;
    message_cache_->setUserId(userId);
}

auto ProtocolHandler::userId() const -> QString
{
    return user_id_;
}

auto ProtocolHandler::setDisplayName(QString const& displayName) -> void
{
    display_name_ = displayName;
}

auto ProtocolHandler::displayName() const -> QString
{
    return display_name_;
}

auto ProtocolHandler::setWorldConversationId(QString const& worldConversationId) -> void
{
    world_conversation_id_ = worldConversationId;
}

auto ProtocolHandler::worldConversationId() const -> QString
{
    return world_conversation_id_;
}

auto ProtocolHandler::localLastSeq(QString const& conversationId) const -> qint64
{
    return local_last_seq_.value(conversationId, 0);
}

auto ProtocolHandler::serverLastSeq(QString const& conversationId) const -> qint64
{
    return conv_last_seq_.value(conversationId, 0);
}

auto ProtocolHandler::loadConversationCache(QString const& conversationId) -> bool
{
    if(user_id_.isEmpty()) {
        return false;
    }

    auto const [messages, last_seq] = message_cache_->loadMessages(conversationId);

    if(messages.isEmpty()) {
        return false;
    }

    local_last_seq_[conversationId] = last_seq;

    for(auto const& item : messages) {
        auto const m = item.toObject();
        auto const sender_id = m.value(QStringLiteral("senderId")).toString();
        auto const sender_name = m.value(QStringLiteral("senderDisplayName")).toString();
        auto const content = m.value(QStringLiteral("content")).toString();
        auto const msg_type = m.value(QStringLiteral("msgType")).toString();
        auto const server_time_ms = static_cast<qint64>(m.value(QStringLiteral("serverTimeMs")).toDouble(0.0));
        auto const seq = static_cast<qint64>(m.value(QStringLiteral("seq")).toDouble(0.0));

        emit messageReceived(conversationId, sender_id, sender_name, content, msg_type, server_time_ms, seq);
    }

    return true;
}

void ProtocolHandler::handleCommand(QString command, QJsonObject payload)
{
    if(command == QStringLiteral("LOGIN_RESP")) {
        handleLoginResponse(payload);
    } else if(command == QStringLiteral("REGISTER_RESP")) {
        handleRegisterResponse(payload);
    } else if(command == QStringLiteral("MSG_PUSH")) {
        handleMessagePush(payload);
    } else if(command == QStringLiteral("HISTORY_RESP")) {
        handleHistoryResponse(payload);
    } else if(command == QStringLiteral("CONV_LIST_RESP")) {
        handleConversationListResponse(payload);
    } else if(command == QStringLiteral("PROFILE_UPDATE_RESP")) {
        handleProfileUpdateResponse(payload);
    } else if(command == QStringLiteral("FRIEND_LIST_RESP")) {
        handleFriendListResponse(payload);
    } else if(command == QStringLiteral("FRIEND_REQ_LIST_RESP")) {
        handleFriendRequestListResponse(payload);
    } else if(command == QStringLiteral("FRIEND_SEARCH_RESP")) {
        handleFriendSearchResponse(payload);
    } else if(command == QStringLiteral("FRIEND_ADD_RESP")) {
        handleFriendAddResponse(payload);
    } else if(command == QStringLiteral("FRIEND_ACCEPT_RESP")) {
        handleFriendAcceptResponse(payload);
    } else if(command == QStringLiteral("FRIEND_REJECT_RESP")) {
        handleFriendRejectResponse(payload);
    } else if(command == QStringLiteral("OPEN_SINGLE_CONV_RESP")) {
        handleOpenSingleConvResponse(payload);
    } else if(command == QStringLiteral("CREATE_GROUP_RESP")) {
        handleCreateGroupResponse(payload);
    } else if(command == QStringLiteral("CONV_MEMBERS_RESP")) {
        handleConversationMembersResponse(payload);
    } else if(command == QStringLiteral("MUTE_MEMBER_RESP")) {
        handleMuteMemberResponse(payload);
    } else if(command == QStringLiteral("UNMUTE_MEMBER_RESP")) {
        handleUnmuteMemberResponse(payload);
    } else if(command == QStringLiteral("SET_ADMIN_RESP")) {
        handleSetAdminResponse(payload);
    } else if(command == QStringLiteral("LEAVE_CONV_RESP")) {
        handleLeaveConversationResponse(payload);
    } else if(command == QStringLiteral("GROUP_SEARCH_RESP")) {
        handleGroupSearchResponse(payload);
    } else if(command == QStringLiteral("GROUP_JOIN_RESP")) {
        handleGroupJoinResponse(payload);
    } else if(command == QStringLiteral("GROUP_JOIN_REQ_LIST_RESP")) {
        handleGroupJoinRequestListResponse(payload);
    } else if(command == QStringLiteral("GROUP_JOIN_ACCEPT_RESP")) {
        handleGroupJoinAcceptResponse(payload);
    } else if(command == QStringLiteral("ERROR")) {
        handleErrorResponse(payload);
    }
}

void ProtocolHandler::handleLoginResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("登录失败"));
        emit errorOccurred(msg);
        return;
    }

    auto const id = obj.value(QStringLiteral("userId")).toString();
    auto const name = obj.value(QStringLiteral("displayName")).toString();
    auto const world_id = obj.value(QStringLiteral("worldConversationId")).toString();

    setUserId(id);
    setDisplayName(name);
    setWorldConversationId(world_id);

    emit loginSucceeded(id, name, world_id);
}

void ProtocolHandler::handleRegisterResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("注册失败"));
        emit errorOccurred(msg);
        return;
    }

    // 注册成功，account 由 LoginBackend 在 sendCurrentCommand 时设置
    emit registrationSucceeded(user_id_);
}

void ProtocolHandler::handleProfileUpdateResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("修改昵称失败"));
        emit errorOccurred(msg);
        return;
    }

    auto const name = obj.value(QStringLiteral("displayName")).toString();
    if(!name.isEmpty()) {
        setDisplayName(name);
        emit displayNameUpdated(name);
    }
}

void ProtocolHandler::handleMessagePush(QJsonObject const& obj)
{
    auto const conversation_id = obj.value(QStringLiteral("conversationId")).toString();
    auto const sender_id = obj.value(QStringLiteral("senderId")).toString();
    auto const sender_name = obj.value(QStringLiteral("senderDisplayName")).toString();
    auto const content = obj.value(QStringLiteral("content")).toString();
    auto const msg_type = obj.value(QStringLiteral("msgType")).toString();
    auto const server_time_ms = static_cast<qint64>(obj.value(QStringLiteral("serverTimeMs")).toDouble(0.0));
    auto const seq = static_cast<qint64>(obj.value(QStringLiteral("seq")).toDouble(0.0));

    // 检测 seq 间隙（短暂离线）。
    auto const local_seq = local_last_seq_.value(conversation_id, 0);
    if(seq > local_seq + 1) {
        // TODO: 可在此触发补拉历史。
    }

    // 首次见到该会话，触发会话列表刷新。
    if(!conv_last_seq_.contains(conversation_id) && network_manager_->isConnected()) {
        emit needRequestConversationList();
    }

    emit messageReceived(conversation_id, sender_id, sender_name, content, msg_type, server_time_ms, seq);

    // 追加写入本地缓存。
    message_cache_->appendMessage(conversation_id, sender_id, sender_name, content, msg_type, server_time_ms, seq);

    // 更新已知的最新 seq。
    conv_last_seq_[conversation_id] = std::max(conv_last_seq_.value(conversation_id, 0), seq);
}

void ProtocolHandler::handleHistoryResponse(QJsonObject const& obj)
{
    auto const conversation_id = obj.value(QStringLiteral("conversationId")).toString();
    auto const messages = obj.value(QStringLiteral("messages")).toArray();

    qint64 max_seq = local_last_seq_.value(conversation_id, 0);
    for(auto const& item : messages) {
        auto const message_obj = item.toObject();
        auto const sender_id = message_obj.value(QStringLiteral("senderId")).toString();
        auto const sender_name = message_obj.value(QStringLiteral("senderDisplayName")).toString();
        auto const content = message_obj.value(QStringLiteral("content")).toString();
        auto const msg_type = message_obj.value(QStringLiteral("msgType")).toString();
        auto const server_time_ms = static_cast<qint64>(message_obj.value(QStringLiteral("serverTimeMs")).toDouble(0.0));
        auto const seq = static_cast<qint64>(message_obj.value(QStringLiteral("seq")).toDouble(0.0));

        emit messageReceived(conversation_id, sender_id, sender_name, content, msg_type, server_time_ms, seq);

        if(seq > max_seq) {
            max_seq = seq;
        }
    }

    if(max_seq > 0) {
        local_last_seq_[conversation_id] = max_seq;
        conv_last_seq_[conversation_id] = std::max(conv_last_seq_.value(conversation_id, 0), max_seq);
    }

    // 将这一页历史写入本地缓存。
    message_cache_->writeMessages(conversation_id, messages);
}

void ProtocolHandler::handleConversationListResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(true);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const array = obj.value(QStringLiteral("conversations")).toArray();

    QVariantList list;
    list.reserve(array.size());

    for(auto const& item : array) {
        auto const conv = item.toObject();

        auto const id = conv.value(QStringLiteral("conversationId")).toString();
        auto const type = conv.value(QStringLiteral("conversationType")).toString();
        auto const title = conv.value(QStringLiteral("title")).toString().simplified();
        auto const last_seq = static_cast<qint64>(conv.value(QStringLiteral("lastSeq")).toDouble(0.0));
        auto const last_time_ms = static_cast<qint64>(conv.value(QStringLiteral("lastServerTimeMs")).toDouble(0.0));

        QVariantMap map;
        map.insert(QStringLiteral("conversationId"), id);
        map.insert(QStringLiteral("conversationType"), type);
        map.insert(QStringLiteral("title"), title);
        map.insert(QStringLiteral("lastSeq"), last_seq);
        map.insert(QStringLiteral("lastServerTimeMs"), last_time_ms);

        map.insert(QStringLiteral("preview"), conv.value(QStringLiteral("preview")).toString());
        map.insert(QStringLiteral("time"), conv.value(QStringLiteral("time")).toString());
        map.insert(QStringLiteral("unreadCount"), conv.value(QStringLiteral("unreadCount")).toInt());

        QString initials;
        if(!title.isEmpty()) {
            initials = title.left(1);
        }
        map.insert(QStringLiteral("initials"), initials);

        QString avatarColor;
        if(type == QStringLiteral("GROUP")) {
            avatarColor = QStringLiteral("#4fbf73");
        } else {
            avatarColor = QStringLiteral("#4f90f2");
        }
        map.insert(QStringLiteral("avatarColor"), avatarColor);

        list.push_back(map);

        // 更新服务器端已知的该会话最新 seq。
        conv_last_seq_[id] = last_seq;
    }

    emit conversationsReset(list);
}

void ProtocolHandler::handleConversationMembersResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const conv_id = obj.value(QStringLiteral("conversationId")).toString();
    auto const array = obj.value(QStringLiteral("members")).toArray();

    QVariantList list;
    list.reserve(array.size());

    for(auto const& item : array) {
        auto const m = item.toObject();
        QVariantMap map;
        map.insert(QStringLiteral("userId"), m.value(QStringLiteral("userId")).toString());
        map.insert(QStringLiteral("displayName"), m.value(QStringLiteral("displayName")).toString());
        map.insert(QStringLiteral("role"), m.value(QStringLiteral("role")).toString());
        map.insert(QStringLiteral("mutedUntilMs"), static_cast<qint64>(m.value(QStringLiteral("mutedUntilMs")).toDouble(0)));
        list.push_back(map);
    }

    emit conversationMembersReady(conv_id, list);
}

void ProtocolHandler::handleLeaveConversationResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
    }
}

void ProtocolHandler::handleMuteMemberResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }
    auto const conv_id = obj.value(QStringLiteral("conversationId")).toString();
    if(!conv_id.isEmpty()) {
        emit needRequestConversationMembers(conv_id);
    }
}

void ProtocolHandler::handleUnmuteMemberResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }
    auto const conv_id = obj.value(QStringLiteral("conversationId")).toString();
    if(!conv_id.isEmpty()) {
        emit needRequestConversationMembers(conv_id);
    }
}

void ProtocolHandler::handleSetAdminResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }
    auto const conv_id = obj.value(QStringLiteral("conversationId")).toString();
    if(!conv_id.isEmpty()) {
        emit needRequestConversationMembers(conv_id);
    }
}

void ProtocolHandler::handleErrorResponse(QJsonObject const& obj)
{
    auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
    if(!msg.isEmpty()) {
        emit errorOccurred(msg);
    }
}

void ProtocolHandler::handleFriendListResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(true);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const array = obj.value(QStringLiteral("friends")).toArray();

    QVariantList list;
    list.reserve(array.size());

    for(auto const& item : array) {
        auto const u = item.toObject();

        QVariantMap map;
        map.insert(QStringLiteral("userId"), u.value(QStringLiteral("userId")).toString());
        map.insert(QStringLiteral("account"), u.value(QStringLiteral("account")).toString());
        map.insert(QStringLiteral("displayName"), u.value(QStringLiteral("displayName")).toString().simplified());
        map.insert(QStringLiteral("region"), u.value(QStringLiteral("region")).toString());
        map.insert(QStringLiteral("signature"), u.value(QStringLiteral("signature")).toString());

        list.push_back(map);
    }

    emit friendsReset(list);
}

void ProtocolHandler::handleFriendRequestListResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(true);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const array = obj.value(QStringLiteral("requests")).toArray();

    QVariantList list;
    list.reserve(array.size());

    for(auto const& item : array) {
        auto const r = item.toObject();

        QVariantMap map;
        map.insert(QStringLiteral("requestId"), r.value(QStringLiteral("requestId")).toString());
        map.insert(QStringLiteral("fromUserId"), r.value(QStringLiteral("fromUserId")).toString());
        map.insert(QStringLiteral("account"), r.value(QStringLiteral("account")).toString());
        map.insert(QStringLiteral("displayName"), r.value(QStringLiteral("displayName")).toString().simplified());
        map.insert(QStringLiteral("status"), r.value(QStringLiteral("status")).toString());
        map.insert(QStringLiteral("helloMsg"), r.value(QStringLiteral("helloMsg")).toString());

        list.push_back(map);
    }

    emit friendRequestsReset(list);
}

void ProtocolHandler::handleFriendSearchResponse(QJsonObject const& obj)
{
    QVariantMap result;

    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    result.insert(QStringLiteral("ok"), ok);

    if(!ok) {
        result.insert(QStringLiteral("errorCode"), obj.value(QStringLiteral("errorCode")).toString());
        result.insert(QStringLiteral("errorMsg"), obj.value(QStringLiteral("errorMsg")).toString());
        emit friendSearchFinished(result);
        return;
    }

    auto const user_obj = obj.value(QStringLiteral("user")).toObject();

    result.insert(QStringLiteral("userId"), user_obj.value(QStringLiteral("userId")).toString());
    result.insert(QStringLiteral("account"), user_obj.value(QStringLiteral("account")).toString());
    result.insert(QStringLiteral("displayName"), user_obj.value(QStringLiteral("displayName")).toString());
    result.insert(QStringLiteral("region"), user_obj.value(QStringLiteral("region")).toString());
    result.insert(QStringLiteral("signature"), user_obj.value(QStringLiteral("signature")).toString());

    result.insert(QStringLiteral("isFriend"), obj.value(QStringLiteral("isFriend")).toBool());
    result.insert(QStringLiteral("isSelf"), obj.value(QStringLiteral("isSelf")).toBool());

    emit friendSearchFinished(result);
}

void ProtocolHandler::handleFriendAddResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("添加好友失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    emit friendRequestSucceeded();
}

void ProtocolHandler::handleFriendAcceptResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("同意好友申请失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    // 同意成功后，刷新"新的朋友"列表和好友列表。
    emit needRequestFriendRequestList();
    emit needRequestFriendList();
}

void ProtocolHandler::handleFriendRejectResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("拒绝好友申请失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    // 拒绝成功后，刷新"新的朋友"列表。
    emit needRequestFriendRequestList();
}

void ProtocolHandler::handleOpenSingleConvResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("打开会话失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const conv_id = obj.value(QStringLiteral("conversationId")).toString();
    if(conv_id.isEmpty()) {
        return;
    }

    // 触发会话列表刷新。
    emit needRequestConversationList();

    emit singleConversationReady(conv_id, obj.value(QStringLiteral("conversationType")).toString());
}

void ProtocolHandler::handleCreateGroupResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("创建群聊失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const conv_id = obj.value(QStringLiteral("conversationId")).toString();
    auto const title = obj.value(QStringLiteral("title")).toString();

    emit needRequestConversationList();
    emit groupCreated(conv_id, title);
}

void ProtocolHandler::handleGroupSearchResponse(QJsonObject const& obj)
{
    QVariantMap result;
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);

    if(!ok) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("errorCode"), obj.value(QStringLiteral("errorCode")).toString());
        result.insert(QStringLiteral("errorMsg"), obj.value(QStringLiteral("errorMsg")).toString());
        emit groupSearchFinished(result);
        return;
    }

    auto const group = obj.value(QStringLiteral("group")).toObject();
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("groupId"), group.value(QStringLiteral("groupId")).toString());
    result.insert(QStringLiteral("name"), group.value(QStringLiteral("name")).toString());
    result.insert(QStringLiteral("memberCount"), static_cast<qint64>(group.value(QStringLiteral("memberCount")).toDouble(0)));
    result.insert(QStringLiteral("isMember"), obj.value(QStringLiteral("isMember")).toBool(false));

    emit groupSearchFinished(result);
}

void ProtocolHandler::handleGroupJoinResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("申请加入群聊失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    emit groupJoinRequestSucceeded();
}

void ProtocolHandler::handleGroupJoinRequestListResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(true);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString();
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    auto const array = obj.value(QStringLiteral("requests")).toArray();

    QVariantList list;
    list.reserve(array.size());

    for(auto const& item : array) {
        auto const r = item.toObject();
        QVariantMap map;
        map.insert(QStringLiteral("requestId"), r.value(QStringLiteral("requestId")).toString());
        map.insert(QStringLiteral("fromUserId"), r.value(QStringLiteral("fromUserId")).toString());
        map.insert(QStringLiteral("account"), r.value(QStringLiteral("account")).toString());
        map.insert(QStringLiteral("displayName"), r.value(QStringLiteral("displayName")).toString());
        map.insert(QStringLiteral("groupId"), r.value(QStringLiteral("groupId")).toString());
        map.insert(QStringLiteral("groupName"), r.value(QStringLiteral("groupName")).toString());
        map.insert(QStringLiteral("status"), r.value(QStringLiteral("status")).toString());
        map.insert(QStringLiteral("helloMsg"), r.value(QStringLiteral("helloMsg")).toString());
        list.push_back(map);
    }

    emit groupJoinRequestsReset(list);
}

void ProtocolHandler::handleGroupJoinAcceptResponse(QJsonObject const& obj)
{
    auto const ok = obj.value(QStringLiteral("ok")).toBool(false);
    if(!ok) {
        auto const msg = obj.value(QStringLiteral("errorMsg")).toString(QStringLiteral("处理入群申请失败"));
        if(!msg.isEmpty()) {
            emit errorOccurred(msg);
        }
        return;
    }

    // 成功后，刷新入群申请列表
    emit needRequestGroupJoinRequestList();
}
