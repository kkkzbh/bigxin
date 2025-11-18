
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <print>

#include <login.backend.h>

/// \brief 简单的 Qt Quick 客户端入口，先进入登录界面。
/// \details 使用 qt_add_qml_module 注册的 QML 模块，从资源中加载 Login。
auto main(int argc, char** argv) -> int
{
    QGuiApplication app{ argc, argv };

    QQmlApplicationEngine engine;

    auto* backend = new LoginBackend{ &app };
    engine.rootContext()->setContextProperty("loginBackend", backend);

    engine.loadFromModule("WeChatClient", "Login");

    if(engine.rootObjects().isEmpty()) {
        std::println("failed to load QML module WeChatClient.Login");
        return -1;
    }

    return app.exec();
}
