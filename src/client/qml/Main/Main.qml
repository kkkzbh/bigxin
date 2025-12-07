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
        loginBackend.requestGroupJoinRequestList()
    }

    Connections {
        target: loginBackend
        function onSingleConversationReady(conversationId, conversationType) {
            // 用户主动打开单聊时切回聊天页签，立即尝试选中会话。
            window.currentTab = 0
            chatList.selectConversation(conversationId)
        }
        function onConversationOpened(conversationId) {
            // 用户从通讯录点击群聊的"发消息"时，切换到聊天页签并立即选中会话。
            window.currentTab = 0
            chatList.selectConversation(conversationId)
        }
        function onGroupCreated(conversationId, title) {
            window.currentTab = 0
            chatList.selectConversation(conversationId)
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
            // 增加 margin 避免与边缘缩放冲突
            Item {
                anchors.fill: parent
                anchors.margins: 10
                DragHandler {
                    target: null
                    onActiveChanged: if (active) window.startSystemMove()
                }
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
                        radius: 4
                        color: "#4f90f2"
                        clip: true
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: loginBackend.displayName.length > 0 ? loginBackend.displayName[0].toUpperCase() : "A"
                            color: "white"
                            font.pixelSize: 18
                            font.bold: true
                            visible: avatarImg.status !== Image.Ready
                        }

                        Image {
                            id: avatarImg
                            anchors.fill: parent
                            source: loginBackend.avatarUrl
                            fillMode: Image.PreserveAspectCrop
                            visible: status === Image.Ready
                            asynchronous: true
                            cache: false
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof settingsDialog !== "undefined" && settingsDialog) {
                                    settingsDialog.displayName = loginBackend.displayName
                                    settingsDialog.avatarUrl = loginBackend.avatarUrl
                                    settingsDialog.avatarPath = loginBackend.avatarPath
                                    settingsDialog.avatarText = loginBackend.displayName.length > 0
                                                               ? loginBackend.displayName[0]
                                                               : "A"
                                    settingsDialog.visible = true
                                    settingsDialog.requestActivate()
                                }
                            }
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
                                
                                onTextChanged: {
                                    searchResultPanel.searchText = text
                                    var hasText = text.trim().length > 0
                                    if (hasText) {
                                        // 延迟到下一帧再打开，确保搜索结果和布局已完成，避免首次打开高度未定导致点击命中异常
                                        Qt.callLater(function() {
                                            if (searchField.text.trim().length > 0) {
                                                searchResultPanel.open()
                                            }
                                        })
                                    } else {
                                        searchResultPanel.close()
                                    }
                                }
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
                                    addMenu.popup(addButton, addButton.width / 2 - addMenu.implicitWidth / 2, addButton.height - 6)
                                }
                            }
                        }
                    }
                    
                    // 搜索结果面板
                    SearchResultPanel {
                        id: searchResultPanel
                        parent: window.contentItem

                        chatModel: chatList.chatModel
                        contactsModel: chatList.contactsModel
                        groupsModel: chatList.groupsModel
                        
                        // 提供给 SearchResultPanel 的位置刷新函数
                        function updatePosition() {
                            if (searchBox && window.contentItem) {
                                var pos = searchBox.mapToItem(window.contentItem, 0, 0)
                                searchResultPanel.x = pos.x
                                searchResultPanel.y = pos.y + searchBox.height + 2
                                searchResultPanel.width = searchBox.width
                            }
                        }
                        
                        onConversationClicked: function(conversationId) {
                            window.currentTab = 0
                            chatList.selectConversation(conversationId)
                            searchField.text = ""
                        }
                        
                        onFriendClicked: function(friendUserId) {
                            window.currentTab = 0
                            loginBackend.openSingleConversation(friendUserId)
                            searchField.text = ""
                        }
                        
                        onGroupClicked: function(conversationId) {
                            window.currentTab = 0
                            chatList.selectConversation(conversationId)
                            searchField.text = ""
                        }
                        
                        onAddFriendClicked: function(query) {
                            addFriendDialog.query = query
                            addFriendDialog.visible = true
                            addFriendDialog.raise()
                            addFriendDialog.requestActivate()
                            searchField.text = ""
                        }
                        
                        onAddGroupClicked: function(query) {
                            joinGroupDialog.query = query
                            joinGroupDialog.visible = true
                            joinGroupDialog.raise()
                            joinGroupDialog.requestActivate()
                            searchField.text = ""
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
                                        id: detailButton
                                        background: null
                                        icon.source: "qrc:/qt/qml/WeChatClient/client/qml/resource/ChatArea/list-unordered.svg"
                                        icon.width: 20
                                        icon.height: 20
                                        display: AbstractButton.IconOnly
                                        onClicked: {
                                            chatArea.toggleDetailPanel()
                                        }
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
            startGroupDialog: startGroupDialog
            joinGroupDialog: joinGroupDialog
        }

        // 添加好友独立窗口（非模态）
        AppTheme.AddFriendDialog {
            id: addFriendDialog
        }

        // 添加群聊独立窗口（非模态）
        AppTheme.JoinGroupDialog {
            id: joinGroupDialog
        }

        // 发起群聊弹窗（模态）
        AppTheme.StartGroupDialog {
            id: startGroupDialog
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
                conversationAvatarPath: chatList.currentConversationAvatarPath
                
                // Contact Tab Data
                contactName: chatList.currentContactName
                contactWeChatId: chatList.currentContactWeChatId
                contactSignature: chatList.currentContactSignature
                requestStatus: chatList.currentRequestStatus
                contactUserId: chatList.currentContactUserId
                contactRequestId: chatList.currentRequestId
                contactRequestType: chatList.currentRequestType
                contactGroupName: chatList.currentGroupName
                contactGroupId: chatList.currentGroupId

                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    // 窗口边缘缩放处理 (Qt 6.9 推荐方式: startSystemResize)
    // 注意：startSystemResize 需要传递 Qt.Edge 标志位，而不是 Qt.Corner
    Item {
        id: resizeFrame
        anchors.fill: parent
        z: 9999 // 确保在最上层

        // 边缘判定宽度
        readonly property int m: 6

        // 四边
        MouseArea {
            width: resizeFrame.m; height: parent.height
            anchors.left: parent.left
            cursorShape: Qt.SizeHorCursor
            onPressed: window.startSystemResize(Qt.LeftEdge)
        }
        MouseArea {
            width: resizeFrame.m; height: parent.height
            anchors.right: parent.right
            cursorShape: Qt.SizeHorCursor
            onPressed: window.startSystemResize(Qt.RightEdge)
        }
        MouseArea {
            height: resizeFrame.m; width: parent.width
            anchors.top: parent.top
            cursorShape: Qt.SizeVerCursor
            onPressed: window.startSystemResize(Qt.TopEdge)
        }
        MouseArea {
            height: resizeFrame.m; width: parent.width
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeVerCursor
            onPressed: window.startSystemResize(Qt.BottomEdge)
        }

        // 四角 (组合 Edge 标志位)
        MouseArea {
            width: resizeFrame.m * 2; height: resizeFrame.m * 2
            anchors.left: parent.left; anchors.top: parent.top
            cursorShape: Qt.SizeFDiagCursor
            onPressed: window.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
        }
        MouseArea {
            width: resizeFrame.m * 2; height: resizeFrame.m * 2
            anchors.right: parent.right; anchors.top: parent.top
            cursorShape: Qt.SizeBDiagCursor
            onPressed: window.startSystemResize(Qt.TopEdge | Qt.RightEdge)
        }
        MouseArea {
            width: resizeFrame.m * 2; height: resizeFrame.m * 2
            anchors.left: parent.left; anchors.bottom: parent.bottom
            cursorShape: Qt.SizeBDiagCursor
            onPressed: window.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }
        MouseArea {
            width: resizeFrame.m * 2; height: resizeFrame.m * 2
            anchors.right: parent.right; anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor
            onPressed: window.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }
}
