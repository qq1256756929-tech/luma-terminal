import QtQuick
import Luma 1.0
import "../components"

Item {
    id: root

    property string message: ""
    property bool shown: false

    width: toastText.implicitWidth + (toastIcon.visible ? 46 : 30)
    height: 34
    opacity: shown ? 1 : 0
    visible: opacity > 0.01
    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    function show(text) {
        root.message = text
        root.shown = true
        hideTimer.restart()
    }

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: Theme.popoverBg
        border.width: 1
        border.color: Theme.popoverBorder

        Row {
            anchors.centerIn: parent
            spacing: 8

            Icon {
                id: toastIcon
                visible: root.message.length > 0
                name: "check"
                size: 13
                color: Theme.success
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: toastText
                text: root.message
                color: Theme.textPrimary
                font.family: Theme.uiFont
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 1600
        onTriggered: root.shown = false
    }
}
