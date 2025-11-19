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
                    }
                    Label {
                        text: labelText
                        color: "#999999"
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                ActionButton { iconText: "💬"; labelText: "发消息" }
                ActionButton { iconText: "📞"; labelText: "语音聊天" }
                ActionButton { iconText: "📹"; labelText: "视频聊天" }
            }

            // Agree Button
            Button {
                visible: root.requestStatus === "waiting"
                text: "同意"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 120
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
                    console.log("Accepted friend request")
                }
            }

            // Add to Contacts Button
            Button {
                visible: root.isStranger
                text: "添加到通讯录"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
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
                    console.log("Add friend request sent")
                }
            }
        }
    }
}
