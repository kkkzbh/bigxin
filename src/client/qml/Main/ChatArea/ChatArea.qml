import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Effects

import WeChatClient as AppTheme
import "../Dialogs"

Rectangle {
    id: root

    // 全局主题单例
    readonly property var theme: AppTheme.Theme

    color: theme.chatAreaBackground

    // 外部通过 Main.qml 传入是否有选中会话
    property bool hasSelection: true
    // 当前选中的会话 ID，用于按会话显示消息。
    property string conversationId: ""
    // 当前选中的会话类型（GROUP / SINGLE）。
    property string conversationType: ""
    // 当前会话标题 / 昵称，用于标题栏显示。
    property string conversationTitle: ""
    // 当前会话头像路径（用于群聊头像显示）。
    property string conversationAvatarPath: ""
    // 单聊对端用户 ID（仅 SINGLE 类型会话时有效）。
    property string peerUserId: ""

    // 当前会话成员信息缓存：以 userId 为键。
    property var memberMap: ({})
    // 当前用户在会话中的角色。
    property string myRole: ""
    // 当前用户是否被禁言。
    property bool isMuted: false
    property real mutedUntilMs: 0

    // 右键菜单上下文（用户）
    property string contextTargetUserId: ""
    property string contextTargetName: ""

    // 右键菜单上下文（消息）
    property string contextMessageId: ""
    property string contextMessageContent: ""
    property bool contextIsMyMessage: false
    property var contextMessageReactions: ({})

    // AI 生成状态
    property bool aiGenerating: false
    
    // 缓存最后发送的消息内容（用于失败消息显示）
    property string lastSentMessage: ""

    // 未读消息相关
    property int lastReadSeq: 0  // 当前会话的已读位置
    property int unreadCount: 0  // 当前会话的未读数
    property int firstUnreadIndex: -1  // 第一条未读消息在 messageModel 中的索引
    property bool unreadButtonShown: false  // 未读按钮是否已经显示过（每个会话只显示一次）
    
    // 用于防止重复请求成员列表
    property string lastRequestedMembersConvId: ""

    // 正常聊天界面，仅在有选中会话时显示
    property int currentTab: 0
    // 联系人详情（用于通讯录 Tab）
    property string contactName: ""
    property string contactWeChatId: ""
    property string contactSignature: ""
    property string requestStatus: ""
    property string contactUserId: ""
    property string contactRequestId: ""
    property string contactRequestType: ""
    property string contactGroupName: ""
    property string contactGroupId: ""

    function generateAiReply() {
        if (root.aiGenerating) return
        root.aiGenerating = true
        
        // 1. 收集并在必要时截断上下文（最近 100 条）
        var messages = []
        var count = 0
        var maxCount = 100
        // 倒序遍历，收集最近的聊天记录
        for (var i = messageModel.count - 1; i >= 0 && count < maxCount; i--) {
            var item = messageModel.get(i)
            if (!item) continue
            
            var s = item.sender
            if (s === "system") continue
            
            // "other" -> 对方说的话 (user role for AI)
            // "me" -> 我之前说的话 (assistant role for AI)
            messages.unshift({
                "role": (s === "me") ? "assistant" : "user",
                "content": item.content
            })
            count++
        }

        // 2. 构造系统提示词
        var systemPrompt = "你是一个智能聊天助手。根据给定的聊天上下文和用户当前正在输入的内容，帮助用户生成一个自然、得体的回复。\\n" +
                           "规则：\\n" +
                           "1. 回复应简洁自然，符合日常聊天风格\\n" +
                           "2. 如果用户有当前输入，结合输入内容优化回复\\n" +
                           "3. 如果没有用户输入，根据上下文生成合适的回复\\n" +
                           "4. 保持友好、有礼貌的语气\\n" +
                           "5. 只返回回复内容本身，不要添加任何解释或前缀"

        var apiMessages = [
            {"role": "system", "content": systemPrompt}
        ].concat(messages)

        // 3. 结合用户当前输入框内容
        var currentInput = inputArea.text.trim()
        if (currentInput.length > 0) {
            apiMessages.push({
                "role": "system",
                "content": "用户当前输入框中有以下草稿/想法：\"" + currentInput + "\"。请基于此生成最终回复。"
            })
        }

        // 4. 发起请求
        var xhr = new XMLHttpRequest()
        xhr.open("POST", "https://api.deepseek.com/chat/completions")
        xhr.setRequestHeader("Content-Type", "application/json")
        xhr.setRequestHeader("Authorization", "Bearer sk-768afb11a4c94fd9920a4a4b6eda45a7")

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                root.aiGenerating = false
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.choices && response.choices.length > 0) {
                            var reply = response.choices[0].message.content
                            inputArea.text = reply
                        }
                    } catch (e) {
                        console.error("JSON Parse Error:", e)
                    }
                } else {
                    console.error("AI API Error:", xhr.status, xhr.responseText)
                }
            }
        }

        var data = {
            "model": "deepseek-reasoner",
            "messages": apiMessages,
            "temperature": 0.7,
            "max_tokens": 500,
            "stream": false
        }

        xhr.send(JSON.stringify(data))
    }

    // 会话详情侧边栏状态
    property bool detailPanelVisible: false
    property bool detailPanelExpanded: false
    readonly property int detailPanelColumns: 4
    readonly property int detailPanelMaxRows: 3
    readonly property int detailPanelMaxItemsCollapsed: detailPanelColumns * detailPanelMaxRows

    Timer {
        id: muteCountdown
        interval: 1000
        running: root.isMuted
        repeat: true
        onTriggered: {
            if (root.mutedUntilMs > 0 && Date.now() >= root.mutedUntilMs) {
                root.isMuted = false
                root.mutedUntilMs = 0
                running = false
            }
        }
    }

    // 根据当前会话信息决定是否请求成员列表。
    function refreshConversationMembers() {
        if (conversationId === "" || !root.hasSelection) {
            return
        }
        
        // 避免对同一会话重复请求
        if (conversationId === lastRequestedMembersConvId) {
            return
        }
        
        // GROUP 和 SINGLE 都需要请求成员列表
        // GROUP 用于显示成员，SINGLE 用于获取 peerUserId
        if (conversationType === "GROUP" || conversationType === "SINGLE") {
            lastRequestedMembersConvId = conversationId
            loginBackend.requestConversationMembers(conversationId)
        } else {
            memberMap = ({})
            memberListModel.clear()
            myRole = ""
            isMuted = false
            muteCountdown.running = false
            peerUserId = ""
        }
    }

    function updateMembers(members) {
        const map = {}
        memberListModel.clear()

        if (!members || members.length === undefined) {
            console.warn("updateMembers: invalid members", members)
        } else {
            for (let i = 0; i < members.length; ++i) {
                const m = members[i]
                if (!m || !m.userId)
                    continue

                map[m.userId] = m
            }

            // 按角色排序：群主 -> 管理员（按昵称） -> 成员（按昵称）
            const sorted = Object.values(map).sort((a, b) => {
                // 群主优先
                if (a.role === "OWNER") return -1
                if (b.role === "OWNER") return 1
                // 管理员其次
                if (a.role === "ADMIN" && b.role !== "ADMIN") return -1
                if (b.role === "ADMIN" && a.role !== "ADMIN") return 1
                // 同角色按昵称排序
                const nameA = (a.displayName || "").toLowerCase()
                const nameB = (b.displayName || "").toLowerCase()
                return nameA.localeCompare(nameB)
            })

            for (let i = 0; i < sorted.length; ++i) {
                const m = sorted[i]
                memberListModel.append({
                    userId: m.userId,
                    displayName: m.displayName || "",
                    role: m.role || "",
                    mutedUntilMs: m.mutedUntilMs || 0,
                    avatarPath: m.avatarPath || ""
                })
            }
        }

        // 键值映射用于禁言等逻辑，ListModel 用于详情侧栏成员展示。
        root.memberMap = map

        const selfId = loginBackend.userId
        const selfMember = map[selfId]
        if (selfId && selfMember) {
            root.myRole = selfMember.role || ""
            root.mutedUntilMs = selfMember.mutedUntilMs || 0
            root.isMuted = selfMember.mutedUntilMs > Date.now()
            muteCountdown.running = root.isMuted
        } else {
            root.myRole = ""
            root.mutedUntilMs = 0
            root.isMuted = false
            muteCountdown.running = false
        }

        // 对于单聊，提取对端用户 ID
        if (root.conversationType === "SINGLE" && Object.keys(map).length === 2) {
            for (const userId in map) {
                if (userId !== selfId) {
                    root.peerUserId = userId
                    break
                }
            }
        } else {
            root.peerUserId = ""
        }
    }

    function isUserMuted(userId) {
        const m = memberMap[userId]
        if (!m || !m.mutedUntilMs)
            return false
        return m.mutedUntilMs > Date.now()
    }

    function getUserRole(userId) {
        const m = memberMap[userId]
        return m ? (m.role || "") : ""
    }

    function formatNameWithRole(name, role) {
        if (!name) return ""
        if (role === "OWNER") {
            return "[群主] " + name
        } else if (role === "ADMIN") {
            return "[管理员] " + name
        }
        return name
    }

    function getUserAvatarUrl(userId) {
        if (userId === loginBackend.userId) {
            return loginBackend.avatarUrl
        }
        var m = memberMap[userId]
        if (m && m.avatarPath) {
            return loginBackend.resolveAvatarUrl(m.avatarPath)
        }
        return ""
    }

    // 切换会话详情侧边栏（好友 / 群统一）。
    function toggleDetailPanel() {
        if (!root.hasSelection)
            return
        // 打开时默认折叠成员列表
        if (!root.detailPanelVisible) {
            root.detailPanelExpanded = false
            // 打开群聊详情时，如果还没有成员数据，主动请求一次
            if (conversationType === "GROUP"
                    && conversationId !== ""
                    && memberListModel.count === 0) {
                loginBackend.requestConversationMembers(conversationId)
            }
        }
        root.detailPanelVisible = !root.detailPanelVisible
    }

    UserContextMenu {
        id: avatarMenu
        parent: Overlay.overlay
        conversationId: root.conversationId
        targetUserId: root.contextTargetUserId
        targetUserName: root.contextTargetName
        isMuted: root.isUserMuted(root.contextTargetUserId)
        targetRole: root.getUserRole(root.contextTargetUserId)
        myRole: root.myRole

        onMuteRequested: {
            if (!root.muteDialog) {
                root.muteDialog = muteDialogComponent.createObject(root, {
                    transientParent: root.Window.window
                })
            }
            root.muteDialog.conversationId = root.conversationId
            root.muteDialog.targetUserId = root.contextTargetUserId
            root.muteDialog.show()
            root.muteDialog.raise()
            root.muteDialog.requestActivate()
        }

        onUnmuteRequested: {
            loginBackend.unmuteMember(root.conversationId, root.contextTargetUserId)
        }

        onSetAdminRequested: function(isAdmin) {
            loginBackend.setAdmin(root.conversationId, root.contextTargetUserId, isAdmin)
        }
    }

    property var muteDialog: null

    Component {
        id: muteDialogComponent
        MuteMemberDialog {
            onMuteConfirmed: function(convId, userId, duration) {
                loginBackend.muteMember(convId, userId, duration)
            }
        }
    }

    MessageContextMenu {
        id: messageMenu
        parent: Overlay.overlay
        conversationId: root.conversationId
        serverMsgId: root.contextMessageId
        messageContent: root.contextMessageContent
        isMyMessage: root.contextIsMyMessage
        isGroupChat: root.conversationType === "GROUP"
        myRole: root.myRole

        onAboutToShow: {
            console.log("[MessageMenu] About to show menu:")
            console.log("  - serverMsgId:", serverMsgId)
            console.log("  - isMyMessage:", isMyMessage)
            console.log("  - isGroupChat:", isGroupChat)
            console.log("  - myRole:", myRole)
        }

        onCopyRequested: {
            console.log("[MessageMenu] Copy requested - content:", root.contextMessageContent)
            // 复制到剪贴板
            var textEdit = Qt.createQmlObject('import QtQuick; TextEdit { visible: false }', root)
            textEdit.text = root.contextMessageContent
            textEdit.selectAll()
            textEdit.copy()
            textEdit.destroy()
        }

        onRecallRequested: {
            console.log("[MessageMenu] Recall requested - conversationId:", root.conversationId, "serverMsgId:", root.contextMessageId)
            loginBackend.recallMessage(root.conversationId, root.contextMessageId)
        }

        onLikeRequested: {
            console.log("[MessageMenu] Like requested - conversationId:", root.conversationId, "serverMsgId:", root.contextMessageId)
            loginBackend.reactToMessage(root.conversationId, root.contextMessageId, "LIKE")
        }

        onDislikeRequested: {
            console.log("[MessageMenu] Dislike requested - conversationId:", root.conversationId, "serverMsgId:", root.contextMessageId)
            loginBackend.reactToMessage(root.conversationId, root.contextMessageId, "DISLIKE")
        }

        onUnlikeRequested: {
            console.log("[MessageMenu] Unlike requested - conversationId:", root.conversationId, "serverMsgId:", root.contextMessageId)
            loginBackend.unreactToMessage(root.conversationId, root.contextMessageId, "LIKE")
        }

        onUndislikeRequested: {
            console.log("[MessageMenu] Undislike requested - conversationId:", root.conversationId, "serverMsgId:", root.contextMessageId)
            loginBackend.unreactToMessage(root.conversationId, root.contextMessageId, "DISLIKE")
        }
    }

    property var reactionDialog: null

    Component {
        id: reactionDialogComponent
        ReactionDetailsDialog {
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.currentTab
        visible: root.hasSelection

        // Tab 0: Chat Interface
        ColumnLayout {
            id: chatLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
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
                    
                    // 初始化时隐藏，等待数据加载后再显示
                    visible: false

                    // 监听内容高度变化，重置显示定时器
                    onContentHeightChanged: {
                        if (!visible && scrollTimer.running) {
                            scrollTimer.restart()
                        }
                    }
                    
                    Component.onCompleted: scrollTimer.start()

                    delegate: Item {
                        id: messageDelegate
                        width: ListView.view.width

                        property bool isMine: sender === "me"
                        property bool isSystem: sender === "system"
                        property bool isOther: !isMine && !isSystem
                        property bool showName: root.conversationType === "GROUP" && isOther

                        readonly property real contentHeight: Math.max(
                            isSystem ? systemBubble.implicitHeight : 0,
                            isOther ? leftRow.implicitHeight : 0,
                            isMine ? rightRow.implicitHeight : 0
                        )

                        height: contentHeight

                        function hasReactions() {
                            return getLikeCount() > 0 || getDislikeCount() > 0
                        }

                        function getLikeCount() {
                            if (!reactions || !reactions.LIKE) {
                                // console.log("[MessageDelegate] getLikeCount: no reactions or LIKE for", serverMsgId)
                                return 0
                            }
                            var count = reactions.LIKE.length || 0
                            if (count > 0) {
                                console.log("[MessageDelegate] getLikeCount for", serverMsgId, "=", count, "reactions:", JSON.stringify(reactions))
                            }
                            return count
                        }

                        function getDislikeCount() {
                            if (!reactions || !reactions.DISLIKE) {
                                // console.log("[MessageDelegate] getDislikeCount: no reactions or DISLIKE for", serverMsgId)
                                return 0
                            }
                            var count = reactions.DISLIKE.length || 0
                            if (count > 0) {
                                console.log("[MessageDelegate] getDislikeCount for", serverMsgId, "=", count)
                            }
                            return count
                        }

                        function checkHasMyReaction(reactionType) {
                            if (!reactions || !reactions[reactionType]) return false
                            var list = reactions[reactionType]
                            for (var i = 0; i < list.length; i++) {
                                if (list[i].userId === loginBackend.userId) {
                                    return true
                                }
                            }
                            return false
                        }

                        // 其他人消息：头像在左，气泡在右，整体靠左
                        Row {
                            id: leftRow
                            visible: isOther
                            anchors.top: parent.top
                            anchors.left: parent.left
                            spacing: 6

                            Rectangle {
                                width: 42
                                height: 42
                                radius: 6
                                color: "#4fbf73"

                                MouseArea {
                                    id: avatarMouseArea
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: function(mouse) {
                                        if (mouse.button !== Qt.RightButton)
                                            return
                                        if (root.conversationType !== "GROUP")
                                            return
                                        // 群主和管理员都可以右键
                                        if (root.myRole !== "OWNER" && root.myRole !== "ADMIN")
                                            return
                                        // 不能对自己操作
                                        if (senderId === loginBackend.userId)
                                            return
                                        root.contextTargetUserId = senderId
                                        root.contextTargetName = senderName
                                        avatarMenu.close()
                                        avatarMenu.isMuted = root.isUserMuted(root.contextTargetUserId)
                                        avatarMenu.targetRole = root.getUserRole(root.contextTargetUserId)
                                        avatarMenu.popup(parent, mouse.x, mouse.y)
                                    }
                                }

                                clip: true
                                
                                // 文本头像 (Fallback)
                                Text {
                                    anchors.centerIn: parent
                                    text: (senderName || "A").slice(0, 1)
                                    color: "#ffffff"
                                    font.pixelSize: 16
                                    font.bold: true
                                    visible: otherAvatarImg.status !== Image.Ready
                                }

                                // 图片头像
                                Image {
                                    id: otherAvatarImg
                                    anchors.fill: parent
                                    source: root.getUserAvatarUrl(senderId)
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                    asynchronous: true
                                }
                            }

                            Column {
                                spacing: 4
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    visible: showName
                                    text: root.formatNameWithRole(senderName, senderId ? root.getUserRole(senderId) : "")
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                    font.bold: false
                                }

                                Rectangle {
                                    id: leftBubble
                                    color: leftBubbleMouseArea.containsMouse ? Qt.lighter(theme.bubbleOther, 1.05) : theme.bubbleOther
                                    radius: 6
                                    border.color: theme.bubbleOther
                                    implicitWidth: Math.min(messageList.width * 0.7, leftText.implicitWidth + 20)
                                    implicitHeight: leftText.implicitHeight + 14
                                    
                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }

                                    Text {
                                        id: leftText
                                        anchors.margins: 8
                                        anchors.fill: parent
                                        text: content
                                        color: theme.bubbleOtherText
                                        font.pixelSize: 16
                                        wrapMode: Text.Wrap
                                    }

                                    MouseArea {
                                        id: leftBubbleMouseArea
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.RightButton) {
                                                console.log("[MessageMenu] Right-click on other's message, serverMsgId:", serverMsgId)
                                                root.contextMessageId = serverMsgId || ""
                                                root.contextMessageContent = content
                                                root.contextIsMyMessage = false
                                                root.contextMessageReactions = reactions || ({})
                                                messageMenu.hasMyLike = checkHasMyReaction("LIKE")
                                                messageMenu.hasMyDislike = checkHasMyReaction("DISLIKE")
                                                messageMenu.popup(leftBubble, mouse.x, mouse.y)
                                            }
                                        }
                                    }
                                }

                                // 反应统计显示
                                Row {
                                    spacing: 8
                                    visible: hasReactions()

                                    // 点赞
                                    Rectangle {
                                        visible: getLikeCount() > 0
                                        width: likeRow.implicitWidth + 12
                                        height: 24
                                        radius: 12
                                        color: theme.chatListItemSelected
                                        border.color: theme.cardBorder
                                        border.width: 1

                                        Row {
                                            id: likeRow
                                            anchors.centerIn: parent
                                            spacing: 4

                                            Text {
                                                text: "👍"
                                                font.pixelSize: 14
                                            }

                                            Text {
                                                text: getLikeCount()
                                                color: theme.textPrimary
                                                font.pixelSize: 12
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!root.reactionDialog) {
                                                    root.reactionDialog = reactionDialogComponent.createObject(root)
                                                }
                                                root.reactionDialog.serverMsgId = serverMsgId
                                                root.reactionDialog.reactions = reactions
                                                root.reactionDialog.currentTab = 0
                                                root.reactionDialog.show()
                                            }
                                        }
                                    }

                                    // 点踩
                                    Rectangle {
                                        visible: getDislikeCount() > 0
                                        width: dislikeRow.implicitWidth + 12
                                        height: 24
                                        radius: 12
                                        color: theme.chatListItemSelected
                                        border.color: theme.cardBorder
                                        border.width: 1

                                        Row {
                                            id: dislikeRow
                                            anchors.centerIn: parent
                                            spacing: 4

                                            Text {
                                                text: "👎"
                                                font.pixelSize: 14
                                            }

                                            Text {
                                                text: getDislikeCount()
                                                color: theme.textPrimary
                                                font.pixelSize: 12
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!root.reactionDialog) {
                                                    root.reactionDialog = reactionDialogComponent.createObject(root)
                                                }
                                                root.reactionDialog.serverMsgId = serverMsgId
                                                root.reactionDialog.reactions = reactions
                                                root.reactionDialog.currentTab = 1
                                                root.reactionDialog.show()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 自己的消息：气泡在左，头像在右，整体靠右
                        Row {
                            id: rightRow
                            visible: isMine
                            anchors.top: parent.top
                            anchors.right: parent.right
                            spacing: 6
                            layoutDirection: Qt.RightToLeft

                            Rectangle {
                                width: 42
                                height: 42
                                radius: 6
                                color: "#ffffff"
                                clip: true

                                Text {
                                    anchors.centerIn: parent
                                    text: "我"
                                    color: "#222222"
                                    font.pixelSize: 16
                                    font.bold: true
                                    visible: mineAvatarImg.status !== Image.Ready
                                }

                                Image {
                                    id: mineAvatarImg
                                    anchors.fill: parent
                                    source: loginBackend.avatarUrl
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                    asynchronous: true
                                }
                            }

                            Column {
                                spacing: 4
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    id: rightBubble
                                    color: {
                                        if (model.isFailed) return "#e74c3c"
                                        return rightBubbleMouseArea.containsMouse ? Qt.darker(theme.bubbleMine, 1.05) : theme.bubbleMine
                                    }
                                    radius: 6
                                    border.color: model.isFailed ? "#e74c3c" : theme.bubbleMine
                                    implicitWidth: Math.min(messageList.width * 0.7, rightText.implicitWidth + 20)
                                    implicitHeight: rightText.implicitHeight + 14
                                    
                                    Behavior on color {
                                        ColorAnimation { duration: 150 }
                                    }

                                    Text {
                                        id: rightText
                                        anchors.margins: 8
                                        anchors.fill: parent
                                        text: content
                                        color: theme.bubbleMineText
                                        font.pixelSize: 16
                                        wrapMode: Text.Wrap
                                    }

                                    MouseArea {
                                        id: rightBubbleMouseArea
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.RightButton) {
                                                console.log("[MessageMenu] Right-click on my message, serverMsgId:", serverMsgId)
                                                root.contextMessageId = serverMsgId || ""
                                                root.contextMessageContent = content
                                                root.contextIsMyMessage = true
                                                root.contextMessageReactions = reactions || ({})
                                                messageMenu.hasMyLike = false
                                                messageMenu.hasMyDislike = false
                                                messageMenu.popup(rightBubble, mouse.x, mouse.y)
                                            }
                                        }
                                    }
                                }

                                // 反应统计显示
                                Row {
                                    spacing: 8
                                    visible: hasReactions()
                                    layoutDirection: Qt.RightToLeft

                                    // 点踩
                                    Rectangle {
                                        visible: getDislikeCount() > 0
                                        width: dislikeRowRight.implicitWidth + 12
                                        height: 24
                                        radius: 12
                                        color: theme.chatListItemSelected
                                        border.color: theme.cardBorder
                                        border.width: 1

                                        Row {
                                            id: dislikeRowRight
                                            anchors.centerIn: parent
                                            spacing: 4

                                            Text {
                                                text: "👎"
                                                font.pixelSize: 14
                                            }

                                            Text {
                                                text: getDislikeCount()
                                                color: theme.textPrimary
                                                font.pixelSize: 12
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!root.reactionDialog) {
                                                    root.reactionDialog = reactionDialogComponent.createObject(root)
                                                }
                                                root.reactionDialog.serverMsgId = serverMsgId
                                                root.reactionDialog.reactions = reactions
                                                root.reactionDialog.currentTab = 1
                                                root.reactionDialog.show()
                                            }
                                        }
                                    }

                                    // 点赞
                                    Rectangle {
                                        visible: getLikeCount() > 0
                                        width: likeRowRight.implicitWidth + 12
                                        height: 24
                                        radius: 12
                                        color: theme.chatListItemSelected
                                        border.color: theme.cardBorder
                                        border.width: 1

                                        Row {
                                            id: likeRowRight
                                            anchors.centerIn: parent
                                            spacing: 4

                                            Text {
                                                text: "👍"
                                                font.pixelSize: 14
                                            }

                                            Text {
                                                text: getLikeCount()
                                                color: theme.textPrimary
                                                font.pixelSize: 12
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (!root.reactionDialog) {
                                                    root.reactionDialog = reactionDialogComponent.createObject(root)
                                                }
                                                root.reactionDialog.serverMsgId = serverMsgId
                                                root.reactionDialog.reactions = reactions
                                                root.reactionDialog.currentTab = 0
                                                root.reactionDialog.show()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 系统消息：居中显示
                        Item {
                            id: systemRow
                            visible: isSystem && content && content.trim().length > 0
                            anchors.top: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: systemBubble.implicitWidth
                            height: systemBubble.implicitHeight

                            Rectangle {
                                id: systemBubble
                                color: theme.cardBackground
                                radius: 6
                                border.color: theme.cardBorder
                                implicitWidth: Math.min(messageList.width * 0.8, systemText.implicitWidth + 20)
                                implicitHeight: systemText.implicitHeight + 12

                                Text {
                                    id: systemText
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    text: content
                                    color: theme.textPrimary
                                    font.pixelSize: 12
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    width: parent.width - 16
                                }
                            }
                        }
                    }
                }
                
                // 跳转到未读消息的悬浮按钮
                Rectangle {
                    id: unreadButton
                    // 使用独立的显示标志，避免绑定循环
                    visible: false
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: 30
                    anchors.topMargin: 20
                    width: 120
                    height: 36
                    radius: 18
                    color: "#07c160"
                    z: 100

                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: "#40000000"
                        shadowBlur: 0.5
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 2
                    }

                    Text {
                        anchors.centerIn: parent
                        text: unreadCount + " 条新消息"
                        color: "#ffffff"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (firstUnreadIndex >= 0 && firstUnreadIndex < messageModel.count) {
                                messageList.positionViewAtIndex(firstUnreadIndex, ListView.Beginning)
                            }
                            // 点击后隐藏按钮
                            unreadButton.visible = false
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
                            id: aiHelpBtn
                            enabled: !root.aiGenerating && !root.isMuted
                            
                            contentItem: Image {
                                source: "../../resource/ChatArea/openai.svg"
                                fillMode: Image.PreserveAspectFit
                                opacity: parent.enabled ? 1.0 : 0.4
                                sourceSize.width: 20
                                sourceSize.height: 20
                                horizontalAlignment: Image.AlignHCenter
                                verticalAlignment: Image.AlignVCenter
                            }
                            
                            background: Rectangle {
                                color: aiHelpBtn.hovered ? "#33888888" : "transparent"
                                radius: 4
                            }
                            
                            onClicked: root.generateAiReply()
                        }
                    }

                    TextArea {
                        id: inputArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        enabled: !root.isMuted
                        wrapMode: TextEdit.Wrap
                        color: theme.textPrimary
                        font.pixelSize: 20
                        placeholderText: root.isMuted ? qsTr("已被禁言") : ""
                        background: Rectangle {
                            radius: 4
                            color: theme.chatAreaBackground
                        }

                        // 禁言提示覆盖层
                        Rectangle {
                            anchors.fill: parent
                            visible: root.isMuted
                            color: "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("已被禁言")
                                color: theme.textSecondary
                                font.pixelSize: 14
                            }
                        }

                        Keys.onReturnPressed: function(event) {
                            if (event.modifiers & Qt.ShiftModifier) {
                                inputArea.text = inputArea.text + "\n"
                                inputArea.cursorPosition = inputArea.text.length
                            } else {
                                if (inputArea.text.length > 0) {
                                    root.lastSentMessage = inputArea.text.trim()
                                    loginBackend.sendMessage(root.conversationId, inputArea.text)
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
                                    root.lastSentMessage = inputArea.text.trim()
                                    loginBackend.sendMessage(root.conversationId, inputArea.text)
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
                            enabled: !root.isMuted && inputArea.text.length > 0
                            background: Rectangle {
                                radius: 4
                                color: enabled ? theme.sendButtonEnabled
                                                : theme.sendButtonDisabled
                            }
                            onClicked: {
                                root.lastSentMessage = inputArea.text.trim()
                                loginBackend.sendMessage(root.conversationId, inputArea.text)
                                inputArea.text = ""
                                // 发送消息后隐藏未读按钮
                                if (unreadButton.visible) {
                                    unreadButton.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }

        // Tab 1: Contact Detail
        ContactDetailView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            hasSelection: root.hasSelection
            contactName: root.contactName
            contactWeChatId: root.contactWeChatId
            contactSignature: root.contactSignature
            requestStatus: root.requestStatus
            contactUserId: root.contactUserId
            requestId: root.contactRequestId
            requestType: root.contactRequestType
            groupName: root.contactGroupName
            groupId: root.contactGroupId
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

    // 会话成员列表（用于详情侧边栏展示）。
    ListModel {
        id: memberListModel
    }

    // 当会话发生变化时，清空当前消息，并通过后端打开会话（先尝试从本地缓存加载，再必要时请求服务器历史）。
    onConversationIdChanged: {
        //切换会话时先隐藏列表，防止看到滚动到底部的“跳跃”过程
        messageList.visible = false
        
        messageModel.clear()
        memberMap = ({})
        memberListModel.clear()
        myRole = ""
        isMuted = false
        peerUserId = ""  // 重置 peerUserId，防止旧值残留
        detailPanelVisible = false
        detailPanelExpanded = false
        firstUnreadIndex = -1  // 重置第一条未读消息索引
        unreadButtonShown = false  // 重置未读按钮显示状态
        unreadButton.visible = false  // 隐藏未读按钮
        lastRequestedMembersConvId = ""  // 重置成员请求记录
        if (conversationId !== "") {
            loginBackend.openConversation(conversationId)
            refreshConversationMembers()
            // 延迟标记已读，等消息加载完成
            markReadTimer.restart()
        }
        // 启动定时器，确保即使没有消息（或消息加载完后）也能触发显示
        scrollTimer.restart()
    }

    // 防止会话类型变化时漏掉成员请求（例如先更新 ID，后更新类型）。
    onConversationTypeChanged: {
        refreshConversationMembers()
    }

    Timer {
        id: scrollTimer
        interval: 100  // 给足够时间让消息加载和布局完成
        repeat: false
        onTriggered: {
            messageList.positionViewAtEnd()
            // 滚动完成后再显示列表，实现“无缝”切换到底部的效果
            messageList.visible = true
        }
    }

    // 用于标记已读的计时器，避免频繁请求
    Timer {
        id: markReadTimer
        interval: 500
        repeat: false
        onTriggered: {
            if (conversationId !== "" && messageModel.count > 0) {
                // 获取最后一条消息的 seq
                var lastItem = messageModel.get(messageModel.count - 1)
                if (lastItem && lastItem.seq > 0) {
                    loginBackend.markConversationAsRead(conversationId, lastItem.seq)
                }
            }
            // 标记已读后，检查是否需要显示未读按钮
            checkAndShowUnreadButton()
        }
    }
    
    // 检查并显示未读按钮
    function checkAndShowUnreadButton() {
        // 只有在未读数>=10且未显示过且有第一条未读消息时才显示
        if (unreadCount >= 10 && !unreadButtonShown && firstUnreadIndex >= 0) {
            unreadButton.visible = true
            unreadButtonShown = true
        }
    }

    Connections {
        target: loginBackend

        function onMessageReceived(conversationId,
                                   senderId,
                                   senderDisplayName,
                                   content,
                                   msgType,
                                   serverTimeMs,
                                   seq,
                                   serverMsgId,
                                   reactions) {
            // 如果收到的消息不是当前会话，不处理显示，但不阻止执行（后续可能需要更新未读数）
            if (conversationId !== root.conversationId)
                return
            const mine = senderId === loginBackend.userId
            const type = (msgType || "").toString()
            const sys = type === "SYSTEM"
            const isFailedMsg = type === "FAILED_TEXT"
            const trimmed = (content || "").trim()
            // 防御空字符串系统消息：不展示、不占位
            if (sys && trimmed.length === 0)
                return
            // 临时方案：如果服务器没返回 serverMsgId，使用 seq 作为替代
            var msgId = serverMsgId || String(seq)
            console.log("[ChatArea] Received message - seq:", seq, "serverMsgId:", serverMsgId, "using:", msgId, "reactions:", JSON.stringify(reactions))
            
            messageModel.append({
                sender: sys ? "system" : (mine ? "me" : "other"),
                senderName: senderDisplayName,
                senderId: senderId,
                content: trimmed,
                isFailed: isFailedMsg,  // 只有 FAILED_TEXT 设置为 true
                seq: seq,  // 保存消息序号
                serverMsgId: msgId,  // 服务器消息 ID（或使用 seq 作为临时替代）
                reactions: reactions || ({}),  // 反应统计
                isRecalled: false  // 是否已撤回
            })
            
            // 更新第一条未读消息的索引（只记录别人发送的未读消息）
            if (firstUnreadIndex === -1 && seq > lastReadSeq && !mine) {
                firstUnreadIndex = messageModel.count - 1
            }
            
            // 如果用户正在当前会话且收到新消息（不是自己发的），立即标记为已读
            if (!mine && messageList.visible) {
                // 延迟标记已读，确保消息已添加到模型
                Qt.callLater(function() {
                    if (conversationId === root.conversationId) {
                        loginBackend.markConversationAsRead(conversationId, seq)
                    }
                })
            }
            
            // 如果列表已显示，直接滚动；否则重置显示定时器
            if (messageList.visible) {
                messageList.positionViewAtEnd()
            } else {
                scrollTimer.restart()
            }
        }

        // 消息发送失败处理（例如非好友发送消息）
        function onMessageSendFailed(conversationId, errorMessage) {
            // 添加失败的消息气泡（红色）
            messageModel.append({
                sender: "me",
                senderName: loginBackend.displayName,
                senderId: loginBackend.userId,
                content: root.lastSentMessage,
                isFailed: true  // 标记为失败状态
            })
            // 添加系统提示消息
            messageModel.append({
                sender: "system",
                senderName: "",
                senderId: "",
                content: errorMessage,  // "请添加对方为好友"
                isFailed: false
            })
            // 滚动到底部
            if (messageList.visible) {
                messageList.positionViewAtEnd()
            } else {
                scrollTimer.restart()
            }
        }
    }

    Connections {
        target: loginBackend
        function onConversationMembersReady(conversationId, members) {
            if (conversationId !== root.conversationId)
                return
            updateMembers(members)
        }

        function onMessageRecalled(conversationId, serverMsgId, recallerId, recallerName) {
            if (conversationId !== root.conversationId)
                return
            
            // 查找并更新消息
            for (var i = 0; i < messageModel.count; i++) {
                var msg = messageModel.get(i)
                if (msg.serverMsgId === serverMsgId) {
                    // 将消息改为系统消息
                    messageModel.set(i, {
                        sender: "system",
                        senderName: "",
                        senderId: "",
                        content: recallerName + " 撤回了一条消息",
                        isFailed: false,
                        seq: msg.seq,
                        serverMsgId: msg.serverMsgId,
                        reactions: ({}),
                        isRecalled: true
                    })
                    break
                }
            }
        }

        function onMessageReactionUpdated(conversationId, serverMsgId, reactions) {
            console.log("[ChatArea] onMessageReactionUpdated called - conversationId:", conversationId, "serverMsgId:", serverMsgId, "reactions:", JSON.stringify(reactions))
            
            if (conversationId !== root.conversationId) {
                console.log("[ChatArea] Conversation mismatch, ignoring")
                return
            }
            
            // 查找并更新消息的反应
            var found = false
            for (var i = 0; i < messageModel.count; i++) {
                var msg = messageModel.get(i)
                
                // 使用字符串比较，确保类型一致
                if (String(msg.serverMsgId) === String(serverMsgId)) {
                    console.log("[ChatArea] Found matching message at index", i, "- updating reactions")
                    
                    // QML ListModel 的对象属性更新需要完整替换才能触发绑定
                    // 先保存其他属性，然后重新设置整个 item
                    var updatedMsg = {
                        sender: msg.sender,
                        senderName: msg.senderName,
                        senderId: msg.senderId,
                        content: msg.content,
                        isFailed: msg.isFailed,
                        seq: msg.seq,
                        serverMsgId: msg.serverMsgId,
                        reactions: reactions,  // 新的 reactions
                        isRecalled: msg.isRecalled || false
                    }
                    
                    messageModel.set(i, updatedMsg)
                    console.log("[ChatArea] Message reactions updated successfully")
                    found = true
                    break
                }
            }
            
            if (!found) {
                console.log("[ChatArea] WARNING: Message with serverMsgId", serverMsgId, "not found in messageModel")
                console.log("[ChatArea] Total messages in model:", messageModel.count)
            }
        }
    }

    onHasSelectionChanged: {
        if (!hasSelection) {
            detailPanelVisible = false
            detailPanelExpanded = false
        }
    }

    // 会话详情侧边栏（点击标题栏右侧按钮，从右侧滑入）
    Item {
        id: detailPanelOverlay
        anchors.fill: parent
        z: 10
        visible: root.hasSelection

        // 点击聊天区域空白处关闭详情面板
        MouseArea {
            anchors.fill: parent
            enabled: root.detailPanelVisible
            onClicked: {
                root.detailPanelVisible = false
                root.detailPanelExpanded = false
            }
        }

        Rectangle {
            id: detailPanel
            width: 320
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            color: theme.panelBackground
            border.color: theme.separatorVertical
            anchors.rightMargin: root.detailPanelVisible ? 0 : -width

            Behavior on anchors.rightMargin {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.InOutQuad
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // 可滚动内容区域（成员网格等）
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 12

                        // 群信息头部（群头像、群名称、群号）
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            visible: root.conversationType === "GROUP"

                            // 群头像
                            Item {
                                Layout.alignment: Qt.AlignHCenter
                                width: 64
                                height: 64

                                Rectangle {
                                    id: groupAvatarRect
                                    anchors.fill: parent
                                    radius: 8
                                    color: "#4fbf73"
                                    clip: true

                                    // 头像文本
                                    Text {
                                        anchors.centerIn: parent
                                        text: (root.conversationTitle || "").slice(0, 1)
                                        color: "#ffffff"
                                        font.pixelSize: 28
                                        font.bold: true
                                        visible: groupAvatarImg.status !== Image.Ready
                                    }

                                    // 头像图片（如果有）
                                    Image {
                                        id: groupAvatarImg
                                        anchors.fill: parent
                                        source: loginBackend.resolveAvatarUrl(root.conversationAvatarPath || "")
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                        asynchronous: true
                                    }

                                    // 悬停时显示的半透明遮罩（仅群主和管理员可见）
                                    Rectangle {
                                        anchors.fill: parent
                                        color: "#80000000"
                                        radius: 8
                                        visible: groupAvatarMouseArea.containsMouse 
                                                 && (root.myRole === "OWNER" || root.myRole === "ADMIN")

                                        // 编辑图标
                                        Text {
                                            anchors.centerIn: parent
                                            text: "✎"
                                            color: "#ffffff"
                                            font.pixelSize: 24
                                        }
                                    }

                                    MouseArea {
                                        id: groupAvatarMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: (root.myRole === "OWNER" || root.myRole === "ADMIN") 
                                                     ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        enabled: root.myRole === "OWNER" || root.myRole === "ADMIN"
                                        onClicked: {
                                            groupAvatarPicker.open()
                                        }
                                    }
                                }
                            }

                            // 群名称
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.fillWidth: true
                                text: root.conversationTitle || ""
                                color: theme.textPrimary
                                font.pixelSize: 16
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }

                            // 群号
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("群号: ") + (root.conversationId || "")
                                color: theme.textSecondary
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        // 修改群名（仅群主和管理员可见）
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            spacing: 8
                            visible: root.conversationType === "GROUP" 
                                     && (root.myRole === "OWNER" || root.myRole === "ADMIN")

                            TextField {
                                id: groupNameField
                                Layout.fillWidth: true
                                placeholderText: qsTr("输入新群名…")
                                text: root.conversationTitle || ""
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
                                text: qsTr("修改")
                                implicitWidth: 60
                                implicitHeight: 28
                                enabled: groupNameField.text.trim().length > 0
                                         && groupNameField.text.trim() !== root.conversationTitle
                                background: Rectangle {
                                    radius: 4
                                    color: parent.enabled ? theme.primaryButton
                                                         : theme.sendButtonDisabled
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "#ffffff"
                                    font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    var newName = groupNameField.text.trim()
                                    if (newName.length > 0 && newName !== root.conversationTitle) {
                                        loginBackend.renameGroup(root.conversationId, newName)
                                    }
                                }
                            }
                        }

                        // 分隔线
                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: theme.separatorHorizontal
                            visible: root.conversationType === "GROUP"
                            Layout.topMargin: 4
                            Layout.bottomMargin: 4
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.conversationType === "GROUP"
                                  ? qsTr("群成员")
                                  : qsTr("聊天成员")
                            color: theme.textSecondary
                            font.pixelSize: 12
                        }

                        // 成员头像 + 昵称网格，固定列数，超过三行时默认折叠
                        GridLayout {
                            id: membersGrid
                            Layout.fillWidth: true
                            columns: root.detailPanelColumns
                            columnSpacing: 12
                            rowSpacing: 12

                            Repeater {
                                model: memberListModel

                                ColumnLayout {
                                    Layout.preferredWidth: 64
                                    Layout.alignment: Qt.AlignHCenter
                                    spacing: 4
                                    visible: root.detailPanelExpanded
                                             || index < root.detailPanelMaxItemsCollapsed

                                    Rectangle {
                                        width: 48
                                        height: 48
                                        radius: 6
                                        color: "#4fbf73"
                                        clip: true

                                        Text {
                                            anchors.centerIn: parent
                                            text: (displayName || userId || "").slice(0, 1)
                                            color: "#ffffff"
                                            font.pixelSize: 18
                                            font.bold: true
                                            visible: memberAvatarImg.status !== Image.Ready
                                        }

                                        Image {
                                            id: memberAvatarImg
                                            anchors.fill: parent
                                            // 侧边栏成员 model 里有 avatarPath
                                            source: loginBackend.resolveAvatarUrl(avatarPath)
                                            fillMode: Image.PreserveAspectCrop
                                            visible: status === Image.Ready
                                            asynchronous: true
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.formatNameWithRole(displayName || userId, role)
                                        color: theme.textSecondary
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Button {
                            Layout.alignment: Qt.AlignLeft
                            visible: memberListModel.count > root.detailPanelMaxItemsCollapsed
                            text: root.detailPanelExpanded ? qsTr("收起") : qsTr("查看更多")
                            implicitWidth: 96
                            implicitHeight: 28
                            background: Rectangle {
                                color: theme.chatListItemSelected
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: theme.textPrimary
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: root.detailPanelExpanded = !root.detailPanelExpanded
                        }
                    }
                }

                // 底部“退出群聊”按钮
                Button {
                    visible: root.conversationType === "GROUP"
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    Layout.bottomMargin: 24
                    implicitWidth: 120
                    implicitHeight: 32
                    text: root.myRole === "OWNER" ? qsTr("解散群聊") : qsTr("退出群聊")
                    background: Rectangle {
                        color: "transparent"
                        radius: 4
                        border.color: theme.dangerRed
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: theme.dangerRed
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (!root.conversationId || root.conversationId === "")
                            return
                        loginBackend.leaveConversation(root.conversationId)
                    }
                }

                // 底部"删除好友"按钮（仅单聊）
                Button {
                    visible: root.conversationType === "SINGLE"
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                    Layout.bottomMargin: 24
                    implicitWidth: 120
                    implicitHeight: 32
                    text: qsTr("删除好友")
                    background: Rectangle {
                        color: "transparent"
                        radius: 4
                        border.color: theme.dangerRed
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: theme.dangerRed
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        console.log("[删除好友] conversationType:", root.conversationType)
                        console.log("[删除好友] peerUserId:", root.peerUserId)
                        if (!root.peerUserId || root.peerUserId === "") {
                            console.log("[删除好友] peerUserId 为空，无法删除")
                            return
                        }
                        console.log("[删除好友] 调用 deleteFriend, peerUserId:", root.peerUserId)
                        loginBackend.deleteFriend(root.peerUserId)
                        // 删除好友后关闭详情面板
                        root.detailPanelVisible = false
                    }
                }
            }
        }
    }

    // 群聊头像选择器
    FileDialog {
        id: groupAvatarPicker
        title: qsTr("选择群头像")
        nameFilters: [qsTr("图片文件 (*.jpg *.png *.jpeg *.bmp)")]
        onAccepted: {
            var path = groupAvatarPicker.selectedFile.toString()
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
            loginBackend.updateGroupAvatar(root.conversationId, path)
        }
    }
}
