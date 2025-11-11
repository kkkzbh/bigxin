import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: theme.bgChat
    // 放大输入区与发送按钮
    property real scaleFactor: 1.0
    // 在布局中使用隐式高度，让父布局通过 Layout.* 控制最终高度
    implicitHeight: Math.max(theme.composerMinH * scaleFactor, textArea.implicitHeight + 16 * scaleFactor) // UI-KNOB: 输入区最小高度与内容高度
    border.color: "#232323"
    border.width: 1
    property var theme

    signal send(string text)

    // 顶部工具行 + 文本编辑容器，按钮悬浮在右下角（留空隙）
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8 // UI-KNOB: 输入区整体外边距
        spacing: 6 // UI-KNOB: 工具栏与编辑区的垂直间距

        // 工具行
        RowLayout {
            Layout.fillWidth: true
            spacing: 12 * scaleFactor // UI-KNOB: 工具图标间距
            Text { text: "😊"; color: theme.textSecondary; font.pixelSize: 16 * scaleFactor } // UI-KNOB: 工具图标字号
            Text { text: "📎"; color: theme.textSecondary; font.pixelSize: 16 * scaleFactor } // UI-KNOB: 工具图标字号
            Item { Layout.fillWidth: true }
        }

        // 文本编辑 + 发送按钮悬浮
        Item {
            id: editorBox
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 右下角按钮尺寸与边距（更接近微信）
            readonly property real brMargin: 12 * scaleFactor // UI-KNOB: 发送按钮距右下角边距
            readonly property real btnW: 108 * scaleFactor // UI-KNOB: 发送按钮宽度
            readonly property real btnH: 36 * scaleFactor  // UI-KNOB: 发送按钮高度

            TextArea {
                id: textArea
                anchors.fill: parent
                // 为右下角按钮留出内边距，避免遮挡文字
                leftPadding: 8 * scaleFactor // UI-KNOB: 编辑区左内边距
                rightPadding: editorBox.btnW + editorBox.brMargin * 2 // UI-KNOB: 为右下按钮预留的右内边距
                topPadding: 8 * scaleFactor // UI-KNOB: 编辑区上内边距
                bottomPadding: editorBox.btnH + editorBox.brMargin * 2 // UI-KNOB: 为右下按钮预留的下内边距
                color: theme.textPrimary
                placeholderText: "输入消息..."
                wrapMode: TextEdit.Wrap
                font.pixelSize: 14 * scaleFactor // UI-KNOB: 编辑区文字字号
                Keys.onReturnPressed: function(event) {
                    if (event.modifiers & Qt.ShiftModifier) { return; }
                    root.send(textArea.text)
                    textArea.clear()
                    event.accepted = true
                }
            }

            Button {
                id: sendBtn
                text: "发送(S)"
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: editorBox.brMargin // UI-KNOB: 发送按钮与右下角距离
                width: editorBox.btnW // UI-KNOB
                height: editorBox.btnH // UI-KNOB
                font.pixelSize: 14 * scaleFactor // UI-KNOB: 按钮文字字号
                onClicked: { root.send(textArea.text); textArea.clear(); }
            }
        }
    }
}
