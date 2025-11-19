import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import WeChatClient as AppTheme

// 独立顶层窗口形式的“添加好友”界面，不阻塞主窗口。
Window {
    id: root

    // ----- 顶层窗口属性（整体大小与系统行为） -----
    visible: false                         // 默认隐藏，通过外部控制显示
    width: 420                             // 窗口宽度：改这里整体变宽/变窄
    height: 700                            // 窗口高度：改这里整体变高/变矮
    minimumWidth: 360
    minimumHeight: 480

    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"                   // 背景透明，由内部 Rectangle 绘制
    modality: Qt.NonModal                  // 非模态：不会冻结主窗口

    readonly property var theme: AppTheme.Theme

    // 输入内容（搜索框文本）
    property alias query: searchField.text

    // idle / loading / notFound / friend / stranger
    property string relationState: "idle"

    // 搜索结果数据模型（后端接入时填充）
    property var searchResult: ({
        found: false,
        isFriend: false,
        avatar: "",
        displayName: "",
        account: "",
        region: "",
        note: "",
        mutualGroups: 0,
        source: ""
    })

    // 对外可调用的重置接口
    function resetState() {
        relationState = "idle"
        searchResult = ({
            found: false,
            isFriend: false,
            avatar: "",
            displayName: "",
            account: "",
            region: "",
            note: "",
            mutualGroups: 0,
            source: ""
        })
        query = ""
    }

    // 整个窗口的可视背景（圆角 + 主题色）
    Rectangle {
        anchors.fill: parent
        radius: 12                          // 外层窗口圆角
        color: theme.chatListBackground     // 与会话列表一致的背景色
        border.color: theme.cardBorder

        // 允许拖动整个窗口（按住任意空白位置拖动）
        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ===== 标题栏区域 =====
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40   // 标题栏高度
                color: "transparent"

                // 标题文字：水平居中，距顶部 20 像素
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 20    // 标题与窗口顶部的垂直距离
                    text: qsTr("添加好友")
                    color: theme.textPrimary
                    font.pixelSize: 20       // 标题字号
                    font.bold: true
                }

                // 关闭按钮：右上角
                ToolButton {
                    anchors.top: parent.top
                    anchors.topMargin: 16    // 关闭按钮与顶部垂直距离
                    anchors.right: parent.right
                    anchors.rightMargin: 8   // 关闭按钮与右边距
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 3
                        color: parent.hovered ? "#2a2c30" : "transparent"
                    }
                    contentItem: Label {
                        text: "×"
                        color: "#f5f5f5"
                        font.pixelSize: 18   // 关闭按钮文字大小
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.visible = false
                }
            }

            // ===== 主体区域：搜索行 + 结果 =====
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24      // 主体区域离外框的边距
                    anchors.topMargin: 24
                    anchors.bottomMargin: 24
                    spacing: 0

                    // 标题与搜索行之间的垂直间距（在主体内部）
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16   // 改这里：标题下到搜索条的距离
                    }

                    // ---- 搜索行 ----
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8                 // 搜索框与右侧按钮的水平间距

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40 // 搜索框高度
                            radius: 6                  // 搜索框圆角
                            color: theme.searchBoxBackground

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10  // 搜索框内部左 padding
                                anchors.rightMargin: 10 // 搜索框内部右 padding
                                spacing: 6              // 图标 / 输入框 / 清除按钮之间间距

                                Label {
                                    text: "\u2315"      // 放大镜符号
                                    color: theme.searchIcon
                                    font.pixelSize: 16 // 放大镜字号
                                }

                                TextField {
                                    id: searchField
                                    Layout.fillWidth: true
                                    background: null
                                    placeholderText: qsTr("账号")
                                    color: theme.searchText
                                    placeholderTextColor: theme.searchPlaceholder
                                    font.pixelSize: 13  // 搜索输入文字字号
                                    inputMethodHints: Qt.ImhNoPredictiveText
                                }

                                ToolButton {
                                    id: clearButton
                                    visible: searchField.text.length > 0
                                    hoverEnabled: true
                                    background: Rectangle {
                                        radius: 10       // 清除按钮背景圆角
                                        color: clearButton.hovered ? "#3a3b3f" : "transparent"
                                    }
                                    contentItem: Label {
                                        text: "×"
                                        color: theme.textSecondary
                                        font.pixelSize: 14 // 清除按钮“×”字号
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: {
                                        searchField.clear()
                                        root.relationState = "idle"
                                    }
                                }
                            }
                        }

                        Button {
                            id: searchButton
                            text: qsTr("搜索")
                            Layout.preferredWidth: 80 // 搜索按钮宽度
                            enabled: searchField.text.length > 0
                            background: Rectangle {
                                radius: 6              // 搜索按钮圆角
                                color: enabled ? theme.primaryButton
                                               : theme.sendButtonDisabled
                            }
                            contentItem: Label {
                                text: searchButton.text
                                color: "#ffffff"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 14     // 搜索按钮文字字号
                                font.bold: true
                            }

                            // 后续在此调用后端搜索接口
                        }
                    }

                    // 搜索行与结果区域之间的垂直间距
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32   // 改这里：搜索条到结果卡片之间距离
                    }

                    // ---- 结果区域：占据剩余空间 ----
                    Item {
                        id: contentArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // 搜索结果卡片：friend / stranger
                        Loader {
                            id: resultLoader
                            anchors.centerIn: parent    // 卡片在内容区域居中

                            sourceComponent: root.relationState === "friend"
                                             ? friendCard
                                             : (root.relationState === "stranger"
                                                ? strangerCard
                                                : null)
                        }

                        // 初始/加载/未找到 提示文字
                        Text {
                            anchors.centerIn: parent
                            visible: root.relationState !== "friend"
                                     && root.relationState !== "stranger"
                            horizontalAlignment: Text.AlignHCenter
                            color: theme.textSecondary

                            text: {
                                switch (root.relationState) {
                                case "loading":
                                    return qsTr("正在搜索…")
                                case "notFound":
                                    return qsTr("未找到该账号")
                                default:
                                    return qsTr("请输入账号进行搜索")
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== 已有好友卡片 =====
    Component {
        id: friendCard

        Rectangle {
            radius: 16
            color: theme.cardBackground
            border.color: theme.cardBorder
            implicitWidth: 380              // 卡片宽度
            implicitHeight: 480             // 卡片高度

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16         // 卡片内部边距
                spacing: 12                 // 各块之间垂直间距

                // 顶部基本信息
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Rectangle {
                        width: 80
                        height: 80
                        radius: 8
                        clip: true
                        color: "#333333"

                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            source: root.searchResult.avatar
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: root.searchResult.displayName
                                color: theme.textPrimary
                                font.pixelSize: 18  // 昵称字号
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Text {
                            text: qsTr("账号：") + root.searchResult.account
                            color: theme.textSecondary
                            font.pixelSize: 13
                        }

                        Text {
                            text: qsTr("地区：") + root.searchResult.region
                            color: theme.textSecondary
                            font.pixelSize: 13
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.separatorHorizontal
                }

                // 备注
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        text: qsTr("备注")
                        width: 60
                        color: theme.textSecondary
                        font.pixelSize: 13
                    }

                    TextField {
                        Layout.fillWidth: true
                        text: root.searchResult.note
                        placeholderText: qsTr("给 TA 备注一个名字")
                        color: theme.textPrimary
                        placeholderTextColor: theme.textMuted
                        background: null
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.separatorHorizontal
                }

                // 朋友圈占位
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        text: qsTr("朋友圈")
                        width: 60
                        color: theme.textSecondary
                        font.pixelSize: 13
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Repeater {
                            model: 3

                            Rectangle {
                                width: 60
                                height: 60
                                radius: 6
                                color: "#333333"
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.separatorHorizontal
                }

                // 共同群聊 / 来源
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    ColumnLayout {
                        width: 60
                        spacing: 8

                        Text {
                            text: qsTr("共同群聊")
                            color: theme.textSecondary
                            font.pixelSize: 13
                        }

                        Text {
                            text: qsTr("来源")
                            color: theme.textSecondary
                            font.pixelSize: 13
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: root.searchResult.mutualGroups + qsTr(" 个")
                            color: theme.textPrimary
                            font.pixelSize: 13
                        }

                        Text {
                            text: root.searchResult.source
                            color: theme.textPrimary
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }
                    }
                }

                // 底部操作栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    radius: 12
                    color: theme.panelBackground

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 12

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("发消息")
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("语音聊天")
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("视频聊天")
                        }
                    }
                }
            }
        }
    }

    // ===== 未添加好友卡片 =====
    Component {
        id: strangerCard

        Rectangle {
            radius: 16
            color: theme.cardBackground
            border.color: theme.cardBorder
            implicitWidth: 380
            implicitHeight: 260

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Rectangle {
                        width: 80
                        height: 80
                        radius: 8
                        clip: true
                        color: "#333333"

                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            source: root.searchResult.avatar
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: root.searchResult.displayName
                            color: theme.textPrimary
                            font.pixelSize: 18
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: qsTr("账号：") + root.searchResult.account
                            color: theme.textSecondary
                            font.pixelSize: 13
                        }

                        Text {
                            text: qsTr("未添加到通讯录")
                            color: theme.textMuted
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.separatorHorizontal
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 180
                    Layout.preferredHeight: 40
                    text: qsTr("添加到通讯录")

                    background: Rectangle {
                        radius: 6
                        color: theme.primaryButton
                    }

                    contentItem: Label {
                        text: qsTr("添加到通讯录")
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 14
                        font.bold: true
                    }

                    // 后续在此发起 addFriend 请求
                }
            }
        }
    }
}

