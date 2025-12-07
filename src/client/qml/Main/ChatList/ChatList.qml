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
    property int currentLastReadSeq: 0
    property int currentUnreadCount: 0
    
    // 监听 currentIndex 变化，更新当前会话信息
    onCurrentIndexChanged: {
        if (currentIndex < 0 || currentIndex >= chatModel.count) {
            currentConversationId = ""
            currentConversationType = ""
            currentConversationTitle = ""
            currentConversationAvatarPath = ""
            currentLastReadSeq = 0
            currentUnreadCount = 0
        } else {
            const item = chatModel.get(currentIndex)
            currentConversationId = String(item.conversationId || "")
            currentConversationType = item.conversationType
            currentConversationTitle = item.title
            currentConversationAvatarPath = item.avatarPath || ""
            currentLastReadSeq = item.lastReadSeq || 0
            currentUnreadCount = item.unreadCount || 0
        }
    }

    property int currentTab: 0
    property bool hasContactSelection: contactList.currentIndex >= 0
    
    // 导出数据模型供搜索功能使用
    property alias chatModel: chatModel
    
    // 辅助函数：格式化消息时间
    function formatMessageTime(timestampMs) {
        if (timestampMs === 0) return ""
        
        var now = new Date()
        var msgTime = new Date(timestampMs)
        var diff = Math.floor((now - msgTime) / 1000) // 秒数差
        
        // 1分钟内：刚刚
        if (diff < 60) return "刚刚"
        
        // 1小时内：X分钟前
        if (diff < 3600) return Math.floor(diff / 60) + "分钟前"
        
        // 今天：HH:MM
        if (now.getFullYear() === msgTime.getFullYear() &&
            now.getMonth() === msgTime.getMonth() &&
            now.getDate() === msgTime.getDate()) {
            var hours = msgTime.getHours().toString().padStart(2, '0')
            var minutes = msgTime.getMinutes().toString().padStart(2, '0')
            return hours + ":" + minutes
        }
        
        // 昨天：昨天
        var yesterday = new Date(now)
        yesterday.setDate(yesterday.getDate() - 1)
        if (yesterday.getFullYear() === msgTime.getFullYear() &&
            yesterday.getMonth() === msgTime.getMonth() &&
            yesterday.getDate() === msgTime.getDate()) {
            return "昨天"
        }
        
        // 本周内：星期X
        if (diff < 7 * 24 * 3600) {
            var weekdays = ["星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"]
            return weekdays[msgTime.getDay()]
        }
        
        // 本年内：MM/DD
        if (now.getFullYear() === msgTime.getFullYear()) {
            var month = (msgTime.getMonth() + 1).toString().padStart(2, '0')
            var day = msgTime.getDate().toString().padStart(2, '0')
            return month + "/" + day
        }
        
        // 其他：YYYY/MM/DD
        var year = msgTime.getFullYear()
        var month2 = (msgTime.getMonth() + 1).toString().padStart(2, '0')
        var day2 = msgTime.getDate().toString().padStart(2, '0')
        return year + "/" + month2 + "/" + day2
    }
    
    // 辅助函数：生成消息预览文本
    function generateMessagePreview(msgType, content, senderName, conversationType, senderId) {
        var preview = ""
        
        // 群聊时显示发送者名字（不包括自己）
        if (conversationType === "GROUP" && senderId !== loginBackend.userId) {
            preview = senderName + ": "
        }
        
        // 根据消息类型生成预览
        if (msgType === "TEXT") {
            // 文本消息，截取前30个字符
            if (content.length > 30) {
                preview += content.substring(0, 30) + "..."
            } else {
                preview += content
            }
        } else if (msgType === "IMAGE") {
            preview += "[图片]"
        } else if (msgType === "FILE") {
            preview += "[文件]"
        } else if (msgType === "AUDIO") {
            preview += "[语音]"
        } else if (msgType === "VIDEO") {
            preview += "[视频]"
        } else {
            preview += "[消息]"
        }
        
        return preview
    }
    
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
                    onClicked: {
                        listView.currentIndex = index
                        // 标记该会话为已读
                        if (model.conversationId && model.lastSeq > 0) {
                            loginBackend.markConversationAsRead(String(model.conversationId), model.lastSeq)
                        }
                    }
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
                                width: unreadCount > 99 ? 24 : (unreadCount > 9 ? 22 : 18)
                                height: 18
                                radius: 9
                                color: "#f24957"

                                Text {
                                    anchors.centerIn: parent
                                    text: unreadCount > 99 ? "99+" : unreadCount
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

        function onConversationUnreadCleared(conversationId) {
            // 本地更新会话列表中的未读数为0
            for (var i = 0; i < chatModel.count; ++i) {
                var item = chatModel.get(i)
                if (String(item.conversationId || "") === String(conversationId)) {
                    chatModel.setProperty(i, "unreadCount", 0)
                    // 同时更新当前选中会话的未读数属性
                    if (i === listView.currentIndex) {
                        root.currentUnreadCount = 0
                    }
                    break
                }
            }
        }
        
        // 监听消息接收，智能更新单个会话项而非刷新整个列表
        function onMessageReceived(conversationId, senderId, senderDisplayName, content, msgType, serverTimeMs, seq) {
            var targetIndex = -1
            var currentItem = null
            
            // 查找目标会话
            for (var i = 0; i < chatModel.count; ++i) {
                var item = chatModel.get(i)
                if (String(item.conversationId || "") === String(conversationId)) {
                    targetIndex = i
                    currentItem = item
                    break
                }
            }
            
            if (targetIndex === -1) {
                return  // 找不到会话，可能是新会话，等待列表刷新
            }
            
            // 获取当前已知的最后一条消息的 seq
            var lastKnownSeq = currentItem.lastSeq || 0
            
            // 判断是否是新消息（seq 大于已知的最大 seq）
            var isNewMessage = seq > lastKnownSeq
            
            // 生成预览文本和格式化时间
            var preview = root.generateMessagePreview(msgType, content, senderDisplayName, currentItem.conversationType, senderId)
            var formattedTime = root.formatMessageTime(serverTimeMs)
            
            // 更新预览消息、时间和 lastSeq
            chatModel.setProperty(targetIndex, "preview", preview)
            chatModel.setProperty(targetIndex, "time", formattedTime)
            if (isNewMessage) {
                chatModel.setProperty(targetIndex, "lastSeq", seq)
            }
            
            // 如果是别人发送的消息且不是当前选中的会话，增加未读数
            if (senderId !== loginBackend.userId && String(root.currentConversationId) !== String(conversationId)) {
                var currentUnread = currentItem.unreadCount || 0
                chatModel.setProperty(targetIndex, "unreadCount", currentUnread + 1)
            }
            
            // 只有真正的新消息才将会话移到列表顶部
            if (isNewMessage && targetIndex !== 0) {
                var oldSelectedIndex = listView.currentIndex
                var oldSelectedId = ""
                
                // 保存当前选中的会话ID
                if (oldSelectedIndex >= 0 && oldSelectedIndex < chatModel.count) {
                    oldSelectedId = String(chatModel.get(oldSelectedIndex).conversationId || "")
                }
                
                // 移动会话到顶部
                chatModel.move(targetIndex, 0, 1)
                
                // 重新计算选中索引：找到之前选中的会话现在的位置
                if (oldSelectedId !== "") {
                    for (var k = 0; k < chatModel.count; ++k) {
                        if (String(chatModel.get(k).conversationId || "") === oldSelectedId) {
                            listView.currentIndex = k
                            break
                        }
                    }
                }
            }
        }
    }
}
