import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Effects

import WeChatClient as AppTheme

Window {
    id: root
    width: 360
    height: 480
    minimumWidth: 320
    minimumHeight: 400
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    color: "transparent"

    readonly property var theme: AppTheme.Theme

    property string serverMsgId: ""
    property var reactions: ({})  // {LIKE: [{userId, displayName}, ...], DISLIKE: [...]}
    property int currentTab: 0  // 0: LIKE, 1: DISLIKE

    Rectangle {
        id: dialogBackground
        anchors.fill: parent
        color: theme.panelBackground
        radius: 12

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#40000000"
            shadowBlur: 0.5
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 2
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 标题栏
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: qsTr("消息反应")
                    color: theme.textPrimary
                    font.pixelSize: 16
                    font.bold: true
                }

                Item {
                    Layout.fillWidth: true
                }

                ToolButton {
                    implicitWidth: 32
                    implicitHeight: 32

                    contentItem: Text {
                        text: "×"
                        color: theme.textPrimary
                        font.pixelSize: 20
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.hovered ? theme.chatListItemSelected : "transparent"
                        radius: 4
                    }

                    onClicked: root.close()
                }
            }

            // Tab 栏
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: root.currentTab === 0 ? theme.chatListItemSelected : "transparent"
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "👍"
                            font.pixelSize: 18
                        }

                        Text {
                            text: getLikeCount()
                            color: theme.textPrimary
                            font.pixelSize: 14
                            font.bold: root.currentTab === 0
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = 0
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: root.currentTab === 1 ? theme.chatListItemSelected : "transparent"
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "👎"
                            font.pixelSize: 18
                        }

                        Text {
                            text: getDislikeCount()
                            color: theme.textPrimary
                            font.pixelSize: 14
                            font.bold: root.currentTab === 1
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.currentTab = 1
                    }
                }
            }

            // 用户列表
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: userListView
                    spacing: 8
                    model: root.currentTab === 0 ? getLikeUsers() : getDislikeUsers()

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 56
                        color: "transparent"
                        radius: 6

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12

                            // 头像
                            Rectangle {
                                width: 40
                                height: 40
                                radius: 6
                                color: "#4fbf73"

                                Text {
                                    anchors.centerIn: parent
                                    text: (modelData.displayName || "").slice(0, 1)
                                    color: "#ffffff"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                            }

                            // 昵称
                            Text {
                                Layout.fillWidth: true
                                text: modelData.displayName || modelData.userId
                                color: theme.textPrimary
                                font.pixelSize: 14
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: theme.separatorHorizontal
                        }
                    }
                }
            }

            // 空状态提示
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: (root.currentTab === 0 && getLikeCount() === 0) || (root.currentTab === 1 && getDislikeCount() === 0)

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.currentTab === 0 ? "👍" : "👎"
                        font.pixelSize: 48
                        opacity: 0.3
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("暂无反应")
                        color: theme.textSecondary
                        font.pixelSize: 14
                    }
                }
            }
        }
        // 移除阴影效果，避免 Qt5Compat.GraphicalEffects 依赖问题
    }

    function getLikeCount() {
        if (!root.reactions || !root.reactions.LIKE) return 0
        return root.reactions.LIKE.length || 0
    }

    function getDislikeCount() {
        if (!root.reactions || !root.reactions.DISLIKE) return 0
        return root.reactions.DISLIKE.length || 0
    }

    function getLikeUsers() {
        if (!root.reactions || !root.reactions.LIKE) return []
        return root.reactions.LIKE
    }

    function getDislikeUsers() {
        if (!root.reactions || !root.reactions.DISLIKE) return []
        return root.reactions.DISLIKE
    }
}
