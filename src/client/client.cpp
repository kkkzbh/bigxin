
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>

#include <print>

#include <login.backend.h>

/// \brief 简单的 Qt Quick 客户端入口，先进入登录界面。
/// \details 使用 qt_add_qml_module 注册的 QML 模块，从资源中加载 Login。
///          支持 --host 和 --port 命令行参数指定服务器地址。
auto main(int argc, char** argv) -> int
{
    QGuiApplication app{ argc, argv };
    app.setApplicationName(QStringLiteral("JuXinClient"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    // 命令行参数解析
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("JuXin 聊天客户端"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption hostOption(
        QStringList() << QStringLiteral("H") << QStringLiteral("host"),
        QStringLiteral("服务器地址 (默认: 127.0.0.1)"),
        QStringLiteral("host"),
        QStringLiteral("127.0.0.1")
    );
    parser.addOption(hostOption);

    QCommandLineOption portOption(
        QStringList() << QStringLiteral("P") << QStringLiteral("port"),
        QStringLiteral("服务器端口 (默认: 5555)"),
        QStringLiteral("port"),
        QStringLiteral("5555")
    );
    parser.addOption(portOption);

    parser.process(app);

    QString host = parser.value(hostOption);
    quint16 port = static_cast<quint16>(parser.value(portOption).toUInt());

    std::println("Connecting to server at {}:{}", host.toStdString(), port);

    QQmlApplicationEngine engine;

    auto* backend = new LoginBackend{ host, port, &app };
    engine.rootContext()->setContextProperty("loginBackend", backend);

    engine.loadFromModule("WeChatClient", "Login");

    if(engine.rootObjects().isEmpty()) {
        std::println("failed to load QML module WeChatClient.Login");
        return -1;
    }

    return app.exec();
}
