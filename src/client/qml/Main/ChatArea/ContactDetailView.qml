import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WeChatClient as AppTheme

Rectangle {
    id: root
    color: "transparent" // Background handled by parent

    readonly property var theme: AppTheme.Theme

    property bool hasSelection: false
    property string contactName: ""
    property string contactWeChatId: ""
    property string contactSignature: ""
    property string requestStatus: ""
    property string contactRegion: ""
    property bool isStranger: false
    // 联系人用户 ID（用于发消息 / 添加好友等操作）。
    property string contactUserId: ""
    // 好友申请 ID（用于"同意"按钮）。
    property string requestId: ""
    // 是否已经向该用户发送过好友申请（用于禁用"添加到通讯录"按钮）。
    property bool hasPendingRequest: false
    // 请求类型：friendRequest / groupJoinRequest / 空
    property string requestType: ""
    // 入群申请的目标群名称
    property string groupName: ""
    // 入群申请的目标群ID
    property string groupId: ""

    // Content
    Item {
        anchors.fill: parent
        visible: root.hasSelection

        ColumnLayout {
            anchors.centerIn: parent
            width: 360
            spacing: 0

            // Top Info Area
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 30
                spacing: 20

                // Avatar
                Rectangle {
                    width: 64
                    height: 64
                    radius: 6
                    color: "#cccccc"
                    Text {
                        anchors.centerIn: parent
                        text: root.contactName ? root.contactName.substring(0, 1) : ""
                        font.pixelSize: 24
                        font.bold: true
                        color: "white"
                    }
                }

                // Name & ID
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    RowLayout {
                        Label {
                            text: root.contactName
                            font.pixelSize: 20
                            font.bold: true
                            color: "#f5f5f5" // Dark theme text
                        }
                    }

                    Label {
                        text: "微信号: " + root.contactWeChatId
                        font.pixelSize: 14
                        color: "#999999"
                    }

                    Label {
                        visible: root.contactRegion !== ""
                        text: "地区: " + root.contactRegion
                        font.pixelSize: 14
                        color: "#999999"
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#333333" // Separator
                Layout.bottomMargin: 20
            }

            // Signature
            RowLayout {
                Layout.fillWidth: true
                Layout.bottomMargin: 40
                spacing: 10
                Label {
                    text: "个性签名"
                    color: "#999999"
                    font.pixelSize: 14
                    Layout.preferredWidth: 60
                    Layout.alignment: Qt.AlignTop
                }
                Label {
                    text: root.contactSignature || "未填写"
                    color: "#f5f5f5"
                    font.pixelSize: 14
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
            }

            // Buttons
            RowLayout {
                visible: root.requestStatus !== "waiting" && !root.isStranger
                Layout.alignment: Qt.AlignHCenter
                spacing: 40

                // Helper to create icon buttons
                component ActionButton: ColumnLayout {
                    property string iconText
                    property string labelText
                    signal triggered()
                    spacing: 8
                    Rectangle {
                        width: 50
                        height: 50
                        radius: 25 // Circle
                        color: "#2b2b2b" // Button bg
                        border.color: "#3a3a3a"

                        Text {
                            anchors.centerIn: parent
                            text: iconText
                            color: "#4fbf73" // Green accent
                            font.pixelSize: 20
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: triggered()
                        }
                    }
                    Label {
                        text: labelText
                        color: "#999999"
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                ActionButton {
                    iconText: "💬"
                    labelText: "发消息"
                    onTriggered: {
                        console.log("[发消息] requestType:", root.requestType,
                                    "contactUserId:", root.contactUserId,
                                    "contactWeChatId:", root.contactWeChatId)
                        // 群聊：使用 contactWeChatId（存储的是 conversationId）
                        if (root.requestType === "group") {
                            if (root.contactWeChatId && root.contactWeChatId !== "") {
                                console.log("[发消息] 打开群聊会话:", root.contactWeChatId)
                                loginBackend.openConversation(root.contactWeChatId)
                            }
                        } else {
                            // 好友：使用 contactUserId
                            if (root.contactUserId && root.contactUserId !== "") {
                                console.log("[发消息] 打开单聊会话, peerUserId:", root.contactUserId)
                                loginBackend.openSingleConversation(root.contactUserId)
                            } else {
                                console.log("[发消息] contactUserId 为空，无法打开单聊")
                            }
                        }
                    }
                }
                ActionButton { iconText: "📞"; labelText: "语音聊天" }
                ActionButton { iconText: "📹"; labelText: "视频聊天" }
            }

            // Agree/Reject Buttons (for friend requests)
            RowLayout {
                visible: root.requestStatus === "waiting" && root.requestType === "friendRequest"
                Layout.alignment: Qt.AlignHCenter
                spacing: 20

                Button {
                    text: qsTr("同意")
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 36
                    background: Rectangle {
                        color: "#4fbf73"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        console.log("[同意好友申请] requestId:", root.requestId, "requestType:", root.requestType)
                        if (root.requestId && root.requestId !== "") {
                            console.log("[同意好友申请] 调用 acceptFriendRequest")
                            loginBackend.acceptFriendRequest(root.requestId)
                        } else {
                            console.log("[同意好友申请] requestId 为空")
                        }
                    }
                }

                Button {
                    text: qsTr("拒绝")
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 36
                    background: Rectangle {
                        color: "#e74c3c"
                        radius: 4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        console.log("[拒绝好友申请] requestId:", root.requestId, "requestType:", root.requestType)
                        if (root.requestId && root.requestId !== "") {
                            console.log("[拒绝好友申请] 调用 rejectFriendRequest")
                            loginBackend.rejectFriendRequest(root.requestId)
                        } else {
                            console.log("[拒绝好友申请] requestId 为空")
                        }
                    }
                }
            }

            // 好友申请已处理的状态标签
            Label {
                visible: root.requestType === "friendRequest" && root.requestStatus === "accepted"
                text: qsTr("已添加")
                color: "#4fbf73"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                visible: root.requestType === "friendRequest" && root.requestStatus === "rejected"
                text: qsTr("已拒绝")
                color: "#e74c3c"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }

            // Buttons for group join requests
            ColumnLayout {
                visible: root.requestType === "groupJoinRequest"
                Layout.alignment: Qt.AlignHCenter
                spacing: 12

                Label {
                    visible: root.groupName !== ""
                    text: qsTr("申请加入群聊: ") + root.groupName
                    color: theme.textSecondary
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    visible: root.requestStatus === "waiting"
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 20

                    Button {
                        text: qsTr("同意")
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        background: Rectangle {
                            color: "#4fbf73"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (root.requestId && root.requestId !== "") {
                                loginBackend.acceptGroupJoinRequest(root.requestId, true)
                            }
                        }
                    }

                    Button {
                        text: qsTr("拒绝")
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        background: Rectangle {
                            color: "#e74c3c"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (root.requestId && root.requestId !== "") {
                                loginBackend.acceptGroupJoinRequest(root.requestId, false)
                            }
                        }
                    }
                }

                Label {
                    visible: root.requestStatus === "accepted"
                    text: qsTr("已同意该申请")
                    color: "#4fbf73"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    visible: root.requestStatus === "rejected"
                    text: qsTr("已拒绝该申请")
                    color: "#e74c3c"
                    font.pixelSize: 14
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Add to Contacts Button
            Button {
                visible: root.isStranger
                text: root.hasPendingRequest ? "已发送" : "添加到通讯录"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 36
                enabled: !root.hasPendingRequest
                background: Rectangle {
                    color: root.hasPendingRequest ? "#3a3a3a" : "#4fbf73"
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (root.contactUserId && root.contactUserId !== "") {
                        loginBackend.sendFriendRequest(root.contactUserId, "")
                    }
                }
            }
        }
    }
}
