import QtQuick
import QtQuick.Controls

import WeChatClient as AppTheme

Menu {
    id: root

    readonly property var theme: AppTheme.Theme
    // 外部注入的发起群聊对话框引用
    property var startGroupDialog

    implicitWidth: 160

    background: Rectangle {
        radius: 6
        color: theme.chatAreaBackground
        border.color: theme.separatorVertical
    }

    MenuItem {
        id: startGroupItem
        text: qsTr("发起群聊")
        background: Rectangle {
            radius: 4
            color: (startGroupItem.hovered || startGroupItem.highlighted)
                   ? theme.primaryButton
                   : "transparent"
        }
        contentItem: Label {
            text: startGroupItem.text
            color: (startGroupItem.hovered || startGroupItem.highlighted)
                   ? "#ffffff"
                   : theme.textPrimary
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 12
            rightPadding: 12
        }
        onTriggered: {
            if (root.startGroupDialog) {
                root.startGroupDialog.resetAndOpen()
            }
        }
    }

    MenuItem {
        id: addFriendItem
        text: qsTr("添加好友")
        background: Rectangle {
            radius: 4
            color: (addFriendItem.hovered || addFriendItem.highlighted)
                   ? theme.primaryButton
                   : "transparent"
        }
        contentItem: Label {
            text: addFriendItem.text
            color: (addFriendItem.hovered || addFriendItem.highlighted)
                   ? "#ffffff"
                   : theme.textPrimary
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 12
            rightPadding: 12
        }
        onTriggered: {
            if (addFriendDialog) {
                addFriendDialog.resetState()
                addFriendDialog.visible = true
                addFriendDialog.raise()
                addFriendDialog.requestActivate()
            }
        }
    }
}
