import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import WeChatClient as AppTheme

// 独立顶层窗口形式的"添加群聊"界面，不阻塞主窗口。
Window {
    id: root

    visible: false
    width: 420
    height: 500
    minimumWidth: 360
    minimumHeight: 400

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    modality: Qt.NonModal

    readonly property var theme: AppTheme.Theme

    property alias query: searchField.text

    // idle / loading / notFound / found / member
    property string searchState: "idle"

    // 搜索结果
    property var searchResult: ({
        groupId: "",
        name: "",
        memberCount: 0
    })

    property string searchGroupId: ""
    property bool requestSent: false

    function resetState() {
        searchState = "idle"
        searchResult = ({
            groupId: "",
            name: "",
            memberCount: 0
        })
        searchGroupId = ""
        query = ""
        requestSent = false
    }

    // 窗口背景
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: theme.chatListBackground
        border.color: theme.cardBorder

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // 标题栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: "transparent"

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 20
                    text: qsTr("添加群聊")
                    color: theme.textPrimary
                    font.pixelSize: 20
                    font.bold: true
                }

                ToolButton {
                    anchors.top: parent.top
                    anchors.topMargin: 16
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 3
                        color: parent.hovered ? "#2a2c30" : "transparent"
                    }
                    contentItem: Label {
                        text: "×"
                        color: "#f5f5f5"
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.visible = false
                }
            }

            // 主体区域
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    anchors.topMargin: 24
                    anchors.bottomMargin: 24
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16
                    }

                    // 搜索行
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            radius: 6
                            color: theme.searchBoxBackground

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 6

                                Label {
                                    text: "\u2315"
                                    color: theme.searchIcon
                                    font.pixelSize: 16
                                }

                                TextField {
                                    id: searchField
                                    Layout.fillWidth: true
                                    background: null
                                    placeholderText: qsTr("群号")
                                    color: theme.searchText
                                    placeholderTextColor: theme.searchPlaceholder
                                    font.pixelSize: 13
                                    inputMethodHints: Qt.ImhDigitsOnly
                                }

                                ToolButton {
                                    id: clearButton
                                    visible: searchField.text.length > 0
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: 10
                                        color: clearButton.hovered ? "#3a3b3f" : "transparent"
                                    }
                                    contentItem: Label {
                                        text: "×"
                                        color: theme.textSecondary
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        searchField.clear()
                                        root.searchState = "idle"
                                    }
                                }
                            }
                        }

                        Button {
                            id: searchButton
                            text: qsTr("搜索")
                            Layout.preferredWidth: 80
                            enabled: searchField.text.length > 0
                            background: Rectangle {
                                radius: 6
                                color: enabled ? theme.primaryButton : theme.sendButtonDisabled
                            }
                            contentItem: Label {
                                text: searchButton.text
                                color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 14
                                font.bold: true
                            }
                            onClicked: {
                                root.searchState = "loading"
                                loginBackend.searchGroupById(root.query)
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                    }

                    // 结果区域
                    Item {
                        id: contentArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // 搜索结果卡片
                        Rectangle {
                            id: resultCard
                            anchors.centerIn: parent
                            width: 340
                            height: 180
                            radius: 16
                            color: theme.cardBackground
                            border.color: theme.cardBorder
                            visible: root.searchState === "found" || root.searchState === "member"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 16

                                // 群头像和名称
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Rectangle {
                                        width: 50
                                        height: 50
                                        radius: 8
                                        color: "#4fbf73"

                                        Label {
                                            anchors.centerIn: parent
                                            text: root.searchResult.name.substring(0, 1)
                                            color: "#ffffff"
                                            font.pixelSize: 22
                                            font.bold: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Label {
                                            text: root.searchResult.name + "(" + root.searchResult.memberCount + ")"
                                            color: theme.textPrimary
                                            font.pixelSize: 16
                                            font.bold: true
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Label {
                                            text: qsTr("群号: ") + root.searchResult.groupId
                                            color: theme.textSecondary
                                            font.pixelSize: 12
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                // 按钮
                                Button {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 40
                                    enabled: root.searchState === "found" && !root.requestSent
                                    text: {
                                        if (root.searchState === "member") return qsTr("已加入该群聊")
                                        if (root.requestSent) return qsTr("已发送申请")
                                        return qsTr("申请加入群聊")
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: parent.enabled ? theme.primaryButton : theme.sendButtonDisabled
                                    }
                                    contentItem: Label {
                                        text: parent.text
                                        color: "#ffffff"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 14
                                    }
                                    onClicked: {
                                        if (root.searchGroupId.length > 0) {
                                            loginBackend.sendGroupJoinRequest(root.searchGroupId, "")
                                        }
                                    }
                                }
                            }
                        }

                        // 提示文字
                        Text {
                            anchors.centerIn: parent
                            visible: root.searchState !== "found" && root.searchState !== "member"
                            horizontalAlignment: Text.AlignHCenter
                            color: theme.textSecondary
                            text: {
                                switch (root.searchState) {
                                case "loading":
                                    return qsTr("正在搜索…")
                                case "notFound":
                                    return qsTr("未找到该群聊")
                                default:
                                    return qsTr("请输入群号进行搜索")
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 群聊搜索结果
    Connections {
        target: loginBackend

        function onGroupSearchFinished(result) {
            if (!root.visible) return

            root.requestSent = false

            if (!result.ok) {
                root.searchState = "notFound"
                root.searchGroupId = ""
                root.searchResult = ({
                    groupId: "",
                    name: "",
                    memberCount: 0
                })
                return
            }

            root.searchGroupId = result.groupId || ""
            root.searchResult = ({
                groupId: result.groupId || "",
                name: result.name || "",
                memberCount: result.memberCount || 0
            })

            if (result.isMember) {
                root.searchState = "member"
            } else {
                root.searchState = "found"
            }
        }

        function onGroupJoinRequestSucceeded() {
            if (!root.visible) return
            root.requestSent = true
            joinSuccessDialog.open()
        }
    }

    // 申请成功弹窗
    Popup {
        id: joinSuccessDialog
        parent: root.contentItem
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 260
        background: Rectangle {
            radius: 8
            color: theme.cardBackground
            border.color: theme.cardBorder
            border.width: 1
        }

        Overlay.modal: Rectangle { color: "#80000000" }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                text: qsTr("已发送入群申请")
                color: theme.textPrimary
                font.pixelSize: 16
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: qsTr("群主或管理员通过验证后，你将自动加入该群聊。")
                color: theme.textSecondary
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: 8 }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("知道了")
                    background: Rectangle {
                        radius: 4
                        color: theme.primaryButton
                    }
                    contentItem: Label {
                        text: parent.text
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                    }
                    onClicked: joinSuccessDialog.close()
                }
            }
        }
    }
}
