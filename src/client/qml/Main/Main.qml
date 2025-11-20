import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import WeChatClient as AppTheme
import "Sidebar"
import "ChatList"
import "ChatArea"
import "Dialogs"

ApplicationWindow {
    id: window
    width: 1400
    height: 900
    visible: true

    // 全局主题单例
    readonly property var theme: AppTheme.Theme

    color: theme.windowBackground
    title: qsTr("WeChat")

    // 无边框窗口，使用自绘标题栏
    flags: Qt.FramelessWindowHint | Qt.Window

    // 是否有选中的会话：用于控制标题栏右侧和对话区显示
    property bool hasSelection: {
        if (currentTab === 0) return chatList.currentIndex >= 0
        if (currentTab === 1) return chatList.hasContactSelection
        return false
    }

    // 当前选中的功能标签页：0=聊天, 1=通讯录
    property int currentTab: 0

    Component.onCompleted: {
        loginBackend.requestConversationList()
        loginBackend.requestFriendList()
        loginBackend.requestFriendRequestList()
    }

    Connections {
        target: loginBackend
        function onSingleConversationReady(conversationId, conversationType) {
            // 用户主动打开单聊时切回聊天页签，等待会话列表刷新后高亮。
            window.currentTab = 0
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部统一标题栏：
        //  - 左：侧边栏头像区域（单行）
        //  - 中：会话栏搜索区域（单行）
        //  - 右：对话框标题区域（内部上下两行：上=窗口按钮，下=会话名字/搜索信息）
        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: 86
            color: theme.topBarBackground

            // 使用 DragHandler + startSystemMove 实现拖动（Qt 6 推荐）
            DragHandler {
                target: null
                onActiveChanged: if (active) window.startSystemMove()
            }

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // 左侧：侧边栏头像区域（单行）
                Rectangle {
                    width: 72
                    Layout.fillHeight: true
                    color: theme.sidebarBackground

                    Rectangle {
                        id: avatar
                        width: 40
                        height: 40
                        radius: 8
                        color: "#ffffff"
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "A"
                            color: "#222222"
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }
                }

                // 中间：会话搜索栏（单行，宽度与会话栏保持一致，避免被右侧标题侵占）
                Rectangle {
                    Layout.preferredWidth: 300   // 回退到会话栏原始宽度
                    Layout.fillHeight: true
                    color: theme.chatListBackground

                    Rectangle {
                        id: searchBox
                        // 高度约占顶栏的 70%
                        width: parent.width * 0.9
                        height: parent.height * 0.7
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 6
                        color: theme.searchBoxBackground

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Label {
                                text: "\u2315"
                                color: theme.searchIcon
                                font.pixelSize: 14
                            }

                            TextField {
                                id: searchField
                                Layout.fillWidth: true
                                background: null
                                placeholderText: qsTr("搜索")
                                color: theme.searchText
                                placeholderTextColor: theme.searchPlaceholder
                                font.pixelSize: 13
                            }

                            ToolButton {
                                id: addButton
                                text: "+"
                                font.pixelSize: 18
                                hoverEnabled: true
                                background: Rectangle {
                                    radius: 4
                                    color: addButton.hovered ? "#2f3035" : "transparent"
                                }
                                onClicked: {
                                    const point = addButton.mapToItem(window.contentItem,
                                                                     addButton.width / 2,
                                                                     addButton.height)
                                    addMenu.x = point.x - addMenu.implicitWidth / 2
                                    addMenu.y = topBar.height - 6
                                    addMenu.open()
                                }
                            }
                        }
                    }
                }

                // 中间竖线：会话栏 / 对话框 分割线（顶部部分）
                Rectangle {
                    id: topSeparator
                    width: 1
                    Layout.fillHeight: true
                    color: theme.separatorVertical
                }

                // 右侧：对话框顶部区域（内部两行：上=系统按钮，下=名字/搜索信息）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: theme.chatAreaBackground

                    // 底部分割线仅在对话框区域内
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: theme.separatorHorizontal
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // 上行：仅 [- □ ×]，固定在右上角
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32

                            Item { Layout.fillWidth: true }

                            RowLayout {
                                spacing: 6

                                ToolButton {
                                    id: minimizeButton
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: 3
                                        color: minimizeButton.hovered ? "#2a2c30" : "transparent"
                                        border.color: "transparent"
                                    }
                                    contentItem: Label {
                                        text: "−"
                                        color: "#f5f5f5"
                                        font.pixelSize: 24
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: window.showMinimized()
                                }

                                ToolButton {
                                    id: maxButton
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: 3
                                        color: maxButton.hovered ? "#2a2c30" : "transparent"
                                        border.color: "transparent"
                                    }
                                    contentItem: Label {
                                        text: window.visibility === Window.Maximized ? "▢" : "□"
                                        color: "#f5f5f5"
                                        font.pixelSize: 23
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        if(window.visibility === Window.Maximized) {
                                            window.showNormal()
                                        } else {
                                            window.showMaximized();
                                        }
                                    }
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
                                        color: closeButton.hovered ? "#ffffff" : "#f5f5f5"
                                        font.pixelSize: 24
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: Qt.quit()
                                }
                            }
                        }

                        // 下行：会话名称 + 顶部工具（仅对话框区域内部）
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 20
                                anchors.rightMargin: 20

                                Label {
                                    text: window.hasSelection ? chatArea.conversationTitle : ""
                                    color: "#f5f5f5"
                                    font.pixelSize: 16
                                    font.bold: true
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Item { Layout.fillWidth: true }

                                RowLayout {
                                    visible: window.hasSelection
                                    spacing: 12

                                    ToolButton {
                                        text: "🔍"
                                        background: null
                                        font.pixelSize: 16
                                    }

                                    ToolButton {
                                        text: "☰"
                                        background: null
                                        font.pixelSize: 16
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 顶部 + 号下拉菜单
        AppTheme.PlusMenu {
            id: addMenu
            parent: window.contentItem
        }

        // 添加好友独立窗口（非模态）
        AppTheme.AddFriendDialog {
            id: addFriendDialog
        }

        // 设置窗口（非模态）
        SettingsDialog {
            id: settingsDialog
        }

        // 主体区域：侧边栏 / 会话列表 / 分隔线 / 对话区域
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Sidebar {
                Layout.preferredWidth: 72
                Layout.fillHeight: true
                currentIndex: window.currentTab
                onTabClicked: (index, key) => {
                    window.currentTab = index
                }
            }

            ChatList {
                id: chatList
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                currentTab: window.currentTab
            }

            // 会话列表与对话区域之间的分隔线（主体区域）
            Rectangle {
                width: 1
                color: theme.separatorVertical
                Layout.fillHeight: true
            }

            ChatArea {
                id: chatArea
                currentTab: window.currentTab
                hasSelection: window.hasSelection
                
                // Chat Tab Data
                conversationId: chatList.currentConversationId
                conversationType: chatList.currentConversationType
                conversationTitle: chatList.currentConversationTitle
                
                // Contact Tab Data
                contactName: chatList.currentContactName
                contactWeChatId: chatList.currentContactWeChatId
                contactSignature: chatList.currentContactSignature
                requestStatus: chatList.currentRequestStatus
                contactUserId: chatList.currentContactUserId
                contactRequestId: chatList.currentRequestId

                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }
}
