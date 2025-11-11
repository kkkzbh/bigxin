import QtQuick
import QtQuick.Controls

ListView {
    id: list
    clip: true
    spacing: 6 * chatScale // UI-KNOB: 消息项间距（随聊天缩放）
    model: []
    property var theme
    property real chatScale: 1.0
    // 对外可读：是否接近底部（供外层“新消息”浮条占位使用）
    property bool nearBottom: (list.contentY + list.height) >= (list.contentHeight - bottomTolerance)
    // 自动滚动策略
    // - 仅当当前已在底部时，入站消息自动滚到底
    // - 若是“自己发送”的消息（模型最后一条 out==true），无论是否在底部都滚到底
    // - 初次创建时滚到底
    // UI-KNOB: 判定“接近底部”的容差像素（随缩放）
    property real bottomTolerance: 24 * chatScale // UI-KNOB: 底部容差
    property int lastObservedCount: 0
    function isNearBottom() {
        return nearBottom;
    }
    function isLastOutgoing() {
        if (!list.model || list.count <= 0) return false;
        var m = list.model;
        var i = list.count - 1;
        try {
            return !!m[i].out;
        } catch(e) {
            return false;
        }
    }
    function scrollToBottom() { list.positionViewAtEnd(); }
    onCountChanged: {
        if (list.count > lastObservedCount) {
            if (isLastOutgoing() || isNearBottom()) {
                Qt.callLater(scrollToBottom)
            }
        }
        lastObservedCount = list.count;
    }
    Component.onCompleted: Qt.callLater(scrollToBottom)

    // TODO(gui-ux): 新消息到达且当前在底部时自动滚动到底；
    // 进入会话时恢复上次滚动位置；上滑到顶加载更早历史。

    delegate: Column {
        width: ListView.view.width
        spacing: 4
        property bool isOutgoing: (modelData && modelData.out) || false
        property string content: (modelData && modelData.content) || ""
        property string ts: (modelData && modelData.ts) || ""

        // 时间分隔
        Rectangle {
            width: parent.width
            height: 28 * chatScale // UI-KNOB: 时间分隔条高度
            color: "transparent"
            Text {
                anchors.centerIn: parent
                text: ts
                color: theme.textSecondary
                font.pixelSize: 12 * chatScale // UI-KNOB: 时间字号
            }
        }

        MessageBubble { outgoing: isOutgoing; text: content; theme: list.theme; scaleFactor: list.chatScale }
    }

    ScrollBar.vertical: ScrollBar { }
}
