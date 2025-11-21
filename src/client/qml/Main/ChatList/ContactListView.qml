import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WeChatClient as AppTheme

Rectangle {
    id: root
    color: "transparent"

    readonly property var theme: AppTheme.Theme

    property int currentIndex: -1
    property string currentContactId: ""
    property string currentContactName: ""
    property string currentContactWeChatId: ""
    property string currentContactSignature: ""
    property string currentRequestStatus: ""
    property string currentContactUserId: ""
    property string currentRequestId: ""

    ListModel {
        id: newFriendsModel
    }

    ListModel {
        id: contactsModel
    }

    ListModel {
        id: groupsModel
    }

    // 同步后端好友列表 / 好友申请列表到本地模型。
    Connections {
        target: loginBackend

        function onFriendRequestsReset(requests) {
            newFriendsModel.clear()
            for (var i = 0; i < requests.length; ++i) {
                var r = requests[i]
                var name = (r.displayName || "").replace(/\s+/g, " ").trim()
                newFriendsModel.append({
                    type: "request",
                    userId: r.fromUserId,
                    requestId: r.requestId,
                    name: name,
                    avatarColor: "#a29bfe",
                    wechatId: r.account,
                    signature: "",
                    status: r.status === "PENDING" ? "waiting" : "added",
                    msg: r.helloMsg || ""
                })
            }
        }

        function onFriendsReset(friends) {
            contactsModel.clear()
            for (var i = 0; i < friends.length; ++i) {
                var f = friends[i]
                var name = (f.displayName || "").replace(/\s+/g, " ").trim()
                contactsModel.append({
                    type: "contact",
                    userId: f.userId,
                    requestId: "",
                    name: name,
                    avatarColor: "#4fbf73",
                    wechatId: f.account,
                    signature: f.signature || ""
                })
            }
        }

        function onConversationsReset(conversations) {
            groupsModel.clear()
            for (var i = 0; i < conversations.length; ++i) {
                var c = conversations[i]
                if (c.conversationType !== "GROUP")
                    continue
                var name = (c.title || "").replace(/\s+/g, " ").trim()
                groupsModel.append({
                    type: "group",
                    conversationId: c.conversationId,
                    wechatId: c.conversationId, // 复用字段以便选中逻辑
                    name: name,
                    avatarColor: "#4fbf73",
                    signature: ""
                })
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 0

            // --- New Friends Group ---
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                Rectangle {
                    id: newFriendsHeaderBg
                    anchors.fill: parent
                    color: headerMouseNewFriends.containsMouse
                           ? theme.chatListItemHover
                           : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 8

                        Label {
                            text: newFriendsList.visible ? "▾" : "▸"
                            color: theme.textSecondary
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Label {
                            text: qsTr("新的朋友")
                            color: theme.textSecondary
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            visible: newFriendsModel.count > 0
                            text: newFriendsModel.count
                            color: theme.textSecondary
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: headerMouseNewFriends
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: newFriendsList.visible = !newFriendsList.visible
                    }
                }
            }

            ListView {
                id: newFriendsList
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? contentHeight : 0
                visible: true
                interactive: false
                model: newFriendsModel
                delegate: contactDelegate
            }

            // --- Groups ---
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                Rectangle {
                    id: groupsHeaderBg
                    anchors.fill: parent
                    color: headerMouseGroups.containsMouse
                           ? theme.chatListItemHover
                           : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 8

                        Label {
                            text: groupsList.visible ? "▾" : "▸"
                            color: theme.textSecondary
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Label {
                            text: qsTr("群聊")
                            color: theme.textSecondary
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            visible: groupsModel.count > 0
                            text: groupsModel.count
                            color: theme.textSecondary
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: headerMouseGroups
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: groupsList.visible = !groupsList.visible
                    }
                }
            }

            ListView {
                id: groupsList
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? contentHeight : 0
                visible: true
                interactive: false
                model: groupsModel
                delegate: contactDelegate
            }

            // --- Contacts Group ---
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                Rectangle {
                    id: contactsHeaderBg
                    anchors.fill: parent
                    color: headerMouseContacts.containsMouse
                           ? theme.chatListItemHover
                           : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 8

                        Label {
                            text: contactsList.visible ? "▾" : "▸"
                            color: theme.textSecondary
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Label {
                            text: qsTr("联系人")
                            color: theme.textSecondary
                            font.pixelSize: 13
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            visible: contactsModel.count > 0
                            text: contactsModel.count
                            color: theme.textSecondary
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: headerMouseContacts
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: contactsList.visible = !contactsList.visible
                    }
                }
            }

            ListView {
                id: contactsList
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? contentHeight : 0
                visible: true
                interactive: false
                model: contactsModel
                delegate: contactDelegate
            }
        }
    }

    // Shared Delegate
    Component {
        id: contactDelegate
        Rectangle {
            id: delegateRoot
            width: ListView.view.width
            height: 68
            color: {
                var isSelected = (root.currentContactId === model.wechatId);
                return isSelected
                        ? theme.chatListItemSelected
                        : (hovered ? theme.chatListItemHover : theme.chatListItemNormal);
            }

            property bool hovered: false

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: delegateRoot.hovered = true
                onExited: delegateRoot.hovered = false
                onClicked: {
                    root.currentContactId = model.wechatId
                    root.currentContactName = model.name
                    root.currentContactWeChatId = model.wechatId
                    root.currentContactSignature = model.signature || ""
                    root.currentRequestStatus = model.status || ""
                    root.currentContactUserId = model.userId || ""
                    root.currentRequestId = model.requestId || ""
                    root.currentIndex = index // Just for compatibility
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Rectangle {
                    width: 42
                    height: 42
                    radius: 6
                    color: model.avatarColor

                    Text {
                        anchors.centerIn: parent
                        text: model.name.substring(0, 1)
                        color: "#ffffff"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: model.name
                        color: theme.textPrimary
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        visible: model.type === "request"
                        text: model.msg || ""
                        color: theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Status Tag for Requests
                Label {
                    visible: model.type === "request"
                    text: model.status === "waiting" ? "等待验证" : "已添加"
                    color: "#7d7d7d"
                    font.pixelSize: 12
                }
            }
        }
    }
}
