import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: theme.bgNav

    // 由上层 App.qml 透传
    property var theme
    property real scaleFactor: 1.0
    property url iconSource: ""

    Column {
        anchors.fill: parent
        spacing: 16 * scaleFactor // UI-KNOB: 导航项垂直间距
        padding: 12 * scaleFactor // UI-KNOB: 导航栏内边距

        // 顶部品牌图标区
        Rectangle {
            width: parent.width - 4 * scaleFactor
            height: 52 * scaleFactor // UI-KNOB: 顶部 logo 容器高度
            radius: 12 * scaleFactor // UI-KNOB: 顶部 logo 容器圆角
            color: Qt.darker(root.color, 1.05)
            anchors.horizontalCenter: parent.horizontalCenter
            clip: true
            visible: iconSource !== ""
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: iconSource
            }
        }

        Repeater {
            model: ["💬", "👥", "⭐", "🗂", "⚙"]
            delegate: Rectangle {
                width: parent.width - 24 * scaleFactor // UI-KNOB: 导航项宽度（相对父宽）
                height: 48 * scaleFactor // UI-KNOB: 导航项高度
                radius: 8 * scaleFactor // UI-KNOB: 导航项圆角
                color: hovered ? Qt.darker(root.color, 1.1) : "transparent"
                property bool hovered: false
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: "#D0D0D0"
                    font.pixelSize: 20 * scaleFactor // UI-KNOB: 导航图标字号
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: parent.hovered = true
                    onExited: parent.hovered = false
                }
            }
        }
        Rectangle { height: 1; width: parent.width; color: Qt.darker(root.color, 1.2) }
        Rectangle { width: parent.width - 24 * scaleFactor; height: 48 * scaleFactor; radius: 8 * scaleFactor; color: "transparent" // UI-KNOB: 底部“更多”按钮区域尺寸
            Text { anchors.centerIn: parent; text: "⬇"; color: "#808080" }
        }
    }
}
