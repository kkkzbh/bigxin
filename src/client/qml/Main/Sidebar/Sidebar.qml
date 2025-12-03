import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import WeChatClient as AppTheme

Rectangle {
    id: root

    // 全局主题单例
    readonly property var theme: AppTheme.Theme
    // 当前选中的侧边栏索引：0=聊天, 1=通讯录, 2=Remix, 3=终端
    property int currentIndex: 0
    signal tabClicked(int index, string key)

    color: theme.sidebarBackground
    implicitWidth: 72

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.bottomMargin: 8
        spacing: 4

        // 中间功能按钮：由上往下排布
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: [
                    {
                        "key": "chat",
                        "iconLine": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/chat-3-line.svg",
                        "iconFill": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/chat-3-fill.svg",
                        "tooltip": qsTr("聊天")
                    },
                    {
                        "key": "user",
                        "iconLine": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/user-line.svg",
                        "iconFill": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/user-fill.svg",
                        "tooltip": qsTr("通讯录")
                    },
                    {
                        "key": "remix",
                        "iconLine": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/remix-fill.svg",
                        "iconFill": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/remix-fill.svg",
                        "tooltip": qsTr("Remix")
                    },
                    {
                        "key": "terminal",
                        "iconLine": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/terminal-box-line.svg",
                        "iconFill": "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/terminal-box-fill.svg",
                        "tooltip": qsTr("终端")
                    }
                ]

                delegate: Item {
                    id: tabItem
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44

                    // hover 用作“焦点”视觉效果，类似 ChatList
                    property bool hovered: false
                    readonly property bool checked: index === root.currentIndex
                    readonly property string iconSource: {
                        if (modelData.key === "remix") {
                            return modelData.iconFill
                        }
                        return checked ? modelData.iconFill : modelData.iconLine
                    }

                    Rectangle {
                        id: btn
                        anchors.fill: parent
                        radius: 22
                        // 4 种状态：未选中 / hover / 选中 / 选中 + hover
                        color: checked
                               ? (hovered ? theme.chatListItemSelectedHover
                                          : theme.chatListItemSelected)
                               : (hovered ? theme.chatListItemHover
                                          : "transparent")

                        Image {
                            anchors.centerIn: parent
                            source: iconSource
                            width: 24
                            height: 24
                            fillMode: Image.PreserveAspectFit
                            // 未选中稍微暗一点，hover/选中更亮
                            opacity: checked ? 1.0 : (tabItem.hovered ? 0.9 : 0.7)
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: tabItem.hovered = true
                            onExited: tabItem.hovered = false
                            onClicked: {
                                // 不要直接设置 root.currentIndex = index，否则会打破从父组件传入的绑定
                                // 只发出信号，让父组件更新 window.currentTab，绑定会自动同步
                                root.tabClicked(index, modelData.key)
                            }
                        }
                    }
                }
            }
        }

        // 中部与底部之间的弹性空白，把底部按钮推到底
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // 底部设置 / 菜单区域
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 36

            Rectangle {
                id: settingsButton
                width: 32
                height: 32
                radius: 16
                color: "#202020"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/WeChatClient/client/qml/resource/sidebar/settings-line.svg"
                    width: 20
                    height: 20
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: settingsButton.color = "#252525"
                    onExited: settingsButton.color = "#202020"
                    onClicked: {
                        if (typeof settingsDialog !== "undefined" && settingsDialog) {
                            settingsDialog.displayName = loginBackend.displayName
                            settingsDialog.avatarText = loginBackend.displayName.length > 0
                                                       ? loginBackend.displayName[0]
                                                       : "A"
                            settingsDialog.visible = true
                            settingsDialog.raise()
                            settingsDialog.requestActivate()
                        }
                    }
                }
            }
        }
    }
}
