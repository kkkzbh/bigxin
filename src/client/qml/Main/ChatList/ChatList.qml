import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../.." as AppTheme

Rectangle {
    id: root
    // 恢复原先的会话栏背景色，偏深灰

    AppTheme.Theme {
        id: theme
    }

    color: theme.chatListBackground

    // 导出当前选中下标，便于标题栏 / 对话区判断是否有选中会话
    property alias currentIndex: listView.currentIndex

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

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
                       ? (hovered ? theme.chatListItemSelectedHover : theme.chatListItemSelected)
                       : (hovered ? theme.chatListItemHover : theme.chatListItemNormal)

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

                        Text {
                            anchors.centerIn: parent
                            text: initials
                            color: "#ffffff"
                            font.pixelSize: 18
                            font.bold: true
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

    }

    ListModel {
        id: chatModel

        ListElement {
            title: "吴呐小琪。"
            preview: "还行 仍需努力，需要奔着满分"
            time: "14:16"
            unreadCount: 1
            initials: "吴"
            avatarColor: "#f2c24f"
        }
        ListElement {
            title: "美团沙河..."
            preview: "团购领券福利自..."
            time: "13:29"
            unreadCount: 2
            initials: "团"
            avatarColor: "#f8674f"
        }
        ListElement {
            title: "服务号"
            preview: "小姐姐辣辣拌麻辣烫"
            time: "13:17"
            unreadCount: 0
            initials: "服"
            avatarColor: "#4f90f2"
        }
        ListElement {
            title: "江南无所有"
            preview: "[动画表情] 加油"
            time: "13:00"
            unreadCount: 0
            initials: "江"
            avatarColor: "#4fbf73"
        }
        ListElement {
            title: "默默"
            preview: "[图片]"
            time: "11:09"
            unreadCount: 0
            initials: "默"
            avatarColor: "#9b75f2"
        }
    }
}
