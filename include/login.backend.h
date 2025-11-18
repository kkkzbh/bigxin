#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <print>

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

        if(user_id_ != id) {
            user_id_ = id;
            emit userIdChanged();
        }

        if(display_name_ != name) {
            display_name_ = name;
            emit displayNameChanged();
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

    QTcpSocket socket_{};
    QByteArray buffer_{};
    bool busy_{ false };
    QString error_message_{};
    QString user_id_{};
    QString display_name_{};

    PendingCommand pending_command_{ PendingCommand::None };
    QString pending_account_{};
    QString pending_password_{};
    QString pending_confirm_{};

    QString host_{ QStringLiteral("127.0.0.1") };
    quint16 port_{ 5555 };
};
