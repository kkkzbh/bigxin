import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import WeChatClient as AppTheme

Rectangle {
    id: root
    // 恢复原先的会话栏背景色，偏深灰

    // 全局主题单例
    readonly property var theme: AppTheme.Theme

    color: theme.chatListBackground

    // 导出当前选中下标，便于标题栏 / 对话区判断是否有选中会话
    property alias currentIndex: listView.currentIndex
    // 当前选中会话的 ID（索引非法时返回空字符串，避免访问越界）。
    property string currentConversationId: {
        if (currentIndex < 0 || currentIndex >= chatModel.count)
            return ""
        return chatModel.get(currentIndex).conversationId
    }
    // 当前选中会话的类型（索引非法时返回空字符串，避免访问越界）。
    property string currentConversationType: {
        if (currentIndex < 0 || currentIndex >= chatModel.count)
            return ""
        return chatModel.get(currentIndex).conversationType
    }
    // 当前选中会话的标题 / 昵称（用于右侧 ChatArea 标题栏）。
    property string currentConversationTitle: {
        if (currentIndex < 0 || currentIndex >= chatModel.count)
            return ""
        return chatModel.get(currentIndex).title
    }

    property int currentTab: 0
    property bool hasContactSelection: contactList.currentIndex >= 0
    property string currentContactName: contactList.currentContactName
    property string currentContactWeChatId: contactList.currentContactWeChatId
    property string currentContactSignature: contactList.currentContactSignature
    property string currentRequestStatus: contactList.currentRequestStatus
    property string currentContactUserId: contactList.currentContactUserId
    property string currentRequestId: contactList.currentRequestId
    property string currentRequestType: contactList.currentRequestType
    property string currentGroupName: contactList.currentGroupName
    property string currentGroupId: contactList.currentGroupId

    // 用户主动打开单聊后等待选中的会话 ID（仅主动操作使用，不影响被动消息）。
    property string pendingSelectConversationId: ""

    // 尝试立即选中指定会话，如果找不到则设置 pending 等待刷新后选中
    function selectConversation(conversationId) {
        // 先尝试在当前列表中查找
        for (var j = 0; j < chatModel.count; ++j) {
            if (chatModel.get(j).conversationId === conversationId) {
                listView.currentIndex = j
                pendingSelectConversationId = ""
                return
            }
        }
        // 找不到，设置 pending 等待列表刷新
        pendingSelectConversationId = conversationId
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentTab

            // 会话列表
            ListView {
                id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: chatModel
            // 初始不选中任何会话
            currentIndex: -1

            delegate: Rectangle {
                id: delegateRoot
                width: ListView.view.width
                height: 68

                // hover 状态用于“焦点”高亮
                property bool hovered: false

                // 普通：#18191c
                // hover：#222326
                // 选中：#202124
                // 选中 + hover：略亮一点 #2a2c30
                color: ListView.isCurrentItem
                       ? (hovered ? theme.chatListItemSelectedHover
                                  : theme.chatListItemSelected)
                       : (hovered ? theme.chatListItemHover
                                  : theme.chatListItemNormal)

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: delegateRoot.hovered = true
                    onExited: delegateRoot.hovered = false
                    onClicked: listView.currentIndex = index
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        id: avatar
                        width: 42
                        height: 42
                        radius: 6
                        color: avatarColor
                        clip: true

                        Text {
                            anchors.centerIn: parent
                            text: initials
                            color: "#ffffff"
                            font.pixelSize: 18
                            font.bold: true
                            visible: avatarImg.status !== Image.Ready
                        }

                        Image {
                            id: avatarImg
                            anchors.fill: parent
                            // chatModel 里我们存了 avatarPath
                            // 有时候 model 字段如果是 undefined 可能会报错，所以 || ""
                            source: loginBackend.resolveAvatarUrl(model.avatarPath || "")
                            fillMode: Image.PreserveAspectCrop
                            visible: status === Image.Ready
                            asynchronous: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: title
                                color: theme.textPrimary
                                font.pixelSize: 14
                                font.bold: unreadCount > 0
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Label {
                                text: time
                                color: theme.textMuted
                                font.pixelSize: 11
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: preview
                                color: theme.textSecondary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                visible: unreadCount > 0
                                width: 18
                                height: 18
                                radius: 9
                                color: "#f24957"

                                Text {
                                    anchors.centerIn: parent
                                    text: unreadCount > 9 ? "9+" : unreadCount
                                    color: "#ffffff"
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }
            }
            }

            ContactListView {
                id: contactList
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    ListModel {
        id: chatModel
    }

    // 监听后端会话列表变更，重建模型。
    Connections {
        target: loginBackend

        function onConversationsReset(conversations) {
            chatModel.clear()
            for (var i = 0; i < conversations.length; ++i) {
                var item = conversations[i]
                // 兜底去掉标题首尾空白，避免通讯录数据带空格。
                item.title = (item.title || "").trim()
                chatModel.append(item)
            }
            // 主动打开会话：尝试选中 pending 会话。
            if (pendingSelectConversationId && pendingSelectConversationId !== "") {
                var targetIndex = -1
                for (var j = 0; j < chatModel.count; ++j) {
                    if (chatModel.get(j).conversationId === pendingSelectConversationId) {
                        targetIndex = j
                        break
                    }
                }
                if (targetIndex >= 0) {
                    listView.currentIndex = targetIndex
                }
                pendingSelectConversationId = ""
            } else if (chatModel.count > 0 && listView.currentIndex === -1) {
                listView.currentIndex = 0
            }
        }
    }

    // 监听单聊打开完成信号：仅源于用户主动点击"发消息"。
    Connections {
        target: loginBackend

        function onSingleConversationReady(conversationId, conversationType) {
            // Main.qml 已处理选中逻辑，这里不需要再设置
        }
    }
}
