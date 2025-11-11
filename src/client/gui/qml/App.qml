import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Chat 1.0

ApplicationWindow {
    id: win
    width: 1405
    height: 985
    visible: true
    color: theme.bgChat
    title: "Chat GUI"
    // 全局主题 Token：改为实例化 Theme.qml，便于集中修改与复用
    Theme { id: theme } // UI-KNOB: 所有颜色/基础尺寸在 Theme.qml 中统一调整

    // 缩放因子：侧栏/会话列表；聊天头像与气泡
    property real sideScale: 1.2 // UI-KNOB,HOT-PARAM(require-approval): 左侧导航与会话列表缩放倍数（请勿擅改）
    property real chatScale: 1.2 // UI-KNOB,HOT-PARAM(require-approval): 聊天区缩放倍数（请勿擅改）

    RowLayout {
        anchors.fill: parent

        // 全局网络客户端（暂定：开窗即连 127.0.0.1:7777，并发送 /nick kkkzbh_gui）
        ChatClient { // UI-KNOB: 临时固定的 GUI 直连配置，未来接入登录页
            id: chat
            host: "127.0.0.1"   // UI-KNOB: 默认主机（本地）
            port: 7777          // UI-KNOB: 默认端口
            nickname: "kkkzbh_gui" // UI-KNOB: 默认昵称
            autoConnect: true   // UI-KNOB: 开窗自动连接并发送 /nick
            onErrorOccurred: (m) => console.warn("[net]", m)
        }
        // 确保开窗即发起连接（避免等到首次发送才连接）
        Component.onCompleted: chat.connectToServer()

        // 监听网络层入站消息，统一注入置顶会话“世界”
        // TODO(gui-routing): 登录/房间上线后，按房间/会话 ID 路由到对应列表项，
        // 并维护每个会话的未读数与最后一条摘要。
        Connections {
            target: chat
            function onMessageAdded(user, text) {
                conv.appendNetworkMessage(user, text, chat.nickname)
            }
        }

        // 左：导航栏
        NavRail {
            id: nav
            Layout.preferredWidth: theme.railW * sideScale // UI-KNOB: 左侧导航宽度 = railW × sideScale
            Layout.fillHeight: true
            theme: theme
            scaleFactor: sideScale
            iconSource: "qrc:/qt/qml/chat/gui/JX_CUT.svg"
        }

        // 中：会话列表
        ConversationList {
            id: conv
            Layout.preferredWidth: theme.listW * sideScale // UI-KNOB: 中栏会话列表宽度 = listW × sideScale
            Layout.fillHeight: true
            theme: theme
            scaleFactor: sideScale
        }

        // 右：聊天面板
        ChatPane {
            id: pane
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: conv.currentTitle
            model: conv.currentModel
            theme: theme
            // 修复BUG：此处必须显式从窗口作用域取值，避免 "chatScale: chatScale" 自引用导致绑定失效
            chatScale: win.chatScale // UI-KNOB: 透传聊天区缩放倍数
            client: chat          // 接入网络客户端，发送文本
        }
    }
}
