import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import WeChatClient

Window {
    id: root
    width: 400
    height: 260
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint
    title: qsTr("设置禁言时长")
    color: Theme.panelBackground
    
    property string conversationId: ""
    property string targetUserId: ""
    
    // 信号：确认禁言
    signal muteConfirmed(string conversationId, string userId, int duration)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 24
        
        RowLayout {
            spacing: 12
            Layout.alignment: Qt.AlignHCenter

            // 月
            MySpinBox {
                id: monthInput
                label: qsTr("月")
                maxVal: 12
            }

            // 天
            MySpinBox {
                id: dayInput
                label: qsTr("天")
                maxVal: 30
            }
            
            // 分
            MySpinBox {
                id: minuteInput
                label: qsTr("分")
                maxVal: 60
            }

            // 秒
            MySpinBox {
                id: secondInput
                label: qsTr("秒")
                maxVal: 60
            }
        }
        
        Label {
            id: warningLabel
            text: qsTr("时长需大于 0")
            color: Theme.dangerRed
            font.pixelSize: 12
            visible: false
            Layout.alignment: Qt.AlignHCenter
        }

        Item {
            Layout.fillHeight: true
        }
        
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 12

            Button {
                text: qsTr("取消")
                implicitWidth: 80
                implicitHeight: 32
                background: Rectangle {
                    color: parent.down ? Theme.chatListItemSelected : "transparent"
                    radius: 4
                    border.color: Theme.separatorHorizontal
                    border.width: 1
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.textSecondary
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                text: qsTr("确定")
                implicitWidth: 80
                implicitHeight: 32
                background: Rectangle {
                    color: Theme.primaryButton
                    radius: 4
                }
                contentItem: Text {
                    text: parent.text
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    var m = parseInt(monthInput.value) || 0
                    var d = parseInt(dayInput.value) || 0
                    var min = parseInt(minuteInput.value) || 0
                    var s = parseInt(secondInput.value) || 0
                    
                    var duration = m * 30 * 24 * 3600 + d * 24 * 3600 + min * 60 + s
                    if (duration <= 0) {
                        warningLabel.visible = true
                    } else {
                        warningLabel.visible = false
                        root.muteConfirmed(root.conversationId, root.targetUserId, duration)
                        root.close()
                    }
                }
            }
        }
    }

}
