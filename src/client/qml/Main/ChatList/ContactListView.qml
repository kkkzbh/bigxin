import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WeChatClient as AppTheme

Rectangle {
    id: root
    color: "transparent"

    readonly property var theme: AppTheme.Theme

    property int currentIndex: -1
    property string currentContactId: ""
    property string currentContactName: ""
    property string currentContactWeChatId: ""
    property string currentContactSignature: ""
    property string currentRequestStatus: ""

    // Mock Models
    ListModel {
        id: newFriendsModel
        ListElement {
            type: "request"
            name: "谁看见我的快递了"
            avatarColor: "#a29bfe"
            wechatId: "kuaidi_123"
            signature: "我是谁看见我的快递了"
            status: "waiting"
            msg: "通过搜索微信号添加"
        }
        ListElement {
            type: "request"
            name: "腻腻"
            avatarColor: "#ffeaa7"
            wechatId: "nini_cat"
            signature: "我是群聊“河大校..."
            status: "added"
            msg: "我是群聊“河大校..."
        }
         ListElement {
            type: "request"
            name: "呆呆小琪。"
            avatarColor: "#fab1a0"
            wechatId: "daidai_77"
            signature: "我是群聊“河大校..."
            status: "added"
            msg: "我是群聊“河大校..."
        }
    }

    ListModel {
        id: contactsModel
        ListElement {
            type: "contact"
            name: "文件传输助手"
            avatarColor: "#4fbf73"
            wechatId: "filehelper"
            signature: ""
        }
        ListElement {
            type: "contact"
            name: "阿白"
            avatarColor: "#ff9f43"
            wechatId: "abai_123"
            signature: "Stay hungry, stay foolish."
        }
         ListElement {
            type: "contact"
            name: "BigXin"
            avatarColor: "#5f27cd"
            wechatId: "bigxin_official"
            signature: "Hello World"
        }
        ListElement {
            type: "contact"
            name: "G"
            avatarColor: "#ff9f43"
            wechatId: "g_official"
            signature: "我是G"
        }
        ListElement {
            type: "contact"
            name: "Sakuta"
            avatarColor: "#a29bfe"
            wechatId: "sakuta_123"
            signature: "我是25软工甲班2班..."
        }
        ListElement {
            type: "contact"
            name: "Ns"
            avatarColor: "#fab1a0"
            wechatId: "ns_official"
            signature: "我是25软件工程张..."
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 0

            // --- New Friends Group ---
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    spacing: 5
                    Text {
                        text: newFriendsList.visible ? "▼" : "▶"
                        color: "#7d7d7d"
                        font.pixelSize: 10
                    }
                    Text {
                        text: "新的朋友"
                        color: "#7d7d7d"
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: newFriendsList.visible = !newFriendsList.visible
                }
            }

            ListView {
                id: newFriendsList
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? contentHeight : 0
                visible: true
                interactive: false
                model: newFriendsModel
                delegate: contactDelegate
            }

            // --- Contacts Group ---
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    spacing: 5
                    Text {
                        text: contactsList.visible ? "▼" : "▶"
                        color: "#7d7d7d"
                        font.pixelSize: 10
                    }
                    Text {
                        text: "联系人"
                        color: "#7d7d7d"
                        font.pixelSize: 12
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: contactsList.visible = !contactsList.visible
                }
            }

            ListView {
                id: contactsList
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? contentHeight : 0
                visible: true
                interactive: false
                model: contactsModel
                delegate: contactDelegate
            }
        }
    }

    // Shared Delegate
    Component {
        id: contactDelegate
        Rectangle {
            id: delegateRoot
            width: ListView.view.width
            height: 60
            color: {
                var isSelected = (root.currentContactId === model.wechatId);
                return isSelected ? theme.chatListItemSelected : (hovered ? theme.chatListItemHover : "transparent");
            }

            property bool hovered: false

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: delegateRoot.hovered = true
                onExited: delegateRoot.hovered = false
                onClicked: {
                    root.currentContactId = model.wechatId
                    root.currentContactName = model.name
                    root.currentContactWeChatId = model.wechatId
                    root.currentContactSignature = model.signature || ""
                    root.currentRequestStatus = model.status || ""
                    root.currentIndex = index // Just for compatibility
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Rectangle {
                    width: 36
                    height: 36
                    radius: 4
                    color: model.avatarColor

                    Text {
                        anchors.centerIn: parent
                        text: model.name.substring(0, 1)
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: model.name
                        color: "#ffffff"
                        font.pixelSize: 14
                    }
                    Label {
                        visible: model.type === "request"
                        text: model.msg || ""
                        color: "#7d7d7d"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Status Tag for Requests
                Label {
                    visible: model.type === "request"
                    text: model.status === "waiting" ? "等待验证" : "已添加"
                    color: "#7d7d7d"
                    font.pixelSize: 12
                }
            }
        }
    }
}
