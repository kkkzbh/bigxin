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
    
    signal muteRequested()
    signal unmuteRequested()

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
}
