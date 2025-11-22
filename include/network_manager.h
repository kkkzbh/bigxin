#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QJsonObject>

/// \brief 网络通信管理器，负责 TCP 连接、断线重连和文本协议收发。
/// \details 封装 QTcpSocket，向上层提供简洁的命令发送和接收接口。
class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject* parent = nullptr);

    /// \brief 获取当前连接状态。
    auto isConnected() const -> bool;

    /// \brief 连接到服务器。
    /// \param host 服务器主机地址。
    /// \param port 服务器端口。
    auto connectToServer(QString const& host, quint16 port) -> void;

    /// \brief 断开与服务器的连接。
    auto disconnect() -> void;

    /// \brief 发送一条命令到服务器。
    /// \param command 命令名（如 LOGIN、SEND_MSG）。
    /// \param payload JSON 对象负载。
    auto sendCommand(QString const& command, QJsonObject const& payload) -> void;

signals:
    /// \brief 连接成功时发出。
    void connected();

    /// \brief 连接断开时发出。
    void disconnected();

    /// \brief 连接错误时发出。
    /// \param errorMessage 错误描述信息。
    void errorOccurred(QString errorMessage);

    /// \brief 收到服务器命令时发出。
    /// \param command 命令名。
    /// \param payload JSON 对象负载。
    void commandReceived(QString command, QJsonObject payload);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket socket_;
    QByteArray buffer_;
};
