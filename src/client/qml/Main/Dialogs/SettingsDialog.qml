import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

import WeChatClient as AppTheme

// 独立顶层窗口形式的“设置”界面，不阻塞主窗口。
Window {
    id: root

    visible: false
    width: 900
    height: 640
    minimumWidth: 720
    minimumHeight: 520

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    modality: Qt.NonModal

    readonly property var theme: AppTheme.Theme

    // 对外暴露的当前选中页（目前只有“账号”，预留扩展）
    property string currentSection: "account"

    // 对外暴露：当前昵称（初始可从 loginBackend.displayName 传入）
    property string displayName: ""
    // 对外暴露：当前头像占位（后续可接入真实头像路径）
    property string avatarText: "A"
    property string avatarPath: ""
    property url avatarUrl: ""

    // TODO: 将来可在此定义信号，例如名称/头像变更提交给后端

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: theme.windowBackground
        border.color: theme.cardBorder

        // 整体拖动
        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }

        // 主体区域：左侧选项栏 + 分割线 + 右侧设置内容
        RowLayout {
            anchors.fill: parent
            spacing: 0

            // 左侧选项栏：仅保留“账号”一项，背景使用会话列表色
            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: theme.chatListBackground

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    anchors.topMargin: 48
                    spacing: 8

                    Rectangle {
                        id: accountItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        radius: 6

                        property bool hovered: false
                        readonly property bool checked: root.currentSection === "account"

                        color: checked
                               ? (hovered ? theme.chatListItemSelectedHover
                                          : theme.chatListItemSelected)
                               : (hovered ? theme.chatListItemHover
                                          : theme.chatListBackground)

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12

                            Label {
                                text: "A"
                                color: theme.textPrimary
                                font.pixelSize: 16
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Label {
                                text: qsTr("账号")
                                color: theme.textPrimary
                                font.pixelSize: 14
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: accountItem.hovered = true
                            onExited: accountItem.hovered = false
                            onClicked: root.currentSection = "account"
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }

            // 中间分割线
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: theme.separatorVertical
            }

            // 右侧设置内容区域：使用消息区域背景色
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: theme.chatAreaBackground

                // 顶部右上角关闭按钮
                ToolButton {
                    id: closeButton
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 3
                        color: closeButton.hovered ? theme.dangerRed : "transparent"
                    }
                    contentItem: Label {
                        text: "×"
                        color: closeButton.hovered ? "#ffffff" : "#f5f5f5"
                        font.pixelSize: 24
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.visible = false
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    anchors.topMargin: closeButton.y + closeButton.implicitHeight + 16
                    spacing: 24

                    // 账号信息卡片：头像 + 昵称输入框 + 修改按钮
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: infoRow.implicitHeight + 40
                        radius: 10
                        color: theme.cardBackground
                        border.color: theme.cardBorder

                        RowLayout {
                            id: infoRow
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: 20
                            spacing: 24

                            // 左列：头像 + 修改头像按钮（固定宽度，垂直排布）
                            ColumnLayout {
                                Layout.preferredWidth: 120
                                Layout.alignment: Qt.AlignTop
                                spacing: 8

                                Rectangle {
                                    width: 72
                                    height: 72
                                    radius: 8
                                    color: "#4f90f2"

                                    clip: true

                                    // 文本头像（无图片时显示）
                                    Text {
                                        anchors.centerIn: parent
                                        text: root.avatarText
                                        color: "#ffffff"
                                        font.pixelSize: 30
                                        font.bold: true
                                        visible: avatarImg.status !== Image.Ready
                                    }

                                    // 图片头像
                                    Image {
                                        id: avatarImg
                                        anchors.fill: parent
                                        // Windows 下需补充 file:/// 前缀，以确保本地路径能被正确加载
                                        // 使用后端提供的绝对路径 URL
                                        source: root.avatarUrl
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                        asynchronous: true
                                        // 禁用缓存以确保更换头像后立即刷新
                                        cache: false
                                    }
                                }

                                Button {
                                    text: qsTr("修改头像")
                                    implicitWidth: 96
                                    implicitHeight: 30
                                    background: Rectangle {
                                        radius: 4
                                        color: theme.chatListItemHover
                                    }
                                    contentItem: Label {
                                        text: qsTr("修改头像")
                                        color: theme.textPrimary
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 13
                                    }
                                    // 预留：点击后弹出文件选择或头像编辑窗口
                                    onClicked: avatarPicker.open()
                                }
                            }

                            // 右列：昵称文本框 + 修改按钮，占据剩余空间
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 12

                                TextField {
                                    id: nameField
                                    Layout.fillWidth: true
                                    text: root.displayName
                                    placeholderText: qsTr("输入新的昵称…")
                                    color: theme.textPrimary
                                    placeholderTextColor: theme.textMuted
                                    font.pixelSize: 13
                                    background: Rectangle {
                                        radius: 4
                                        color: theme.chatListBackground
                                        border.color: theme.cardBorder
                                    }
                                }

                                Button {
                                    text: qsTr("修改名称")
                                    implicitWidth: 96
                                    implicitHeight: 30
                                    enabled: nameField.text.trim().length > 0
                                    background: Rectangle {
                                        radius: 4
                                        color: enabled ? theme.primaryButton
                                                       : theme.sendButtonDisabled
                                    }
                                    contentItem: Label {
                                        text: qsTr("修改名称")
                                        color: "#ffffff"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 13
                                        font.bold: true
                                    }
                                    // 预留：点击后同步 root.displayName，并通知后端
                                    onClicked: {
                                        const name = nameField.text.trim()
                                        if (name.length === 0)
                                            return
                                        loginBackend.updateDisplayName(name)
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }

    FileDialog {
        id: avatarPicker
        title: "选择头像"
        nameFilters: ["图片文件 (*.jpg *.png *.jpeg *.bmp)"]
        onAccepted: {
            // QtQuick.Dialogs 在 Qt6 中使用 selectedFile (或者 currentFile)
            var path = avatarPicker.selectedFile.toString()
            // 简单的 URL 转本地路径处理
            if (Qt.platform.os === "windows") {
                // file:///C:/... -> C:/...
                path = path.replace(/^(file:\/{3})/, "")
            } else {
                // file:///home/... -> /home/...
                path = path.replace(/^(file:\/\/)/, "")
            }
            // 解码 URL 编码字符（如中文文件名）
            path = decodeURIComponent(path)
            loginBackend.updateAvatar(path)
        }
    }

    // 同步后端昵称变化到对话框 UI。
    Connections {
        target: loginBackend

        function onDisplayNameChanged() {
            root.displayName = loginBackend.displayName
            nameField.text = loginBackend.displayName
            if (loginBackend.displayName.length > 0) {
                root.avatarText = loginBackend.displayName[0]
            }
        }

        function onAvatarUrlChanged() {
            root.avatarUrl = loginBackend.avatarUrl
        }
    }
}
