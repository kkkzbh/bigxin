import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: theme.bgList

    // 由 App.qml 透传主题
    property var theme
    property real scaleFactor: 1.0

    property string currentTitle: list.currentIndex >= 0 ? list.model[list.currentIndex].title : ""
    property var currentModel: list.currentIndex >= 0 ? list.model[list.currentIndex].messages : []

    // 将网络入站消息插入置顶会话“世界”，并触发界面刷新
    // 注意：必须定义在根对象上，供 App.qml 通过 id 调用。
    // TODO(gui-model): 迭代替换为 QML ListModel 或 C++ QAbstractListModel，
    // 以增量 append() 取代整数组重建（slice + 赋值），避免 O(N) 拷贝与 GC 压力。
    function appendNetworkMessage(sender, text, selfNick) { // UI-API: 入站消息注入（世界频道）
        if (!list.model || list.model.length === 0)
            return;
        var m = list.model.slice(); // 复制一份数组，触发 QML 变更检测
        var world = m[0];
        var msgs = world.messages ? world.messages.slice() : [];
        msgs.push({ id: Date.now(), sender: sender, content: text, ts: "", out: (sender === selfNick) });
        // 更新置顶项的 last/time/unread（未读只在未选中“世界”时递增）
        var unread = world.unread || 0;
        if (list.currentIndex !== 0) unread += 1;
        m[0] = { title: world.title, last: text, time: "", unread: unread, pinned: true, messages: msgs };
        list.model = m; // 新数组赋值：触发绑定刷新
        // TODO(gui-ux): 若当前选中“世界”，滚动到底部；否则在该项显示未读提示条。
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52 * scaleFactor // UI-KNOB: 顶部搜索栏高度
            color: theme.bgList
            radius: 0
            anchors.margins: 0
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12 * scaleFactor // UI-KNOB: 搜索栏内边距
                spacing: 8 * scaleFactor // UI-KNOB: 搜索栏元素间距
                Rectangle { Layout.preferredWidth: 28 * scaleFactor; Layout.preferredHeight: 28 * scaleFactor; radius: 6 * scaleFactor; color: theme.bgSearch
                    Text { anchors.centerIn: parent; text: "🔍"; color: theme.textSecondary }
                }
                Text { text: "搜索"; color: theme.textSecondary; verticalAlignment: Text.AlignVCenter; font.pixelSize: 14 * scaleFactor; Layout.fillWidth: true } // UI-KNOB: 搜索提示字号
                Rectangle { Layout.preferredWidth: 28 * scaleFactor; Layout.preferredHeight: 28 * scaleFactor; radius: 6 * scaleFactor; color: theme.bgSearch
                    Text { anchors.centerIn: parent; text: "+"; color: theme.textSecondary; font.pixelSize: 16 * scaleFactor } // UI-KNOB: 新建按钮字号
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            currentIndex: 0 // UX: 默认选中置顶会话“世界”，确保能看到入站消息
            model: [
                // 置顶会话：世界（全局公共群聊）
                { title: "世界", last: "大家好，欢迎加入世界频道", time: "12:30", unread: 0, pinned: true, messages: [] },
                { title: "m", last: "心烦只能说", time: "12:26", unread: 1, messages: [
                    { id: 1, sender: "她", content: "没招了", ts: "12:05", out: false },
                    { id: 2, sender: "她", content: "我现在特别害怕这个老师给我低分", ts: "12:05", out: false },
                    { id: 3, sender: "她", content: "这还是个必修课", ts: "12:26", out: false },
                    { id: 4, sender: "我", content: "那么问题来了？", ts: "12:26", out: true },
                    { id: 5, sender: "我", content: "张芳型老师 和 李基民型", ts: "12:26", out: true }
                ]},
                { title: "公众号", last: "张口就来", time: "12:20", unread: 0, messages: [] },
                { title: "服务号", last: "更新预告", time: "11:24", unread: 0, messages: [] }
            ]
            delegate: ConversationItem {
                required property var modelData
                title: modelData.title
                preview: modelData.last
                time: modelData.time
                unread: modelData.unread
                selected: ListView.isCurrentItem
                theme: root.theme
                scaleFactor: root.scaleFactor // UI-KNOB: 列表项整体缩放
                // 使用委托内置的 index，无需手动传递，避免自引用
            }
            ScrollBar.vertical: ScrollBar { }
        }
    }
}
