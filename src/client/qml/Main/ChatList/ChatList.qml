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
    
    // 使用内部属性避免绑定循环，当currentIndex变化时更新
    property string currentConversationId: ""
    property string currentConversationType: ""
    property string currentConversationTitle: ""
    property string currentConversationAvatarPath: ""
    
    // 监听 currentIndex 变化，更新当前会话信息
    onCurrentIndexChanged: {
        if (currentIndex < 0 || currentIndex >= chatModel.count) {
            currentConversationId = ""
            currentConversationType = ""
            currentConversationTitle = ""
            currentConversationAvatarPath = ""
        } else {
            const item = chatModel.get(currentIndex)
            currentConversationId = String(item.conversationId || "")
            currentConversationType = item.conversationType
            currentConversationTitle = item.title
            currentConversationAvatarPath = item.avatarPath || ""
        }
    }

    property int currentTab: 0
    property bool hasContactSelection: contactList.currentIndex >= 0
    
    // 导出数据模型供搜索功能使用
    property alias chatModel: chatModel
    
    property string currentContactName: contactList.currentContactName
    property string currentContactWeChatId: contactList.currentContactWeChatId
    property string currentContactSignature: contactList.currentContactSignature
    property string currentRequestStatus: contactList.currentRequestStatus
    property string currentContactUserId: contactList.currentContactUserId
    property string currentRequestId: contactList.currentRequestId
    property string currentRequestType: contactList.currentRequestType
    property string currentGroupName: contactList.currentGroupName
    property string currentGroupId: contactList.currentGroupId
    
    // 导出通讯录模型供搜索功能使用
    property alias contactsModel: contactList.contactsModel
    property alias groupsModel: contactList.groupsModel

    // 用户主动打开单聊后等待选中的会话 ID（仅主动操作使用,不影响被动消息）。
    property string pendingSelectConversationId: ""

    // 尝试立即选中指定会话，如果找不到则设置 pending 等待刷新后选中
    function selectConversation(conversationId) {
        var targetId = String(conversationId || "")
        // 先尝试在当前列表中查找（统一字符串对比，避免数字/字符串类型不一致导致无法选中）
        for (var j = 0; j < chatModel.count; ++j) {
            var item = chatModel.get(j)
            if (String(item.conversationId || "") === targetId) {
                listView.currentIndex = j
                pendingSelectConversationId = ""
                return
            }
        }
        // 找不到，设置 pending 等待列表刷新
        pendingSelectConversationId = targetId
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
            // 保存当前选中的会话ID，以便刷新后恢复
            var previousConversationId = String(root.currentConversationId || "")
            
            chatModel.clear()
            for (var i = 0; i < conversations.length; ++i) {
                var item = conversations[i]
                // 兜底去掉标题首尾空白，避免通讯录数据带空格。
                item.title = (item.title || "").trim()
                chatModel.append(item)
            }
            
            // 优先级1: 主动打开会话（pendingSelectConversationId）
            if (pendingSelectConversationId && pendingSelectConversationId !== "") {
                var targetIndex = -1
                for (var j = 0; j < chatModel.count; ++j) {
                    if (String(chatModel.get(j).conversationId || "") === String(pendingSelectConversationId)) {
                        targetIndex = j
                        break
                    }
                }
                if (targetIndex >= 0) {
                    listView.currentIndex = targetIndex
                }
                pendingSelectConversationId = ""
            } 
            // 优先级2: 恢复之前选中的会话
            else if (previousConversationId && previousConversationId !== "") {
                var restoreIndex = -1
                for (var k = 0; k < chatModel.count; ++k) {
                    if (String(chatModel.get(k).conversationId || "") === String(previousConversationId)) {
                        restoreIndex = k
                        break
                    }
                }
                if (restoreIndex >= 0) {
                    listView.currentIndex = restoreIndex
                    // 同时更新标题和头像（可能已改名或换头像）
                    root.currentConversationTitle = chatModel.get(restoreIndex).title
                    root.currentConversationAvatarPath = chatModel.get(restoreIndex).avatarPath || ""
                } else if (chatModel.count > 0) {
                    // 之前的会话已不存在，选中第一个
                    listView.currentIndex = 0
                }
            }
            // 优先级3: 首次加载，选中第一个
            else if (chatModel.count > 0 && listView.currentIndex === -1) {
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
