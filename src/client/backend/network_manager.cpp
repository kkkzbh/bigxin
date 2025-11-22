#include "network_manager.h"

#include <QJsonDocument>
#include <print>

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent)
{
    connect(&socket_, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &NetworkManager::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &NetworkManager::onErrorOccurred);
}

auto NetworkManager::isConnected() const -> bool
{
    return socket_.state() == QAbstractSocket::ConnectedState;
}

auto NetworkManager::connectToServer(QString const& host, quint16 port) -> void
{
    if(socket_.state() == QAbstractSocket::ConnectedState) {
        return;
    }

    socket_.abort();
    socket_.connectToHost(host, port);
}

auto NetworkManager::disconnect() -> void
{
    socket_.disconnectFromHost();
}

auto NetworkManager::sendCommand(QString const& command, QJsonObject const& payload) -> void
{
    if(socket_.state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonDocument doc{ payload };
    auto const json = doc.toJson(QJsonDocument::Compact);

    QByteArray line;
    line.reserve(command.toUtf8().size() + 1 + json.size() + 1);
    line.append(command.toUtf8());
    line.append(':');
    line.append(json);
    line.append('\n');

    socket_.write(line);
}

void NetworkManager::onConnected()
{
    emit connected();
}

void NetworkManager::onDisconnected()
{
    emit disconnected();
}

void NetworkManager::onReadyRead()
{
    buffer_.append(socket_.readAll());

    while(true) {
        auto const index = buffer_.indexOf('\n');
        if(index < 0) {
            break;
        }

        auto const line = QString::fromUtf8(buffer_.constData(), index);
        buffer_.remove(0, index + 1);

        // 解析 "COMMAND:{...}" 格式
        auto const colon = line.indexOf(u':');
        if(colon <= 0) {
            continue;
        }

        auto const command = line.left(colon);
        auto const jsonText = line.mid(colon + 1).toUtf8();

        auto const doc = QJsonDocument::fromJson(jsonText);
        if(!doc.isObject()) {
            continue;
        }

        emit commandReceived(command, doc.object());
    }
}

void NetworkManager::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    std::println("NetworkManager socket error: {}", static_cast<int>(socketError));
    emit errorOccurred(socket_.errorString());
}
