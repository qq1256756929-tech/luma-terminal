import QtQuick
import Luma 1.0

Item {
    id: root

    property string icon: ""
    property string tooltip: ""
    property int size: 28
    property int iconSize: 15
    property bool active: false
    property color color: root.active ? Theme.accent : Theme.textSecondary
    signal clicked()

    width: size
    height: size
    opacity: enabled ? 1 : 0.35

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: root.active ? Qt.rgba(0.357, 0.608, 1, 0.14)
                           : (hover.hovered ? Theme.hoverTint : "transparent")
        border.width: root.active ? 1 : 0
        border.color: Qt.rgba(0.357, 0.608, 1, 0.20)
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Icon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        color: root.color
        opacity: root.enabled ? 1 : 0.4
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }

    Rectangle {
        id: tip
        visible: hover.hovered && root.tooltip.length > 0
        z: 1000
        x: Math.round((root.width - width) / 2)
        y: root.height + 7
        width: tipMetrics.advanceWidth + 14
        height: 24
        radius: 6
        color: Theme.popoverBg
        border.width: 1
        border.color: Theme.popoverBorder

        TextMetrics {
            id: tipMetrics
            font: tipText.font
            text: root.tooltip
        }
        Text {
            id: tipText
            anchors.centerIn: parent
            text: root.tooltip
            color: Theme.textPrimary
            font.family: Theme.uiFont
            font.pixelSize: 11
        }
    }
}
