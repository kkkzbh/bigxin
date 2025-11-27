import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import WeChatClient as AppTheme

// 独立窗口“发起群聊”，不阻塞主窗口
Window {
    id: root
    width: 920
    height: 620
    visible: false
    
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    modality: Qt.NonModal

    readonly property var theme: AppTheme.Theme

    // 选择状态
    property var selectedIds: []

    // 是否正在向服务器发起建群请求（用于禁用“完成”按钮）。
    property bool creatingGroup: false

    // 过滤文本
    property string filterText: ""

    function resetAndOpen() {
        selectedIds = []
        filterText = ""
        if (friendsModel.count === 0) {
            loginBackend.requestFriendList()
        }
        show()
        requestActivate()
    }

    function close() {
        visible = false
    }

    function isSelected(uid) {
        return selectedIds.indexOf(uid) >= 0
    }

    function toggleSelect(uid) {
        var idx = selectedIds.indexOf(uid)
        var arr = selectedIds.slice()
        if (idx >= 0) {
            arr.splice(idx, 1)
        } else {
            arr.push(uid)
        }
        selectedIds = arr
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: theme.windowBackground
        border.color: theme.cardBorder

        // 顶部整体拖动
        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 标题栏区域
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 36

                RowLayout {
                    anchors.fill: parent

                    // 左上角标题已移除

                    Item { Layout.fillWidth: true }

                    Label {
                        text: qsTr("已选择 %1 个联系人").arg(selectedIds.length)
                        color: theme.textSecondary
                        font.pixelSize: 12
                    }

                    ToolButton {
                        id: closeBtn
                        hoverEnabled: true
                        background: Rectangle {
                            radius: 3
                            color: closeBtn.hovered ? theme.dangerRed : "transparent"
                        }
                        contentItem: Label {
                            text: "×"
                            color: closeBtn.hovered ? "#ffffff" : "#f5f5f5"
                            font.pixelSize: 18
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: root.close()
                    }
                }
            }

            RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // 左列：好友列表 + 搜索
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 420
                color: theme.chatListBackground
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
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
                                placeholderText: qsTr("搜索联系人")
                                color: theme.searchText
                                placeholderTextColor: theme.searchPlaceholder
                                font.pixelSize: 13
                                onTextChanged: filterText = text
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ListView {
                            id: friendList
                            width: parent.width
                            model: friendsModel
                            interactive: true
                            delegate: Rectangle {
                                id: row
                                width: ListView.view.width
                                height: visible ? 68 : 0
                                color: hovered
                                       ? theme.chatListItemHover
                                       : (root.isSelected(userId)
                                          ? theme.chatListItemSelected
                                          : "transparent")
                                radius: 6

                                property bool hovered: false

                                visible: filterText.length === 0
                                         || (name.toLowerCase().indexOf(filterText.toLowerCase()) >= 0)
                                opacity: visible ? 1 : 0

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: row.hovered = true
                                    onExited: row.hovered = false
                                    onClicked: root.toggleSelect(userId)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 10

                                    // 头像（与会话列表保持一致尺寸）
                                    Rectangle {
                                        id: avatarRect
                                        width: 42
                                        height: 42
                                        radius: 6
                                        color: avatarColor

                                        Text {
                                            anchors.centerIn: parent
                                            text: initials
                                            color: "#ffffff"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Label {
                                            text: name
                                            color: theme.textPrimary
                                            font.pixelSize: 14
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: wechatId
                                            color: theme.textMuted
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // 圆形勾选框移到右侧，避免拉大头像左侧间距
                                    Rectangle {
                                        width: 20
                                        height: 20
                                        radius: 10
                                        border.width: 2
                                        border.color: root.isSelected(userId) ? "#4fbf73" : theme.textSecondary
                                        color: "transparent"
                                        Layout.alignment: Qt.AlignVCenter

                                        Rectangle {
                                            visible: root.isSelected(userId)
                                            anchors.centerIn: parent
                                            width: 12
                                            height: 12
                                            radius: 6
                                            color: "#4fbf73"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 右列：已选联系人
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 420
                color: theme.chatAreaBackground
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        text: qsTr("已选择 %1 个联系人").arg(selectedIds.length)
                        color: theme.textSecondary
                        font.pixelSize: 12
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: selectedList
                            width: parent.width
                            model: selectedModel
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 54
                                color: "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 10

                                    // 头像
                                    Rectangle {
                                        width: 38
                                        height: 38
                                        radius: 6
                                        color: avatarColor

                                        Text {
                                            anchors.centerIn: parent
                                            text: initials
                                            color: "#ffffff"
                                            font.pixelSize: 14
                                            font.bold: true
                                        }
                                    }

                                    Label {
                                        text: name
                                        color: theme.textPrimary
                                        font.pixelSize: 14
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    ToolButton {
                                        text: "×"
                                        background: null
                                        onClicked: root.toggleSelect(userId)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            Item { Layout.fillWidth: true }

            Button {
                id: cancelBtn
                text: qsTr("取消")
                implicitWidth: 108
                implicitHeight: 36
                background: Rectangle {
                    radius: 6
                    color: theme.chatListItemHover
                }
                contentItem: Label {
                    text: cancelBtn.text
                    color: theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 14
                }
                onClicked: root.close()
            }

            Button {
                id: okBtn
                text: qsTr("完成")
                enabled: selectedIds.length >= 2 && !root.creatingGroup
                implicitWidth: 108
                implicitHeight: 36
                background: Rectangle {
                    radius: 6
                    color: enabled ? theme.primaryButton : theme.sendButtonDisabled
                }
                contentItem: Label {
                    text: okBtn.text
                    color: enabled ? "#ffffff" : theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 14
                    font.bold: true
                }
                onClicked: {
                    if (root.creatingGroup)
                        return
                    root.creatingGroup = true
                    loginBackend.createGroupConversation(selectedIds, "")
                }
            }
        }
        }
    }

    Dialog {
        id: successDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        dim: true
        closePolicy: Popup.NoAutoClose
        implicitWidth: 280
        implicitHeight: 160
        background: Rectangle {
            radius: 10
            color: theme.chatAreaBackground
            border.color: theme.cardBorder
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 18

            Label {
                text: qsTr("创建成功！")
                color: theme.textPrimary
                font.pixelSize: 16
                font.bold: true
            }

            Label {
                text: qsTr("新的群聊已创建，点击确认关闭窗口。")
                color: theme.textSecondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }

            Button {
                id: confirmBtn
                text: qsTr("确认")
                Layout.alignment: Qt.AlignRight
                implicitWidth: 96
                implicitHeight: 32
                background: Rectangle {
                    radius: 6
                    color: theme.primaryButton
                }
                contentItem: Label {
                    text: confirmBtn.text
                    color: "#ffffff"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 14
                    font.bold: true
                }
                onClicked: {
                    successDialog.close()
                    root.close()
                }
            }
        }
    }

    ListModel { id: friendsModel }
    ListModel { id: selectedModel }

    // 同步好友列表
    Connections {
        target: loginBackend
        function onFriendsReset(friends) {
            friendsModel.clear()
            for (var i = 0; i < friends.length; ++i) {
                var f = friends[i]
                // 使用正则去除首尾空格，确保兼容性
                var name = (f.displayName || "").replace(/\s+/g, " ").trim()
                var initials = name.length > 0 ? name.substring(0, 1) : "?"
                friendsModel.append({
                    userId: f.userId,
                    name: name,
                    wechatId: f.account,
                    initials: initials,
                    avatarColor: "#4f90f2"
                })
            }
        }
    }

    // 创建群聊成功
    Connections {
        target: loginBackend
        function onGroupCreated(conversationId, title) {
            root.creatingGroup = false
            successDialog.open()
            successDialog.forceActiveFocus()
        }
    }

    // 创建群聊失败时恢复按钮
    Connections {
        target: loginBackend
        function onErrorMessageChanged() {
            root.creatingGroup = false
        }
    }

    // 当选择列表变化时，同步右侧模型
    onSelectedIdsChanged: {
        selectedModel.clear()
        for (var i = 0; i < selectedIds.length; ++i) {
            var uid = selectedIds[i]
            // 在 friendsModel 中找到对应数据
            for (var j = 0; j < friendsModel.count; ++j) {
                var row = friendsModel.get(j)
                if (row.userId === uid) {
                    selectedModel.append(row)
                    break
                }
            }
        }
    }
}
