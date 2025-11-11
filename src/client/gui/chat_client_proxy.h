// 轻量 QML 代理：包装 client.core::ChatClient，提供 QML 可用的连接/发送接口
#pragma once

#include <QObject>
#include <QString>
#include <memory>

// 前置声明（在 .cpp 中 import C++ module）
struct ChatClient;

class ChatClientProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString nickname READ nickname WRITE setNickname NOTIFY nicknameChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY autoConnectChanged)

public:
    explicit ChatClientProxy(QObject* parent = nullptr);
    ~ChatClientProxy() override;

    // TODO(net-state): 暴露连接状态（connected/busy/error），并在 QML 显示网络提示；
    // TODO(net-auth): 登录功能上线后，提供 login(nick/token) 等接口与属性。

    // properties
    QString host() const { return host_; }
    void setHost(QString h);

    int port() const { return static_cast<int>(port_); }
    void setPort(int p);

    QString nickname() const { return nickname_; }
    void setNickname(QString n);

    bool autoConnect() const { return auto_connect_; }
    void setAutoConnect(bool v);

    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void sendText(QString text);

signals:
    void hostChanged();
    void portChanged();
    void nicknameChanged();
    void autoConnectChanged();

    void connected();
    void disconnected();
    void messageAdded(QString user, QString text);
    void errorOccurred(QString message);

private:
    void ensureClient();
    void doHelloIfNeeded();

private:
    QString host_{"127.0.0.1"};   // 暂定：固定本地服务器
    quint16 port_{7777};           // 暂定：固定端口
    QString nickname_{"kkkzbh_gui"};
    bool auto_connect_{true};      // App 启动时自动连接

    std::shared_ptr<ChatClient> client_{};
};
