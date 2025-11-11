

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "chat_client_proxy.h"

auto main(int argc, char** argv) -> int
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    // clang-format off  // 文档优先：多行参数时，函数名与 '(' 之间留 1 个空格
    QObject::connect (
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [](QObject* obj, QUrl const&) {
            if (not obj) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection
    );
    // clang-format on
    // 注册 QML 类型 ChatClient（命名空间 Chat 1.0）
    qmlRegisterType<ChatClientProxy>("Chat", 1, 0, "ChatClient");

    engine.loadFromModule("chat.gui", "App");

    return app.exec();
}
