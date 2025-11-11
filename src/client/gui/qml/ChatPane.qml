import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: theme.bgChat

    property string title: ""
    property var model: []
    property var theme
    property real chatScale: 1.0
    // 新增：网络客户端代理（来自 App.qml 的 ChatClient）
    property var client

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ChatHeader { Layout.fillWidth: true; title: root.title; theme: root.theme }

        Rectangle { Layout.fillWidth: true; Layout.fillHeight: true; color: theme.bgChat
            MessageList { id: messageList; anchors.fill: parent; model: root.model; theme: root.theme; chatScale: root.chatScale }

            // TODO(gui-ux:new-msg-bar): 底部“新消息”浮条占位
            // 交互：当不在底部且有未读入站消息时显示；点击滚到底，并清除未读提示。
            // 当前为占位，不计算未读数量，默认隐藏；需要时根据 messageList.nearBottom & 未读计数控制 visible。
            Rectangle {
                id: newMsgBar
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12 * root.chatScale // UI-KNOB: 浮条边距
                radius: 14 * root.chatScale // UI-KNOB: 圆角
                color: "#2A2B2F" // UI-KNOB: 浮条底色（沿用 bgSearch）
                border.color: "#3A3B3F"
                border.width: 1
                visible: false // TODO(gui-ux:new-msg-bar): 根据未读&是否在底部切换
                height: 32 * root.chatScale // UI-KNOB: 浮条高度
                width: 120 * root.chatScale // UI-KNOB: 浮条宽度
                Row {
                    anchors.fill: parent
                    anchors.margins: 8 * root.chatScale
                    spacing: 6 * root.chatScale
                    Text { text: "新消息"; color: theme.textPrimary; font.pixelSize: 13 * root.chatScale }
                    // TODO(gui-ux:new-msg-bar): 在此显示未读条数 badge
                }
                MouseArea { anchors.fill: parent; onClicked: messageList.scrollToBottom() }
            }
        }

        // TODO(gui-model): 将右侧 model 切换为增量模型（ListModel/AbstractListModel），
        // 发送时显示“发送中/失败重试”状态，并在网络确认后更新。
        ComposerBar {
            id: composer
            Layout.fillWidth: true
            // 至少占右侧聊天面板高度的 35%，否则按内容与最小高度自适应
            Layout.preferredHeight: Math.max(root.height * 0.35, composer.implicitHeight) // UI-KNOB,HOT-PARAM(require-approval): 输入区占比（0.35）
            theme: root.theme
            scaleFactor: root.chatScale // UI-KNOB: 输入区缩放跟随聊天区
            // 输入区不放大，保持微信视觉比例；如需放大，可乘 chatScale
            onSend: function(text) {
                if (!text || text.trim().length === 0) return;
                // 仅转发到网络层，由服务器广播回显；
                // 不要本地修改 root.model，避免打断对 conv.currentModel 的绑定。
                if (root.client) { root.client.sendText(text); }
            }
        }
    }
}
