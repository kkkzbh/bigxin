import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import WeChatClient

ColumnLayout {
    id: root

    property string label: ""
    property int maxVal: 60
    property alias value: comboBox.editText

    onMaxValChanged: comboBox.populateOptions()

    spacing: 8

    Label {
        text: root.label
        color: Theme.textSecondary
        font.pixelSize: 12
        Layout.alignment: Qt.AlignHCenter
    }

    ComboBox {
        id: comboBox
        Layout.preferredWidth: 80
        Layout.preferredHeight: 36
        editable: true
        editText: "0"
        model: options
        // 使用可编辑下拉：既可手输也可选 0..maxVal
        delegate: ItemDelegate {
            width: comboBox.width
            text: modelData
            font.pixelSize: 14
            padding: 8
        }

        property var options: []

        onEditTextChanged: {
            var v = parseInt(editText)
            if (isNaN(v) || v < 0) v = 0
            if (v > root.maxVal) v = root.maxVal
            var newText = v.toString()
            if (newText !== editText)
                editText = newText
        }

        Component.onCompleted: populateOptions()

        function populateOptions() {
            var arr = []
            for (var i = 0; i <= root.maxVal; ++i) {
                arr.push(i.toString())
            }
            options = arr
            if (parseInt(editText) > root.maxVal) {
                editText = root.maxVal.toString()
            }
        }

        contentItem: TextField {
            text: comboBox.editText
            placeholderText: "0"
            color: Theme.textPrimary
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            validator: IntValidator { bottom: 0; top: root.maxVal }
            selectByMouse: true

            onTextChanged: {
                if (comboBox.editText !== text)
                    comboBox.editText = text
            }

            background: Rectangle {
                color: Theme.searchBoxBackground
                radius: 6
                border.color: activeFocus ? Theme.primaryButton : Theme.separatorHorizontal
                border.width: 1
            }
        }

        indicator: Rectangle {
            width: 12
            height: 8
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 10
            color: "transparent"
            border.color: "transparent"
            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = Theme.textSecondary
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(width, 0)
                    ctx.lineTo(width / 2, height)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        background: Rectangle {
            color: Theme.searchBoxBackground
            radius: 6
            border.color: comboBox.activeFocus ? Theme.primaryButton : Theme.separatorHorizontal
            border.width: 1
        }
    }
}
