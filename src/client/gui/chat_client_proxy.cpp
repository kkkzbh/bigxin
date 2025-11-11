// 实现：在此导入 C++ 模块 client.core
#include "chat_client_proxy.h"

#include <QDebug>
#include <QPointer>

import std;
import client.core; // ChatClient / ChatEventHandler

// 前置声明，供上方 lambda 使用
static auto qstr_to_std_utf8(QString const& s) -> std::string;
static auto std_to_qstr_utf8(std::string const& s) -> QString;

ChatClientProxy::ChatClientProxy(QObject* parent)
    : QObject(parent) {}

ChatClientProxy::~ChatClientProxy() {
    if (client_) {
        client_->close();
        client_.reset();
    }
}

void ChatClientProxy::setHost(QString h) {
    if (host_ == h) return;
    host_ = std::move(h);
    emit hostChanged();
}

void ChatClientProxy::setPort(int p) {
    auto np = static_cast<quint16>(std::clamp(p, 0, 65535));
    if (port_ == np) return;
    port_ = np;
    emit portChanged();
}

void ChatClientProxy::setNickname(QString n) {
    if (nickname_ == n) return;
    nickname_ = std::move(n);
    emit nicknameChanged();
}

void ChatClientProxy::setAutoConnect(bool v) {
    if (auto_connect_ == v) return;
    auto_connect_ = v;
    emit autoConnectChanged();
}

void ChatClientProxy::ensureClient() {
    if (client_) return;
    client_ = std::make_shared<ChatClient>();
    ChatEventHandler h{};
    QPointer<ChatClientProxy> that(this);
    // 保证跨线程安全：统一切回 GUI 线程再发信号；并取消“已登录”气泡
    // NOTE: 这里不再注入“已登录”消息，遵循需求。
    h.on_hello_ack = [that](std::string const& /*user*/){
        if (!that) return;
        QMetaObject::invokeMethod(that, [that]{
            if (!that) return;
            emit that->connected();
        }, Qt::QueuedConnection);
    };
    h.on_broadcast = [that](std::string const& user, std::string const& text){
        if (!that) return;
        auto quser = std_to_qstr_utf8(user);
        auto qtext = std_to_qstr_utf8(text);
        QMetaObject::invokeMethod(that, [that, quser, qtext]{
            if (!that) return;
            emit that->messageAdded(quser, qtext);
        }, Qt::QueuedConnection);
    };
    h.on_error = [that](std::string const& msg){
        if (!that) return;
        auto qmsg = std_to_qstr_utf8(msg);
        QMetaObject::invokeMethod(that, [that, qmsg]{
            if (!that) return;
            emit that->errorOccurred(qmsg);
        }, Qt::QueuedConnection);
    };
    h.on_closed = [that]{
        if (!that) return;
        QMetaObject::invokeMethod(that, [that]{
            if (!that) return;
            emit that->disconnected();
        }, Qt::QueuedConnection);
    };
    h.on_connected = [that]{
        if (!that) return;
        QMetaObject::invokeMethod(that, [that]{
            if (!that) return;
            emit that->connected();
            that->doHelloIfNeeded();
        }, Qt::QueuedConnection);
    };
    client_->set_handler(std::move(h));

    if (auto_connect_) {
        connectToServer();
    }
}

static auto qstr_to_std_utf8(QString const& s) -> std::string {
    auto const b = s.toUtf8();
    return std::string(b.constData(), static_cast<std::size_t>(b.size()));
}

static auto std_to_qstr_utf8(std::string const& s) -> QString {
    return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

void ChatClientProxy::connectToServer() {
    ensureClient();
    client_->connect(qstr_to_std_utf8(host_), static_cast<std::uint16_t>(port_));
}

void ChatClientProxy::sendText(QString text) {
    ensureClient();
    if (text.isEmpty()) return;
    client_->post(qstr_to_std_utf8(text));
}

void ChatClientProxy::doHelloIfNeeded() {
    if (!client_) return;
    if (!nickname_.isEmpty()) {
        client_->hello(qstr_to_std_utf8(nickname_));
    }
}
