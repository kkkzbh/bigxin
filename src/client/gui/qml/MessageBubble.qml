import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    // 委托宽度：优先父级宽度，回落到 ListView 宽度
    width: parent ? parent.width : (ListView.view ? ListView.view.width : 600)
    // 修复BUG：此前引用了未定义的 contentItem，导致 delegate 高度恒为 40
    // 这里按文本高度/头像尺寸与外边距计算委托的隐式高度
    implicitHeight: Math.max(36 * scaleFactor, // UI-KNOB: 头像尺寸参与的最小高度
                             Math.max(contentItemL.implicitHeight, contentItemR.implicitHeight) + 12 * scaleFactor)
                    + 24 * scaleFactor // UI-KNOB: 行外边距合计（上下各 12）

    property bool outgoing: false
    property string text: ""
    property var theme
    property real scaleFactor: 1.0

    // 使用两个 RowLayout，根据方向切换，实现彻底的左右对齐
    RowLayout {
        id: leftRow
        visible: !root.outgoing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12 * scaleFactor // UI-KNOB: 单条消息外边距
        spacing: 12 * scaleFactor // UI-KNOB: 头像与气泡间距

        // 头像（左）
        Rectangle { Layout.alignment: Qt.AlignLeft; width: 36 * scaleFactor; height: 36 * scaleFactor; radius: 4 * scaleFactor; color: "#3A3B3F" // UI-KNOB: 头像大小/圆角/占位色
            Text { anchors.centerIn: parent; text: "她"; color: "#DADADA"; font.pixelSize: 12 * scaleFactor }
        }
        // 气泡
        Rectangle {
            id: bubbleL
            radius: theme.corner * scaleFactor // UI-KNOB: 气泡圆角
            color: theme.bubbleIn // UI-KNOB: 入站气泡颜色
            Layout.alignment: Qt.AlignLeft
            // 58% 的最大宽度：减去两侧外边距(24)与头像+间距(48)
            Layout.preferredWidth: Math.min(0.58 * (root.width - 72 * scaleFactor), contentItemL.implicitWidth + 16 * scaleFactor) // UI-KNOB: 0.58 为最大相对宽度
            Layout.preferredHeight: contentItemL.implicitHeight + 12 * scaleFactor
            Text {
                id: contentItemL
                anchors.fill: parent
                anchors.margins: 8 * scaleFactor // UI-KNOB: 气泡内边距
                wrapMode: Text.Wrap
                color: theme.textPrimary
                text: root.text
                font.pixelSize: 14 * scaleFactor // UI-KNOB: 文本字号
            }
        }
        Item { Layout.fillWidth: true }
    }

    RowLayout {
        id: rightRow
        visible: root.outgoing
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12 * scaleFactor // UI-KNOB: 单条消息外边距
        spacing: 12 * scaleFactor // UI-KNOB: 气泡与头像间距

        Item { Layout.fillWidth: true }
        // 气泡（靠右）
        Rectangle {
            id: bubbleR
            radius: theme.corner * scaleFactor // UI-KNOB: 气泡圆角
            color: theme.bubbleOut // UI-KNOB: 出站气泡颜色
            Layout.alignment: Qt.AlignRight
            Layout.preferredWidth: Math.min(0.58 * (root.width - 72 * scaleFactor), contentItemR.implicitWidth + 16 * scaleFactor) // UI-KNOB: 0.58 为最大相对宽度
            Layout.preferredHeight: contentItemR.implicitHeight + 12 * scaleFactor
            Text {
                id: contentItemR
                anchors.fill: parent
                anchors.margins: 8 * scaleFactor // UI-KNOB: 气泡内边距
                wrapMode: Text.Wrap
                color: theme.bubbleOutText // UI-KNOB: 出站气泡文字颜色
                text: root.text
                font.pixelSize: 14 * scaleFactor // UI-KNOB: 文本字号
            }
        }
        // 头像（右）
        Rectangle { Layout.alignment: Qt.AlignRight; width: 36 * scaleFactor; height: 36 * scaleFactor; radius: 4 * scaleFactor; color: "#3A3B3F" // UI-KNOB: 头像大小/圆角/占位色
            Text { anchors.centerIn: parent; text: "我"; color: "#DADADA"; font.pixelSize: 12 * scaleFactor }
        }
    }
}
