import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import WeChatClient as AppTheme

// 登录主界面：账号密码登录 + 注册弹窗
ApplicationWindow {
    id: window

    // 注册窗口实例（避免重复创建）。
    property var registerWindow: null
    // 主界面窗口实例（避免重复创建，并防止被 GC 回收）。
    property Window mainWindow: null
    width: 420
    height: 360
    visible: true

    // 全局主题单例
    readonly property var theme: AppTheme.Theme

    color: theme.windowBackground
    title: qsTr("WeChat 登录")

    // 无边框窗口，后续可按需加入自定义标题栏
    flags: Qt.FramelessWindowHint | Qt.Window

    // 顶部简易拖动区域 + 窗口控制按钮
    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 36
        color: theme.windowBackground

        DragHandler {
            target: null
            onActiveChanged: if (active) window.startSystemMove()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

                Label {
                    text: qsTr("微信登录")
                    color: theme.textPrimary
                font.pixelSize: 14
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
                onClicked: Qt.quit()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: topBar.height
        color: theme.windowBackground

        ColumnLayout {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 320
            spacing: 16

            Label {
                text: qsTr("微信帐号登录")
                color: theme.textPrimary
                font.pixelSize: 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }

            // 账号输入
            TextField {
                id: accountField
                Layout.fillWidth: true
                placeholderText: qsTr("邮箱 / 微信号 / 手机号")
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

            // 密码输入
            TextField {
                id: passwordField
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

            // 错误信息提示
            Label {
                Layout.fillWidth: true
                visible: loginBackend.errorMessage.length > 0
                text: loginBackend.errorMessage
                color: "#f24957"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    id: loginButton
                    text: qsTr("登录")
                    Layout.fillWidth: true
                    enabled: !loginBackend.busy
                             && accountField.text.length > 0
                             && passwordField.text.length > 0

                    background: Rectangle {
                        radius: 4
                        color: loginButton.enabled ? theme.sendButtonEnabled
                                                   : theme.sendButtonDisabled
                    }

                    contentItem: Label {
                        text: loginButton.text
                        color: "#ffffff"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: loginBackend.login(accountField.text, passwordField.text)
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: registerButton
                    text: qsTr("注册账号")
                    flat: true
                    background: null
                    contentItem: Label {
                        text: registerButton.text
                        color: theme.textSecondary
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        loginBackend.clearError()
                        if (!window.registerWindow || !window.registerWindow.visible) {
                            const component = Qt.createComponent("RegisterWindow.qml")
                            if (component.status === Component.Ready) {
                                window.registerWindow = component.createObject(window, {
                                                                            loginBackend: loginBackend
                                                                        })
                            }
                        } else {
                            window.registerWindow.raise()
                            window.registerWindow.requestActivate()
                        }
                    }
                }
            }
        }
    }

    // 登录成功后进入主界面
    Connections {
        target: loginBackend

        function onLoginSucceeded() {
            if (!window.mainWindow) {
                const component = Qt.createComponent("../Main/Main.qml")
                if (component.status === Component.Ready) {
                    // 将 Main 窗口的 QObject 父对象设置为登录窗口，
                    // 并保存在属性中，避免被 QML 垃圾回收提前销毁。
                    window.mainWindow = component.createObject(window)
                }
            }

            if (window.mainWindow) {
                window.mainWindow.show()
                window.mainWindow.raise()
                window.mainWindow.requestActivate()
            }

            // 隐藏登录窗口，而不是销毁根对象。
            window.visible = false
        }
    }
}
