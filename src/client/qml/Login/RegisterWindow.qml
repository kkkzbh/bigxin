import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import WeChatClient as AppTheme

// 注册窗口：独立大窗，应用级模态，注册成功后弹出提示并自动关闭。
ApplicationWindow {
    id: registerWindow

    property var loginBackend

    width: 640
    height: 480
    visible: true

    // 全局主题单例
    readonly property var theme: AppTheme.Theme

    color: theme.windowBackground
    title: qsTr("注册微信账号")

    // 应用级模态，冻结登录窗口，但保持其可见。
    modality: Qt.ApplicationModal
    flags: Qt.FramelessWindowHint | Qt.Window

    // 顶部标题栏，可拖动。
    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        color: theme.windowBackground

        DragHandler {
            target: null
            onActiveChanged: if (active) registerWindow.startSystemMove()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Label {
                text: qsTr("注册微信账号")
                color: theme.textPrimary
                font.pixelSize: 15
                Layout.alignment: Qt.AlignVCenter
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                id: closeButton
                hoverEnabled: true
                background: Rectangle {
                    radius: 3
                    color: closeButton.hovered ? theme.dangerRed : "transparent"
                    border.color: "transparent"
                }
                contentItem: Label {
                    text: "×"
                    color: closeButton.hovered ? "#ffffff" : theme.textPrimary
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: registerWindow.close()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        color: theme.windowBackground

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 16

            Label {
                text: qsTr("创建你的微信账号")
                color: theme.textPrimary
                font.pixelSize: 18
                font.bold: true
            }

            Label {
                text: qsTr("请填写账号和密码，昵称将自动生成。")
                color: theme.textSecondary
                font.pixelSize: 12
            }

            // 账号
            TextField {
                id: regAccountField
                Layout.fillWidth: true
                placeholderText: qsTr("账号（微信号 / 邮箱 / 手机号）")
                color: theme.textPrimary
                placeholderTextColor: theme.textSecondary
                font.pixelSize: 14
                background: Rectangle {
                    radius: 4
                    color: theme.chatAreaBackground
                    border.color: theme.separatorHorizontal
                    border.width: 1
                }
            }

            // 密码
            TextField {
                id: regPasswordField
                Layout.fillWidth: true
                placeholderText: qsTr("密码")
                echoMode: TextInput.Password
                color: theme.textPrimary
                placeholderTextColor: theme.textSecondary
                font.pixelSize: 14
                background: Rectangle {
                    radius: 4
                    color: theme.chatAreaBackground
                    border.color: theme.separatorHorizontal
                    border.width: 1
                }
            }

            // 确认密码
            TextField {
                id: regConfirmField
                Layout.fillWidth: true
                placeholderText: qsTr("确认密码")
                echoMode: TextInput.Password
                color: theme.textPrimary
                placeholderTextColor: theme.textSecondary
                font.pixelSize: 14
                background: Rectangle {
                    radius: 4
                    color: theme.chatAreaBackground
                    border.color: theme.separatorHorizontal
                    border.width: 1
                }
            }

            // 错误信息（注册失败）
            Label {
                Layout.fillWidth: true
                visible: loginBackend && loginBackend.errorMessage.length > 0
                text: loginBackend ? loginBackend.errorMessage : ""
                color: "#f24957"
                font.pixelSize: 12
            }

            Item {
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: qsTr("取消")
                    background: Rectangle {
                        radius: 4
                        color: theme.sendButtonDisabled
                    }
                    contentItem: Label {
                        text: qsTr("取消")
                        color: "#ffffff"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: registerWindow.close()
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: registerButton
                    text: qsTr("注册")
                    enabled: loginBackend
                             && !loginBackend.busy
                             && regAccountField.text.length > 0
                             && regPasswordField.text.length > 0
                             && regConfirmField.text.length > 0

                    background: Rectangle {
                        radius: 4
                        color: enabled ? theme.sendButtonEnabled : theme.sendButtonDisabled
                    }

                    contentItem: Label {
                        text: registerButton.text
                        color: "#ffffff"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (!loginBackend) {
                            return
                        }
                        loginBackend.clearError()
                        loginBackend.registerAccount(
                                    regAccountField.text,
                                    regPasswordField.text,
                                    regConfirmField.text)
                    }
                }
            }
        }
    }

    // 注册成功提示框：点击确定后关闭注册窗口，回到登录。
    Dialog {
        id: successDialog
        modal: true
        width: 320
        height: 140
        x: (registerWindow.width - width) / 2
        y: (registerWindow.height - height) / 2
        standardButtons: Dialog.Ok
        title: qsTr("注册成功")

        background: Rectangle {
            radius: 8
            color: theme.chatAreaBackground
            border.color: theme.separatorHorizontal
            border.width: 1
        }

        contentItem: Rectangle {
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                Label {
                    id: successText
                    Layout.fillWidth: true
                    text: qsTr("注册成功，请使用该账号登录。")
                    color: theme.textPrimary
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }
            }
        }

        onAccepted: {
            registerWindow.close()
        }
        onRejected: {
            registerWindow.close()
        }
    }

    // 监听注册成功信号，弹出成功提示框。
    Connections {
        target: loginBackend

        function onRegistrationSucceeded(account) {
            successDialog.open()
        }
    }
}
