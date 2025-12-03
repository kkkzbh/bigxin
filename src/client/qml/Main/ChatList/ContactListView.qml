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
    property string currentRequestType: ""
    property string currentGroupName: ""
    property string currentGroupId: ""

    // 根据当前选中联系人 ID / 用户 ID / 请求 ID
    // 在最新的模型中同步右侧详情面板的数据。
    function refreshCurrentSelection() {
        if (!currentContactId && !currentContactUserId && !currentRequestId) {
            return
        }

        // 优先使用“联系人”列表中的数据（已经成为好友时，以好友信息为准）。
        for (var i = 0; i < contactsModel.count; ++i) {
            var c = contactsModel.get(i)
            if ((currentContactUserId && c.userId === currentContactUserId)
                    || (currentContactId && c.wechatId === currentContactId)) {
                currentContactId = c.wechatId
                currentContactName = c.name
                currentContactWeChatId = c.wechatId
                currentContactSignature = c.signature || ""
                // 好友视图不再展示“等待验证”，清空状态即可。
                currentRequestStatus = ""
                currentContactUserId = c.userId
                currentRequestId = ""
                return
            }
        }

        // 若还不是好友，则尝试从“新的朋友”列表中同步状态（例如 PENDING -> 已添加）。
        for (var j = 0; j < newFriendsModel.count; ++j) {
            var r = newFriendsModel.get(j)
            if ((currentRequestId && r.requestId === currentRequestId)
                    || (currentContactUserId && r.userId === currentContactUserId)
                    || (currentContactId && r.wechatId === currentContactId)) {
                currentContactId = r.wechatId
                currentContactName = r.name
                currentContactWeChatId = r.wechatId
                currentContactSignature = ""
                currentRequestStatus = r.status || ""
                currentContactUserId = r.userId
                currentRequestId = r.requestId
                return
            }
        }
    }

    ListModel {
        id: newFriendsModel
    }

    ListModel {
        id: contactsModel
    }

    ListModel {
        id: groupsModel
    }

    // 缓存好友申请和入群申请列表
    property var cachedFriendRequests: []
    property var cachedGroupJoinRequests: []

    // 合并重建"新的朋友"模型
    function rebuildNewFriendsModel(friendReqs, groupReqs) {
        if (friendReqs !== null) cachedFriendRequests = friendReqs
        if (groupReqs !== null) cachedGroupJoinRequests = groupReqs

        newFriendsModel.clear()

        // 添加好友申请
        for (var i = 0; i < cachedFriendRequests.length; ++i) {
            var r = cachedFriendRequests[i]
            var name = (r.displayName || "").replace(/\s+/g, " ").trim()
            newFriendsModel.append({
                type: "friendRequest",
                userId: r.fromUserId,
                requestId: r.requestId,
                name: name,
                avatarColor: "#a29bfe",
                wechatId: r.account,
                signature: "",
                status: r.status === "PENDING" ? "waiting" : "added",
                msg: r.helloMsg || "",
                groupId: "",
                groupName: ""
            })
        }

        // 添加入群申请
        for (var j = 0; j < cachedGroupJoinRequests.length; ++j) {
            var g = cachedGroupJoinRequests[j]
            var gname = (g.displayName || "").replace(/\s+/g, " ").trim()
            var statusText = "waiting"
            if (g.status === "ACCEPTED") statusText = "accepted"
            else if (g.status === "REJECTED") statusText = "rejected"
            else if (g.status === "PENDING") statusText = "waiting"

            newFriendsModel.append({
                type: "groupJoinRequest",
                userId: g.fromUserId,
                requestId: g.requestId,
                name: gname,
                avatarColor: "#74b9ff",
                wechatId: g.account,
                signature: "",
                status: statusText,
                msg: g.helloMsg || qsTr("申请加入群聊: ") + (g.groupName || ""),
                groupId: g.groupId || "",
                groupName: g.groupName || ""
            })
        }

        refreshCurrentSelection()
    }

    // 同步后端好友列表 / 好友申请列表 / 入群申请列表到本地模型。
    Connections {
        target: loginBackend

        function onFriendRequestsReset(requests) {
            rebuildNewFriendsModel(requests, null)
        }

        function onGroupJoinRequestsReset(requests) {
            rebuildNewFriendsModel(null, requests)
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
            // 好友列表刷新后，优先使用好友信息更新当前详情。
            refreshCurrentSelection()
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
                    root.currentRequestType = model.type || ""
                    root.currentGroupName = model.groupName || ""
                    root.currentGroupId = model.groupId || ""
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
                        visible: model.type === "friendRequest" || model.type === "groupJoinRequest"
                        text: model.msg || ""
                        color: theme.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Status Tag for Requests
                Label {
                    visible: model.type === "friendRequest" || model.type === "groupJoinRequest"
                    text: {
                        if (model.type === "friendRequest") {
                            return model.status === "waiting" ? qsTr("等待验证") : qsTr("已添加")
                        } else if (model.type === "groupJoinRequest") {
                            if (model.status === "waiting") return qsTr("待处理")
                            if (model.status === "accepted") return qsTr("已同意")
                            if (model.status === "rejected") return qsTr("已拒绝")
                            return ""
                        }
                        return ""
                    }
                    color: "#7d7d7d"
                    font.pixelSize: 12
                }
            }
        }
    }
}
