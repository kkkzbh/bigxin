import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../.." as AppTheme

Rectangle {
    id: root

    AppTheme.Theme {
        id: theme
    }

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
                    { "icon": "\uE608", "tooltip": qsTr("聊天") },
                    { "icon": "\uE603", "tooltip": qsTr("通讯录") },
                    { "icon": "\uE609", "tooltip": qsTr("收藏") },
                    { "icon": "\uE60A", "tooltip": qsTr("设置") }
                ]

                delegate: Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44

                    Rectangle {
                        id: btn
                        anchors.fill: parent
                        radius: 22
                        color: hovered ? theme.sidebarButtonHover : "transparent"

                        property bool hovered: false

                        Text {
                            anchors.centerIn: parent
                            text: modelData.icon
                            font.pixelSize: 22
                            color: "#f5f5f5"
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: btn.hovered = true
                            onExited: btn.hovered = false
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
                width: 28
                height: 28
                radius: 14
                color: "#202020"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "⋮"
                    color: "#f0f0f0"
                    font.pixelSize: 18
                }
            }
        }
    }
}
