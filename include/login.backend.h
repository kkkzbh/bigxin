#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>

#include <print>
#include <algorithm>

/// \brief 登录 / 注册相关的前端网络适配层。
/// \details 使用 QTcpSocket 与后端 Asio 服务器通过文本协议交互。
class LoginBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY userIdChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)

public:
    /// \brief 构造函数，初始化 socket 并连接信号槽。
    explicit LoginBackend(QObject* parent = nullptr)
        : QObject(parent)
    {
        QObject::connect(&socket_, &QTcpSocket::connected, this, &LoginBackend::onConnected);
        QObject::connect(&socket_, &QTcpSocket::readyRead, this, &LoginBackend::onReadyRead);
        QObject::connect(
            &socket_,
            &QTcpSocket::errorOccurred,
            this,
            &LoginBackend::onErrorOccurred
        );
    }

    /// \brief 当前是否有网络请求在处理。
    auto busy() const -> bool
    {
        return busy_;
    }

    /// \brief 最近一次错误信息，空字符串表示无错误。
    auto errorMessage() const -> QString
    {
        return error_message_;
    }

    /// \brief 登录成功后的用户 ID。
    auto userId() const -> QString
    {
        return user_id_;
    }

    /// \brief 登录成功后的用户昵称。
    auto displayName() const -> QString
    {
        return display_name_;
    }

    /// \brief 默认“世界”会话的 ID。
    auto worldConversationId() const -> QString
    {
        return world_conversation_id_;
    }

    /// \brief 发起登录请求。
    /// \param account 登录账号。
    /// \param password 登录密码。
    /// \note 作为 Q_INVOKABLE，使用传统 void 返回类型以兼容 Qt moc。
    Q_INVOKABLE void login(QString const& account, QString const& password)
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
        ensureConnected();
    }

    /// \brief 发起注册请求。
    /// \param account 注册账号。
    /// \param password 注册密码。
    /// \param confirmPassword 确认密码。
    /// \note 作为 Q_INVOKABLE，使用传统 void 返回类型以兼容 Qt moc。
    Q_INVOKABLE void registerAccount(
        QString const& account,
        QString const& password,
        QString const& confirmPassword
    )
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
        ensureConnected();
    }

    /// \brief 主动清空错误信息。
    /// \note 作为 Q_INVOKABLE，使用传统 void 返回类型以兼容 Qt moc。
    Q_INVOKABLE void clearError()
    {
        setErrorMessage(QString{});
    }

    /// \brief 向默认“世界”会话发送一条文本消息。
    /// \param text 要发送的消息内容。
    /// \note 仅在已登录且 socket 仍保持连接时有效。
    Q_INVOKABLE void sendWorldTextMessage(QString const& text)
    {
        sendMessage(world_conversation_id_, text);
    }

    /// \brief 向指定会话发送一条文本消息。
    /// \param conversationId 会话 ID。
    /// \param text 要发送的消息内容。
    Q_INVOKABLE void sendMessage(QString const& conversationId, QString const& text)
    {
        if(text.isEmpty()) {
            return;
        }
        if(user_id_.isEmpty()) {
            setErrorMessage(QStringLiteral("请先登录后再发送消息"));
            return;
        }
        if(socket_.state() != QAbstractSocket::ConnectedState) {
            setErrorMessage(QStringLiteral("与服务器的连接已断开"));
            return;
        }
        if(conversationId.isEmpty()) {
            setErrorMessage(QStringLiteral("未选择会话"));
            return;
        }

        QJsonObject obj;
        auto const client_id =
            QString::number(QDateTime::currentMSecsSinceEpoch());

        obj.insert("conversationId", conversationId);
        obj.insert("conversationType", QStringLiteral("GROUP"));
        obj.insert("senderId", user_id_);
        obj.insert("clientMsgId", client_id);
        obj.insert("msgType", QStringLiteral("TEXT"));
        obj.insert("content", text);

        QJsonDocument doc{ obj };
        auto payload = doc.toJson(QJsonDocument::Compact);

        QByteArray line;
        line.reserve(8 + 1 + payload.size() + 1);
        line.append("SEND_MSG");
        line.append(':');
        line.append(payload);
        line.append('\n');

        socket_.write(line);
    }

    /// \brief 向服务器请求一页默认“世界”会话的历史消息。
    /// \note 当前实现从最新开始向前拉取一页，大小固定为 50 条。
    Q_INVOKABLE void requestInitialWorldHistory()
    {
        if(world_conversation_id_.isEmpty()) {
            return;
        }
        requestHistory(world_conversation_id_);
    }

    /// \brief 向服务器请求当前用户的会话列表（群聊 + 单聊）。
    Q_INVOKABLE void requestConversationList()
    {
        if(socket_.state() != QAbstractSocket::ConnectedState) {
            return;
        }

        QByteArray line;
        line.append("CONV_LIST_REQ:{}\n");
        socket_.write(line);
    }

    /// \brief 请求指定会话的一页历史消息。
    /// \param conversationId 会话 ID。
    Q_INVOKABLE void requestHistory(QString const& conversationId)
    {
        if(conversationId.isEmpty()) {
            return;
        }
        if(socket_.state() != QAbstractSocket::ConnectedState) {
            return;
        }

        QJsonObject obj;
        obj.insert("conversationId", conversationId);
        obj.insert("beforeSeq", 0);
        obj.insert("limit", 50);

        QJsonDocument doc{ obj };
        auto payload = doc.toJson(QJsonDocument::Compact);

        QByteArray line;
        line.reserve(11 + 1 + payload.size() + 1);
        line.append("HISTORY_REQ");
        line.append(':');
        line.append(payload);
        line.append('\n');

        socket_.write(line);
    }

    /// \brief 打开指定会话：先尝试从本地缓存加载，再在必要时向服务器请求历史。
    /// \param conversationId 会话 ID。
    Q_INVOKABLE void openConversation(QString const& conversationId)
    {
        if(conversationId.isEmpty()) {
            return;
        }
        if(user_id_.isEmpty()) {
            // 尚未登录，不加载会话。
            return;
        }

        auto const loaded = load_conversation_cache(conversationId);
        auto const local_seq = local_last_seq_.value(conversationId, 0);
        auto const server_seq = conv_last_seq_.value(conversationId, 0);

        if(!loaded) {
            // 没有本地缓存：拉最新一页历史。
            requestHistory(conversationId);
        } else if(server_seq > local_seq && socket_.state() == QAbstractSocket::ConnectedState) {
            // 有本地缓存但落后：请求 afterSeq 增量历史。
            QJsonObject obj;
            obj.insert("conversationId", conversationId);
            obj.insert("afterSeq", local_seq);
            obj.insert("limit", 100);

            QJsonDocument doc{ obj };
            auto payload = doc.toJson(QJsonDocument::Compact);

            QByteArray line;
            line.reserve(11 + 1 + payload.size() + 1);
            line.append("HISTORY_REQ");
            line.append(':');
            line.append(payload);
            line.append('\n');

            socket_.write(line);
        }
    }

signals:
    /// \brief busy 状态变化时发出。
    void busyChanged();
    /// \brief 错误信息变化时发出。
    void errorMessageChanged();
    /// \brief userId 变化时发出。
    void userIdChanged();
    /// \brief displayName 变化时发出。
    void displayNameChanged();
    /// \brief 登录成功时发出，可用于进入主界面。
    void loginSucceeded();
    /// \brief 注册成功时发出，参数为注册成功的账号。
    void registrationSucceeded(QString account);

    /// \brief 收到服务器推送的聊天消息时发出。
    /// \param conversationId 会话 ID，目前为“世界”群的 ID。
    /// \param senderId 发送者用户 ID。
    /// \param content 消息文本内容。
    /// \param serverTimeMs 服务器时间戳（毫秒）。
    /// \param seq 会话内消息序号。
    void messageReceived(
        QString conversationId,
        QString senderId,
        QString content,
        qint64 serverTimeMs,
        qint64 seq
    );

    /// \brief 会话列表发生变化时发出，QML 通过该信号重建模型。
    /// \param conversations 由 QVariantMap 组成的列表，每个元素代表一个会话。
    void conversationsReset(QVariantList conversations);

private slots:
    /// \brief socket 连接建立后的回调。
    void onConnected()
    {
        sendCurrentCommand();
    }

    /// \brief socket 收到数据时的回调。
    void onReadyRead()
    {
        buffer_.append(socket_.readAll());

        while(true) {
            auto const index = buffer_.indexOf('\n');
            if(index < 0) {
                break;
            }

            auto const line = QString::fromUtf8(buffer_.constData(), index);
            buffer_.remove(0, index + 1);
            handleLine(line);
        }
    }

    /// \brief socket 错误时的回调。
    /// \param socketError Qt 报告的错误枚举。
    void onErrorOccurred(QAbstractSocket::SocketError socketError)
    {
        std::println("socket error: {}", static_cast<int>(socketError));
        setBusy(false);
        setErrorMessage(QStringLiteral("无法连接服务器"));
    }

private:
    enum class PendingCommand
    {
        None,
        Login,
        Register
    };

    /// \brief 确保与服务器建立连接，并在连接就绪后发送挂起命令。
    /// \details
    /// - 如果 socket 已经处于 ConnectedState，则直接发送当前挂起的命令；
    /// - 否则先中止旧连接，再发起新的 connectToHost，等待 onConnected() 回调里发送。
    auto ensureConnected() -> void
    {
        if(socket_.state() == QAbstractSocket::ConnectedState) {
            sendCurrentCommand();
            return;
        }

        socket_.abort();
        socket_.connectToHost(host_, port_);
    }

    /// \brief 更新 busy 状态并发出信号。
    /// \param value 新的 busy 状态。
    auto setBusy(bool value) -> void
    {
        if(busy_ == value) {
            return;
        }
        busy_ = value;
        emit busyChanged();
    }

    /// \brief 更新错误信息并发出信号。
    /// \param msg 新的错误信息。
    auto setErrorMessage(QString const& msg) -> void
    {
        if(error_message_ == msg) {
            return;
        }
        error_message_ = msg;
        emit errorMessageChanged();
    }

    /// \brief 向服务器发送当前挂起的命令。
    auto sendCurrentCommand() -> void
    {
        if(pending_command_ == PendingCommand::None) {
            return;
        }

        QJsonObject obj;
        QByteArray command;

        if(pending_command_ == PendingCommand::Login) {
            command = "LOGIN";
            obj.insert("account", pending_account_);
            obj.insert("password", pending_password_);
        } else if(pending_command_ == PendingCommand::Register) {
            command = "REGISTER";
            obj.insert("account", pending_account_);
            obj.insert("password", pending_password_);
            obj.insert("confirmPassword", pending_confirm_);
        }

        QJsonDocument doc{ obj };
        auto payload = doc.toJson(QJsonDocument::Compact);

        QByteArray line;
        line.reserve(command.size() + 1 + payload.size() + 1);
        line.append(command);
        line.append(':');
        line.append(payload);
        line.append('\n');

        socket_.write(line);
    }

    /// \brief 处理收到的一行文本。
    /// \param line 不包含结尾换行符的一整行。
    auto handleLine(QString const& line) -> void
    {
        auto const index = line.indexOf(u':');
        if(index <= 0) {
            return;
        }

        auto const command = line.left(index);
        auto const jsonText = line.mid(index + 1).toUtf8();

        auto const doc = QJsonDocument::fromJson(jsonText);
        if(!doc.isObject()) {
            return;
        }
        auto const obj = doc.object();

        if(command == "LOGIN_RESP") {
            handleLoginResponse(obj);
        } else if(command == "REGISTER_RESP") {
            handleRegisterResponse(obj);
        } else if(command == "MSG_PUSH") {
            handleMessagePush(obj);
        } else if(command == "HISTORY_RESP") {
            handleHistoryResponse(obj);
        } else if(command == "CONV_LIST_RESP") {
            handleConversationListResponse(obj);
        }
    }

    /// \brief 处理登录响应。
    /// \param obj LOGIN_RESP 的 JSON 对象。
    auto handleLoginResponse(QJsonObject const& obj) -> void
    {
        setBusy(false);
        pending_command_ = PendingCommand::None;

        auto const ok = obj.value("ok").toBool(false);
        if(!ok) {
            auto const msg = obj.value("errorMsg").toString(QStringLiteral("登录失败"));
            setErrorMessage(msg);
            return;
        }

        auto const id = obj.value("userId").toString();
        auto const name = obj.value("displayName").toString();
        auto const world_id = obj.value("worldConversationId").toString();

        if(user_id_ != id) {
            user_id_ = id;
            emit userIdChanged();
        }

        if(display_name_ != name) {
            display_name_ = name;
            emit displayNameChanged();
        }

        if(!world_id.isEmpty()) {
            world_conversation_id_ = world_id;
        }

        emit loginSucceeded();
    }

    /// \brief 处理注册响应。
    /// \param obj REGISTER_RESP 的 JSON 对象。
    auto handleRegisterResponse(QJsonObject const& obj) -> void
    {
        setBusy(false);
        pending_command_ = PendingCommand::None;

        auto const ok = obj.value("ok").toBool(false);
        if(!ok) {
            auto const msg = obj.value("errorMsg").toString(QStringLiteral("注册失败"));
            setErrorMessage(msg);
            return;
        }

        // 注册成功：清空错误，通知外部注册成功，让界面决定如何提示。
        setErrorMessage(QString{});
        emit registrationSucceeded(pending_account_);
    }

    /// \brief 处理服务器推送的聊天消息。
    /// \param obj MSG_PUSH 的 JSON 对象。
    auto handleMessagePush(QJsonObject const& obj) -> void
    {
        auto const conversation_id = obj.value("conversationId").toString();
        auto const sender_id = obj.value("senderId").toString();
        auto const content = obj.value("content").toString();
        auto const server_time_ms =
            static_cast<qint64>(obj.value("serverTimeMs").toDouble(0.0));
        auto const seq = static_cast<qint64>(obj.value("seq").toDouble(0.0));

        // 检测是否存在 seq 间隙（短暂离线），如有需要可以在此触发一次增量 HISTORY_REQ。
        auto const local_seq = local_last_seq_.value(conversation_id, 0);
        if(seq > local_seq + 1) {
            // TODO: 后续可在这里触发一次补拉历史。
        }

        emit messageReceived(conversation_id, sender_id, content, server_time_ms, seq);

        // 追加写入本地缓存。
        cache_append_message(conversation_id, sender_id, content, server_time_ms, seq);

        // 更新已知的最新 seq。
        conv_last_seq_[conversation_id] = std::max(conv_last_seq_.value(conversation_id, 0), seq);
    }

    /// \brief 处理历史消息响应，将其中每条消息转发为 messageReceived 信号。
    /// \param obj HISTORY_RESP 的 JSON 对象。
    auto handleHistoryResponse(QJsonObject const& obj) -> void
    {
        auto const conversation_id = obj.value("conversationId").toString();
        auto const messages = obj.value("messages").toArray();

        qint64 max_seq = local_last_seq_.value(conversation_id, 0);
        for(auto const& item : messages) {
            auto const message_obj = item.toObject();
            auto const sender_id = message_obj.value("senderId").toString();
            auto const content = message_obj.value("content").toString();
            auto const server_time_ms =
                static_cast<qint64>(message_obj.value("serverTimeMs").toDouble(0.0));
            auto const seq =
                static_cast<qint64>(message_obj.value("seq").toDouble(0.0));

            emit messageReceived(conversation_id, sender_id, content, server_time_ms, seq);

            if(seq > max_seq) {
                max_seq = seq;
            }
        }

        if(max_seq > 0) {
            local_last_seq_[conversation_id] = max_seq;
            conv_last_seq_[conversation_id] =
                std::max(conv_last_seq_.value(conversation_id, 0), max_seq);
        }

        // 将这一页历史写入本地缓存，方便下次快速加载。
        cache_write_messages(conversation_id, messages);
    }

    /// \brief 处理会话列表响应，转为 QVariantList 通知 QML。
    /// \param obj CONV_LIST_RESP 的 JSON 对象。
    auto handleConversationListResponse(QJsonObject const& obj) -> void
    {
        auto const ok = obj.value("ok").toBool(true);
        if(!ok) {
            auto const msg = obj.value("errorMsg").toString();
            if(!msg.isEmpty()) {
                setErrorMessage(msg);
            }
            return;
        }

        auto const array = obj.value("conversations").toArray();

        QVariantList list;
        list.reserve(array.size());

        for(auto const& item : array) {
            auto const conv = item.toObject();

            auto const id = conv.value("conversationId").toString();
            auto const type = conv.value("conversationType").toString();
            auto const title = conv.value("title").toString();
             auto const last_seq =
                static_cast<qint64>(conv.value("lastSeq").toDouble(0.0));
            auto const last_time_ms =
                static_cast<qint64>(conv.value("lastServerTimeMs").toDouble(0.0));

            QVariantMap map;
            map.insert(QStringLiteral("conversationId"), id);
            map.insert(QStringLiteral("conversationType"), type);
            map.insert(QStringLiteral("title"), title);
            map.insert(QStringLiteral("lastSeq"), last_seq);
            map.insert(QStringLiteral("lastServerTimeMs"), last_time_ms);

            map.insert(QStringLiteral("preview"), conv.value("preview").toString());
            map.insert(QStringLiteral("time"), conv.value("time").toString());
            map.insert(QStringLiteral("unreadCount"), conv.value("unreadCount").toInt());

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

    /// \brief 返回当前用户的缓存基础目录。
    /// \details 目录结构为：
    /// - <appDir>/../cache/user_<userId>/conv_<conversationId>.json
    auto cache_base_dir() const -> QDir
    {
        auto dir = QDir{ QCoreApplication::applicationDirPath() };
        dir.cdUp();

        if(!dir.exists(QStringLiteral("cache"))) {
            static_cast<void>(dir.mkdir(QStringLiteral("cache")));
        }
        dir.cd(QStringLiteral("cache"));

        if(!user_id_.isEmpty()) {
            auto const sub = QStringLiteral("user_%1").arg(user_id_);
            if(!dir.exists(sub)) {
                static_cast<void>(dir.mkdir(sub));
            }
            dir.cd(sub);
        }

        return dir;
    }

    /// \brief 生成指定会话的缓存文件路径。
    auto cache_file_path(QString const& conversationId) const -> QString
    {
        auto dir = cache_base_dir();
        return dir.filePath(QStringLiteral("conv_%1.json").arg(conversationId));
    }

    /// \brief 将一页历史消息写入本地缓存文件，覆盖旧内容。
    /// \param conversationId 会话 ID。
    /// \param messages 服务端 HISTORY_RESP 中的 messages 数组。
    auto cache_write_messages(
        QString const& conversationId,
        QJsonArray const& messages
    ) -> void
    {
        if(user_id_.isEmpty()) {
            return;
        }

        auto const path = cache_file_path(conversationId);

        // 计算本页中最大 seq，作为该会话最新 seq。
        auto last_seq = qint64{};
        for(auto const& item : messages) {
            auto const obj = item.toObject();
            auto const seq =
                static_cast<qint64>(obj.value(QStringLiteral("seq")).toDouble(0.0));
            if(seq > last_seq) {
                last_seq = seq;
            }
        }

        QJsonObject root;
        root.insert(QStringLiteral("conversationId"), conversationId);
        root.insert(QStringLiteral("messages"), messages);
        root.insert(QStringLiteral("lastSeq"), static_cast<qint64>(last_seq));

        auto const doc = QJsonDocument{ root };

        QFile file{ path };
        if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }

        static_cast<void>(file.write(doc.toJson(QJsonDocument::Compact)));
        file.close();

        // 同步更新内存中的本地 latest seq。
        local_last_seq_[conversationId] = last_seq;
    }

    /// \brief 追加一条消息到本地缓存。
    /// \param conversationId 会话 ID。
    /// \param senderId 发送者 ID。
    /// \param content 消息文本。
    /// \param serverTimeMs 服务器时间戳（毫秒）。
    /// \param seq 会话内序号。
    auto cache_append_message(
        QString const& conversationId,
        QString const& senderId,
        QString const& content,
        qint64 serverTimeMs,
        qint64 seq
    ) -> void
    {
        if(user_id_.isEmpty()) {
            return;
        }

        auto const path = cache_file_path(conversationId);

        QJsonArray messages;

        {
            QFile file{ path };
            if(file.exists() && file.open(QIODevice::ReadOnly)) {
                auto const data = file.readAll();
                file.close();

                auto const doc = QJsonDocument::fromJson(data);
                if(doc.isObject()) {
                    auto const obj = doc.object();
                    messages = obj.value(QStringLiteral("messages")).toArray();
                }
            }
        }

        QJsonObject message;
        message.insert(QStringLiteral("senderId"), senderId);
        message.insert(QStringLiteral("content"), content);
        message.insert(QStringLiteral("serverTimeMs"), serverTimeMs);
        message.insert(QStringLiteral("seq"), seq);

        messages.append(message);

        cache_write_messages(conversationId, messages);
    }

    /// \brief 从本地缓存加载指定会话的消息，并以 messageReceived 形式重放。
    /// \param conversationId 会话 ID。
    /// \return 是否成功从缓存加载至少一条消息。
    auto load_conversation_cache(QString const& conversationId) -> bool
    {
        if(user_id_.isEmpty()) {
            return false;
        }

        auto const path = cache_file_path(conversationId);
        QFile file{ path };
        if(!file.exists()) {
            return false;
        }
        if(!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        auto const data = file.readAll();
        file.close();

        auto const doc = QJsonDocument::fromJson(data);
        if(!doc.isObject()) {
            return false;
        }

        auto const obj = doc.object();
        auto const messages = obj.value(QStringLiteral("messages")).toArray();

        if(messages.isEmpty()) {
            return false;
        }

        // 从缓存中恢复 lastSeq。
        auto last_seq =
            static_cast<qint64>(obj.value(QStringLiteral("lastSeq")).toDouble(0.0));
        if(last_seq <= 0) {
            for(auto const& item : messages) {
                auto const m = item.toObject();
                auto const seq =
                    static_cast<qint64>(m.value(QStringLiteral("seq")).toDouble(0.0));
                if(seq > last_seq) {
                    last_seq = seq;
                }
            }
        }
        local_last_seq_[conversationId] = last_seq;

        for(auto const& item : messages) {
            auto const m = item.toObject();
            auto const sender_id = m.value(QStringLiteral("senderId")).toString();
            auto const content = m.value(QStringLiteral("content")).toString();
            auto const server_time_ms =
                static_cast<qint64>(m.value(QStringLiteral("serverTimeMs")).toDouble(0.0));
            auto const seq =
                static_cast<qint64>(m.value(QStringLiteral("seq")).toDouble(0.0));

            emit messageReceived(conversationId, sender_id, content, server_time_ms, seq);
        }

        return true;
    }

    QTcpSocket socket_{};
    QByteArray buffer_{};
    bool busy_{ false };
    QString error_message_{};
    QString user_id_{};
    QString display_name_{};
    QString world_conversation_id_{};

    PendingCommand pending_command_{ PendingCommand::None };
    QString pending_account_{};
    QString pending_password_{};
    QString pending_confirm_{};

    QString host_{ QStringLiteral("127.0.0.1") };
    quint16 port_{ 5555 };

    /// \brief 每个会话在服务器端已知的最新 seq。
    QHash<QString, qint64> conv_last_seq_{};
    /// \brief 本地缓存中每个会话的最新 seq。
    QHash<QString, qint64> local_last_seq_{};
};
