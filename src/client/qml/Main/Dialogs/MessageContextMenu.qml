import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import WeChatClient as AppTheme

Menu {
    id: root
    modal: true
    implicitWidth: 168
    padding: 6

    readonly property var theme: AppTheme.Theme

    // 消息相关属性
    property bool isMyMessage: false
    property bool isGroupChat: false
    property string myRole: ""  // OWNER/ADMIN/MEMBER
    property string messageContent: ""
    property string serverMsgId: ""
    property string conversationId: ""
    
    // 反应状态
    property bool hasMyLike: false
    property bool hasMyDislike: false

    // 信号
    signal copyRequested()
    signal recallRequested()
    signal likeRequested()
    signal dislikeRequested()
    signal unlikeRequested()
    signal undislikeRequested()

    background: Rectangle {
        id: menuBackground
        color: theme.panelBackground
        radius: 8
        border.color: theme.cardBorder
        border.width: 1

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#40000000"
            shadowBlur: 0.5
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 2
        }
    }

    // 复制
    MenuItem {
        text: qsTr("复制")
        implicitHeight: 32
        font.pixelSize: 13

        background: Rectangle {
            color: parent.hovered ? theme.chatListItemSelected : "transparent"
            radius: 4
        }

        contentItem: Text {
            text: parent.text
            color: theme.textPrimary
            font: parent.font
            verticalAlignment: Text.AlignVCenter
        }

        onTriggered: root.copyRequested()
    }

    // 点赞（仅对方消息）
    MenuItem {
        visible: !root.isMyMessage && root.serverMsgId !== ""
        enabled: root.serverMsgId !== ""
        height: visible ? 32 : 0
        text: root.hasMyLike ? qsTr("取消 👍") : qsTr("👍 点赞")
        implicitHeight: 32
        font.pixelSize: 13

        Component.onCompleted: {
            console.log("[MessageMenu] Like item - isMyMessage:", root.isMyMessage, "serverMsgId:", root.serverMsgId, "visible:", visible)
        }

        background: Rectangle {
            color: parent.hovered ? theme.chatListItemSelected : "transparent"
            radius: 4
        }

        contentItem: Text {
            text: parent.text
            color: theme.textPrimary
            font: parent.font
            verticalAlignment: Text.AlignVCenter
        }

        onTriggered: {
            console.log("[MessageMenu] Like MenuItem triggered - hasMyLike:", root.hasMyLike)
            if (root.hasMyLike) {
                root.unlikeRequested()
            } else {
                root.likeRequested()
            }
        }
    }

    // 点踩（仅对方消息）
    MenuItem {
        visible: !root.isMyMessage && root.serverMsgId !== ""
        enabled: root.serverMsgId !== ""
        height: visible ? 32 : 0
        text: root.hasMyDislike ? qsTr("取消 👎") : qsTr("👎 点踩")
        implicitHeight: 32
        font.pixelSize: 13

        background: Rectangle {
            color: parent.hovered ? theme.chatListItemSelected : "transparent"
            radius: 4
        }

        contentItem: Text {
            text: parent.text
            color: theme.textPrimary
            font: parent.font
            verticalAlignment: Text.AlignVCenter
        }

        onTriggered: {
            console.log("[MessageMenu] Dislike MenuItem triggered - hasMyDislike:", root.hasMyDislike)
            if (root.hasMyDislike) {
                root.undislikeRequested()
            } else {
                root.dislikeRequested()
            }
        }
    }

    // 分隔线
    MenuSeparator {
        visible: (root.isMyMessage || (root.isGroupChat && (root.myRole === "OWNER" || root.myRole === "ADMIN"))) && root.serverMsgId !== ""
        height: visible ? implicitHeight : 0
        padding: 0
        topPadding: 4
        bottomPadding: 4
        contentItem: Rectangle {
            implicitHeight: 1
            color: theme.separatorHorizontal
        }
    }

    // 撤回（自己的消息 或 群主/管理员）
    MenuItem {
        visible: (root.isMyMessage || (root.isGroupChat && (root.myRole === "OWNER" || root.myRole === "ADMIN"))) && root.serverMsgId !== ""
        enabled: root.serverMsgId !== ""
        height: visible ? 32 : 0
        text: qsTr("撤回")
        implicitHeight: 32
        font.pixelSize: 13

        background: Rectangle {
            color: parent.hovered ? "#33e74c3c" : "transparent"
            radius: 4
        }

        contentItem: Text {
            text: parent.text
            color: theme.dangerRed
            font: parent.font
            verticalAlignment: Text.AlignVCenter
        }

        onTriggered: root.recallRequested()
    }
}
