import QtQuick
import QtQuick.Controls
import WeChatClient

Menu {
    id: root
    modal: true
    implicitWidth: 168
    padding: 6

    readonly property var theme: Theme

    property string conversationId: ""
    property string targetUserId: ""
    property string targetUserName: ""
    property bool isMuted: false
    property string targetRole: ""
    property string myRole: ""
    
    signal muteRequested()
    signal unmuteRequested()
    signal setAdminRequested(bool isAdmin)

    background: Rectangle {
        implicitWidth: root.implicitWidth
        implicitHeight: contentItem.implicitHeight + root.padding * 2
        color: theme.panelBackground
        radius: 8
        border.color: theme.separatorHorizontal
        border.width: 1
    }

    MenuItem {
        id: muteItem
        text: root.isMuted ? qsTr("解禁") : qsTr("禁言")
        visible: root.targetRole !== "OWNER"  // 不能禁言群主
        height: visible ? implicitHeight : 0  // 不可见时不占据空间
        implicitWidth: root.implicitWidth - root.padding * 2
        implicitHeight: 38
        leftPadding: 12
        rightPadding: 12
        topPadding: 8
        bottomPadding: 8
        
        contentItem: Label {
            text: muteItem.text
            color: theme.textPrimary
            font.pixelSize: 14
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        
        background: Rectangle {
            color: muteItem.highlighted ? theme.chatListItemHover : "transparent"
            radius: 6
        }

        onTriggered: {
            if (root.isMuted) {
                root.unmuteRequested()
            } else {
                root.muteRequested()
            }
        }
    }

    MenuItem {
        id: adminItem
        text: root.targetRole === "ADMIN" ? qsTr("取消管理员") : qsTr("设为管理员")
        visible: root.myRole === "OWNER"  // 只有群主可以设置管理员
        height: visible ? implicitHeight : 0  // 不可见时不占据空间
        implicitWidth: root.implicitWidth - root.padding * 2
        implicitHeight: 38
        leftPadding: 12
        rightPadding: 12
        topPadding: 8
        bottomPadding: 8
        
        contentItem: Label {
            text: adminItem.text
            color: theme.textPrimary
            font.pixelSize: 14
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
        }
        
        background: Rectangle {
            color: adminItem.highlighted ? theme.chatListItemHover : "transparent"
            radius: 6
        }

        onTriggered: {
            root.setAdminRequested(root.targetRole !== "ADMIN")
        }
    }
}
