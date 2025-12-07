import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import WeChatClient as AppTheme

// 搜索结果下拉面板
Popup {
    id: root

    readonly property var theme: AppTheme.Theme

    // 搜索关键词
    property string searchText: ""

    // 数据源
    property var chatModel: null
    property var contactsModel: null
    property var groupsModel: null

    // 信号
    signal conversationClicked(string conversationId)
    signal friendClicked(string friendUserId)
    signal groupClicked(string conversationId)
    signal addFriendClicked(string query)
    signal addGroupClicked(string query)

    // 搜索结果
    property var conversationResults: []
    property var friendResults: []
    property var groupResults: []

    function refreshIfActive() {
        // 当已有输入但模型数据刚刚加载/变化时，重新计算一次结果，避免首次登录时需要再敲一个字符。
        if (searchText && searchText.trim().length > 0) {
            updateSearchResults()
        }
    }

    // 自动更新搜索结果
    onSearchTextChanged: {
        updateSearchResults()
    }

    // 模型变更/数据加载后，如果当前已有搜索内容则重新刷新结果。
    Connections {
        target: chatModel
        function onCountChanged() { root.refreshIfActive() }
    }
    Connections {
        target: contactsModel
        function onCountChanged() { root.refreshIfActive() }
    }
    Connections {
        target: groupsModel
        function onCountChanged() { root.refreshIfActive() }
    }

    // 转义正则表达式特殊字符
    function escapeRegex(str) {
        return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
    }

    // 高亮匹配文本
    function highlightMatch(text, keyword) {
        if (!keyword || keyword.length === 0) return text
        var escaped = escapeRegex(keyword)
        var regex = new RegExp("(" + escaped + ")", "gi")
        return text.replace(regex, '<font color="#4fbf73">$1</font>')
    }

    // 计算匹配字符数
    function countMatches(text, keyword) {
        if (!text || !keyword) return 0
        var lowerText = text.toLowerCase()
        var lowerKeyword = keyword.toLowerCase()
        var count = 0
        var index = 0
        while ((index = lowerText.indexOf(lowerKeyword, index)) !== -1) {
            count++
            index += lowerKeyword.length
        }
        return count
    }

    // 更新搜索结果
    function updateSearchResults() {
        if (!searchText || searchText.trim().length === 0) {
            conversationResults = []
            friendResults = []
            groupResults = []
            return
        }

        var keyword = searchText.trim().toLowerCase()
        var convResults = []
        var frdResults = []
        var grpResults = []

        // 搜索会话
        if (chatModel) {
            for (var i = 0; i < chatModel.count; i++) {
                var item = chatModel.get(i)
                if (!item || !item.title) continue
                
                var title = item.title
                if (title.toLowerCase().indexOf(keyword) !== -1) {
                    var matchCount = countMatches(title, searchText.trim())
                    convResults.push({
                        conversationId: item.conversationId,
                        title: title,
                        avatarPath: item.avatarPath || "",
                        avatarColor: item.avatarColor || "#4fbf73",
                        initials: item.initials || "",
                        matchCount: matchCount
                    })
                }
            }
        }

        // 搜索好友
        if (contactsModel) {
            for (var j = 0; j < contactsModel.count; j++) {
                var contact = contactsModel.get(j)
                if (!contact) continue
                
                var name = contact.name || ""
                var wechatId = contact.wechatId || ""
                var matched = false
                var matches = 0
                
                if (name.toLowerCase().indexOf(keyword) !== -1) {
                    matched = true
                    matches += countMatches(name, searchText.trim())
                }
                if (wechatId.toLowerCase().indexOf(keyword) !== -1) {
                    matched = true
                    matches += countMatches(wechatId, searchText.trim())
                }
                
                if (matched) {
                    frdResults.push({
                        userId: contact.userId,
                        name: name,
                        wechatId: wechatId,
                        avatarPath: contact.avatarPath || "",
                        avatarColor: contact.avatarColor || "#4fbf73",
                        matchCount: matches
                    })
                }
            }
        }

        // 搜索群聊
        if (groupsModel) {
            for (var k = 0; k < groupsModel.count; k++) {
                var group = groupsModel.get(k)
                if (!group) continue
                
                var groupName = group.name || ""
                var groupId = group.conversationId || ""
                var groupMatched = false
                var groupMatches = 0
                
                if (groupName.toLowerCase().indexOf(keyword) !== -1) {
                    groupMatched = true
                    groupMatches += countMatches(groupName, searchText.trim())
                }
                if (groupId.toLowerCase().indexOf(keyword) !== -1) {
                    groupMatched = true
                    groupMatches += countMatches(groupId, searchText.trim())
                }
                
                if (groupMatched) {
                    grpResults.push({
                        conversationId: groupId,
                        name: groupName,
                        avatarPath: group.avatarPath || "",
                        avatarColor: group.avatarColor || "#4fbf73",
                        matchCount: groupMatches
                    })
                }
            }
        }

        // 按匹配数排序（降序）
        convResults.sort(function(a, b) { return b.matchCount - a.matchCount })
        frdResults.sort(function(a, b) { return b.matchCount - a.matchCount })
        grpResults.sort(function(a, b) { return b.matchCount - a.matchCount })

        conversationResults = convResults
        friendResults = frdResults
        groupResults = grpResults
    }

    // 弹窗属性
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0

    // 确保每次打开前刷新位置，避免首次打开时 item 坐标尚未稳定。
    // 由外部 Main.qml 通过绑定提供 updatePosition 函数。
    onAboutToShow: {
        if (typeof updatePosition === "function") {
            updatePosition()
        }
    }

    // 背景
    background: Rectangle {
        color: theme.chatListBackground
        border.color: theme.cardBorder
        border.width: 1
        radius: 8
    }

    // 内容区域
    contentItem: ScrollView {
        id: scrollView
        implicitWidth: 280
        implicitHeight: Math.min(contentColumn.implicitHeight, 500)
        clip: true

        ColumnLayout {
            id: contentColumn
            width: scrollView.width
            spacing: 0

            // 会话组
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: conversationResults.length > 0 ? conversationSection.implicitHeight : 0
                visible: conversationResults.length > 0

                ColumnLayout {
                    id: conversationSection
                    anchors.fill: parent
                    spacing: 0

                    // 组标题
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: "transparent"

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("会话")
                            color: theme.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    // 会话列表
                    Repeater {
                        model: conversationResults
                        delegate: searchResultItem
                    }
                }
            }

            // 好友组
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: friendResults.length > 0 ? friendSection.implicitHeight : 0
                visible: friendResults.length > 0

                ColumnLayout {
                    id: friendSection
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: "transparent"

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("好友")
                            color: theme.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    Repeater {
                        model: friendResults
                        delegate: searchResultItem
                    }
                }
            }

            // 群聊组
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: groupResults.length > 0 ? groupSection.implicitHeight : 0
                visible: groupResults.length > 0

                ColumnLayout {
                    id: groupSection
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: "transparent"

                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("群聊")
                            color: theme.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }

                    Repeater {
                        model: groupResults
                        delegate: searchResultItem
                    }
                }
            }

            // 分隔线（在有结果时显示）
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                color: theme.separatorHorizontal
                visible: (conversationResults.length > 0 || friendResults.length > 0 || groupResults.length > 0)
            }

            // 添加好友选项
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: addFriendMouse.containsMouse ? theme.chatListItemHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 6
                        color: theme.primaryButton

                        Label {
                            anchors.centerIn: parent
                            text: "+"
                            color: "#ffffff"
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }

                    Label {
                        text: qsTr("添加好友: ") + root.searchText
                        color: theme.textPrimary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: addFriendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.addFriendClicked(root.searchText)
                        root.close()
                    }
                }
            }

            // 添加群聊选项
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: addGroupMouse.containsMouse ? theme.chatListItemHover : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 6
                        color: theme.primaryButton

                        Label {
                            anchors.centerIn: parent
                            text: "+"
                            color: "#ffffff"
                            font.pixelSize: 20
                            font.bold: true
                        }
                    }

                    Label {
                        text: qsTr("添加群聊: ") + root.searchText
                        color: theme.textPrimary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: addGroupMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.addGroupClicked(root.searchText)
                        root.close()
                    }
                }
            }
        }
    }

    // 搜索结果项组件
    Component {
        id: searchResultItem

        Rectangle {
            required property var modelData
            width: scrollView.width
            height: 52
            color: mouseArea.containsMouse ? root.theme.chatListItemHover : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // 头像
                Rectangle {
                    width: 36
                    height: 36
                    radius: 6
                    color: modelData.avatarColor || "#4fbf73"
                    clip: true

                    Text {
                        anchors.centerIn: parent
                        text: modelData.initials || (modelData.name ? modelData.name.substring(0, 1) : (modelData.title ? modelData.title.substring(0, 1) : "?"))
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.bold: true
                        visible: avatarImg.status !== Image.Ready
                    }

                    Image {
                        id: avatarImg
                        anchors.fill: parent
                        source: loginBackend.resolveAvatarUrl(modelData.avatarPath || "")
                        fillMode: Image.PreserveAspectCrop
                        visible: status === Image.Ready
                        asynchronous: true
                    }
                }

                // 名称和ID
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: root.highlightMatch(modelData.title || modelData.name || "", root.searchText.trim())
                        color: root.theme.textPrimary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        textFormat: Text.StyledText
                    }

                    Text {
                        visible: modelData.wechatId !== undefined
                        text: root.highlightMatch(modelData.wechatId || "", root.searchText.trim())
                        color: root.theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        textFormat: Text.StyledText
                    }
                }

                // 匹配数标记
                Label {
                    visible: modelData.matchCount > 1
                    text: modelData.matchCount
                    color: root.theme.textSecondary
                    font.pixelSize: 10
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    // 通过判断 modelData 的属性来确定类型并触发对应信号
                    if (modelData.conversationId !== undefined) {
                        if (modelData.title !== undefined) {
                            root.conversationClicked(modelData.conversationId)
                        } else {
                            root.groupClicked(modelData.conversationId)
                        }
                    } else if (modelData.userId !== undefined) {
                        root.friendClicked(modelData.userId)
                    }
                }
            }
        }
    }
}
