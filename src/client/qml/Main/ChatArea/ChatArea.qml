import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

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

    // 当前会话成员信息缓存：以 userId 为键。
    property var memberMap: ({})
    // 当前用户在会话中的角色。
    property string myRole: ""
    // 当前用户是否被禁言。
    property bool isMuted: false
    property real mutedUntilMs: 0

    // 右键菜单上下文
    property string contextTargetUserId: ""
    property string contextTargetName: ""

    // 正常聊天界面，仅在有选中会话时显示
    property int currentTab: 0
    // 联系人详情（用于通讯录 Tab）
    property string contactName: ""
    property string contactWeChatId: ""
    property string contactSignature: ""
    property string requestStatus: ""
    property string contactUserId: ""
    property string contactRequestId: ""

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
        if (conversationType !== "GROUP") {
            memberMap = ({})
            memberListModel.clear()
            myRole = ""
            isMuted = false
            muteCountdown.running = false
            return
        }
        loginBackend.requestConversationMembers(conversationId)
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
                memberListModel.append({
                    userId: m.userId,
                    displayName: m.displayName || "",
                    role: m.role || "",
                    mutedUntilMs: m.mutedUntilMs || 0
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
    }

    function isUserMuted(userId) {
        const m = memberMap[userId]
        if (!m || !m.mutedUntilMs)
            return false
        return m.mutedUntilMs > Date.now()
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
                    Component.onCompleted: positionViewAtEnd()

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

                        // 其他人消息：头像在左，气泡在右，整体靠左
                        Row {
                            id: leftRow
                            visible: isOther
                            anchors.top: parent.top
                            anchors.left: parent.left
                            spacing: 6

                            Rectangle {
                                width: 36
                                height: 36
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
                                        if (root.myRole !== "OWNER")
                                            return
                                        root.contextTargetUserId = senderId
                                        root.contextTargetName = senderName
                                        avatarMenu.close()
                                        avatarMenu.isMuted = root.isUserMuted(root.contextTargetUserId)
                                        avatarMenu.popup(parent, mouse.x, mouse.y)
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "TA"
                                    color: "#ffffff"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }

                            Column {
                                spacing: 4
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    visible: showName
                                    text: senderName
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                    font.bold: false
                                }

                                Rectangle {
                                    id: leftBubble
                                    color: theme.bubbleOther
                                    radius: 6
                                    border.color: theme.bubbleOther
                                    implicitWidth: Math.min(messageList.width * 0.7, leftText.implicitWidth + 20)
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
                        }

                        // 自己的消息：气泡在左，头像在右，整体靠右
                        Row {
                            id: rightRow
                            visible: isMine
                            anchors.top: parent.top
                            anchors.right: parent.right
                            spacing: 6

                            Rectangle {
                                id: rightBubble
                                color: theme.bubbleMine
                                radius: 6
                                border.color: theme.bubbleMine
                                implicitWidth: Math.min(messageList.width * 0.7, rightText.implicitWidth + 20)
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
                                loginBackend.sendMessage(root.conversationId, inputArea.text)
                                inputArea.text = ""
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
        messageModel.clear()
        memberMap = ({})
        memberListModel.clear()
        myRole = ""
        isMuted = false
        detailPanelVisible = false
        detailPanelExpanded = false
        if (conversationId !== "") {
            loginBackend.openConversation(conversationId)
            refreshConversationMembers()
        }
    }

    // 防止会话类型变化时漏掉成员请求（例如先更新 ID，后更新类型）。
    onConversationTypeChanged: {
        refreshConversationMembers()
    }

    Connections {
        target: loginBackend

        function onMessageReceived(conversationId,
                                   senderId,
                                   senderDisplayName,
                                   content,
                                   msgType,
                                   serverTimeMs,
                                   seq) {
            if (conversationId !== root.conversationId)
                return
            const mine = senderId === loginBackend.userId
            const type = (msgType || "").toString()
            const sys = type === "SYSTEM"
            const trimmed = (content || "").trim()
            // 防御空字符串系统消息：不展示、不占位
            if (sys && trimmed.length === 0)
                return
            messageModel.append({
                sender: sys ? "system" : (mine ? "me" : "other"),
                senderName: senderDisplayName,
                senderId: senderId,
                content: trimmed
            })
            messageList.positionViewAtEnd()
        }
    }

    Connections {
        target: loginBackend
        function onConversationMembersReady(conversationId, members) {
            if (conversationId !== root.conversationId)
                return
            updateMembers(members)
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

                                        Text {
                                            anchors.centerIn: parent
                                            text: (displayName || userId || "").slice(0, 1)
                                            color: "#ffffff"
                                            font.pixelSize: 18
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: displayName || userId
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
            }
        }
    }
}
