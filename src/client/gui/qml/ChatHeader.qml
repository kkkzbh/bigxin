import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: theme.bgChat
    height: theme.headerH // UI-KNOB: 标题栏高度
    Layout.fillWidth: true

    property string title: ""
    // 由父级透传
    property var theme

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12 // UI-KNOB: 标题栏内边距
        spacing: 8 // UI-KNOB: 标题与右侧图标间距
        Text { text: title; color: theme.textPrimary; font.pixelSize: 24 } // UI-KNOB: 标题字号
        Item { Layout.fillWidth: true }
        Text { text: "⋯"; color: theme.textSecondary; font.pixelSize: 18 } // UI-KNOB: 右侧菜单图标字号
    }
    Rectangle { anchors.bottom: parent.bottom; height: 1; width: parent.width; color: "#232323" }
}
