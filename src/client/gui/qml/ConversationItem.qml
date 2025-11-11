import QtQuick
import QtQuick.Controls

Item {
    id: root
    width: ListView.view ? ListView.view.width : 280
    height: 64 * scaleFactor // UI-KNOB: 列表项行高

    // 由列表透传主题与缩放
    property var theme
    property real scaleFactor: 1.0
    // 使用委托内置的 index 变量，无需额外透传，避免与自身同名造成自引用

    property alias title: titleText.text
    property alias preview: previewText.text
    property alias time: timeText.text
    property int unread: 0
    property bool hovered: false
    property bool selected: false

    Rectangle { anchors.fill: parent; color: selected ? Qt.darker(theme.bgList, 1.2) : (hovered ? Qt.darker(theme.bgList, 1.08) : theme.bgList) }

    Row {
        anchors.fill: parent
        anchors.margins: 12 * scaleFactor // UI-KNOB: 列表项内边距
        spacing: 12 * scaleFactor // UI-KNOB: 头像与文本区间距

        Rectangle { width: 40 * scaleFactor; height: 40 * scaleFactor; radius: 6 * scaleFactor; color: "#3A3B3F" // UI-KNOB: 头像尺寸/圆角
            Text { anchors.centerIn: parent; text: "👤"; color: "#DADADA"; font.pixelSize: 14 * scaleFactor }
        }
        Column { spacing: 4 * scaleFactor; width: parent.width - (40 + 12 + 60) * scaleFactor
            Text { id: titleText; color: theme.textPrimary; font.pixelSize: 15 * scaleFactor; elide: Text.ElideRight } // UI-KNOB: 标题字号
            Text { id: previewText; color: theme.textSecondary; font.pixelSize: 13 * scaleFactor; elide: Text.ElideRight } // UI-KNOB: 预览字号
        }
        Column { spacing: 4 * scaleFactor; width: 48 * scaleFactor
            Text { id: timeText; color: theme.textSecondary; font.pixelSize: 12 * scaleFactor; horizontalAlignment: Text.AlignRight } // UI-KNOB: 时间字号
            Rectangle { width: unread>0 ? 18 * scaleFactor : 0; height: 18 * scaleFactor; radius: 9 * scaleFactor; color: "#F04D43"; // UI-KNOB: 未读角标尺寸/颜色
                anchors.horizontalCenter: parent.horizontalCenter
                Text { anchors.centerIn: parent; text: unread; visible: unread>0; color: "white"; font.pixelSize: 11 * scaleFactor } // UI-KNOB: 未读数字字号
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: if (ListView.view) ListView.view.currentIndex = index
    }
}
