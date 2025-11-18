import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../.." as AppTheme

Rectangle {
    id: root

    AppTheme.Theme {
        id: theme
    }

    color: theme.chatAreaBackground

    // 外部通过 Main.qml 传入是否有选中会话
    property bool hasSelection: true

    // 正常聊天界面，仅在有选中会话时显示
    ColumnLayout {
        id: chatLayout
        visible: root.hasSelection
        anchors.fill: parent
        spacing: 0

        // 中间消息区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: theme.chatAreaBackground

                ListView {
                    id: messageList
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16
                    model: messageModel
                    clip: true
                    Component.onCompleted: positionViewAtEnd()

                delegate: Item {
                    width: ListView.view.width
                    height: Math.max(leftRow.implicitHeight, rightRow.implicitHeight)

                    property bool isMine: sender === "me"

                    // 其他人消息：头像在左，气泡在右，整体靠左
                    Row {
                        id: leftRow
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        visible: !isMine

                        Rectangle {
                            width: 36
                            height: 36
                            radius: 6
                            color: "#4fbf73"

                            Text {
                                anchors.centerIn: parent
                                text: "TA"
                                color: "#ffffff"
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Rectangle {
                            id: leftBubble
                            color: theme.bubbleOther
                            radius: 6
                            border.color: theme.bubbleOther
                            implicitWidth: leftText.implicitWidth + 20
                            implicitHeight: leftText.implicitHeight + 14

                            Text {
                                id: leftText
                                anchors.margins: 8
                                anchors.fill: parent
                                text: content
                                color: theme.bubbleOtherText
                                font.pixelSize: 14
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    // 自己的消息：气泡在左，头像在右，整体靠右
                    Row {
                        id: rightRow
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        visible: isMine

                        Rectangle {
                            id: rightBubble
                            color: theme.bubbleMine
                            radius: 6
                            border.color: theme.bubbleMine
                            implicitWidth: rightText.implicitWidth + 20
                            implicitHeight: rightText.implicitHeight + 14

                            Text {
                                id: rightText
                                anchors.margins: 8
                                anchors.fill: parent
                                text: content
                                color: theme.bubbleMineText
                                font.pixelSize: 14
                                wrapMode: Text.Wrap
                            }
                        }

                        Rectangle {
                            width: 36
                            height: 36
                            radius: 6
                            color: "#ffffff"

                            Text {
                                anchors.centerIn: parent
                                text: "我"
                                color: "#222222"
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }

        // 底部输入区域
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 210
            color: theme.chatAreaBackground
            border.color: theme.separatorHorizontal
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ToolButton {
                        text: "😊"
                        background: null
                    }
                    ToolButton {
                        text: "📎"
                        background: null
                    }
                    ToolButton {
                        text: "💻"
                        background: null
                    }
                }

                TextArea {
                    id: inputArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    wrapMode: TextEdit.Wrap
                    color: theme.textPrimary
                    font.pixelSize: 20
                    placeholderText: ""
                    background: Rectangle {
                        radius: 4
                        color: theme.chatAreaBackground
                    }

                    Keys.onReturnPressed: function(event) {
                        if (event.modifiers & Qt.ShiftModifier) {
                            inputArea.text = inputArea.text + "\n"
                            inputArea.cursorPosition = inputArea.text.length
                        } else {
                            if (inputArea.text.length > 0) {
                                loginBackend.sendWorldTextMessage(inputArea.text)
                                inputArea.text = ""
                            }
                        }
                        event.accepted = true
                    }

                    Keys.onEnterPressed: function(event) {
                        if (event.modifiers & Qt.ShiftModifier) {
                            inputArea.text = inputArea.text + "\n"
                            inputArea.cursorPosition = inputArea.text.length
                        } else {
                            if (inputArea.text.length > 0) {
                                loginBackend.sendWorldTextMessage(inputArea.text)
                                inputArea.text = ""
                            }
                        }
                        event.accepted = true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("发送(S)")
                        implicitWidth: 96
                        implicitHeight: 32
                        enabled: inputArea.text.length > 0
                        background: Rectangle {
                            radius: 4
                            color: enabled ? theme.sendButtonEnabled : theme.sendButtonDisabled
                        }
                        onClicked: {
                            loginBackend.sendWorldTextMessage(inputArea.text)
                            inputArea.text = ""
                        }
                    }
                }
            }
        }
    }

    // 未选中会话时的占位界面
    Rectangle {
        anchors.fill: parent
        visible: !root.hasSelection
        color: theme.chatAreaBackground

        Image {
            anchors.centerIn: parent
            source: ""   // 这里可以换成你的占位图，如微信 Logo
            width: 120
            height: 120
            opacity: 0.3
        }
    }

    ListModel {
        id: messageModel

        // 初始为空，由服务器推送的 MSG_PUSH 填充。
    }

    Connections {
        target: loginBackend

        function onMessageReceived(conversationId, senderId, content, serverTimeMs, seq) {
            const mine = senderId === loginBackend.userId
            messageModel.append({
                sender: mine ? "me" : "other",
                content: content
            })
            messageList.positionViewAtEnd()
        }
    }
}
