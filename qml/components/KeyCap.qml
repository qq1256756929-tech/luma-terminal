import QtQuick
import Luma 1.0

Item {
    id: root

    property string label: ""
    property bool accent: false

    width: Math.max(20, keyMetrics.advanceWidth + 10)
    height: 20

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: root.accent ? Qt.rgba(0.357, 0.608, 1, 0.16) : Theme.softTint
        border.width: 1
        border.color: root.accent ? Qt.rgba(0.357, 0.608, 1, 0.28) : Theme.subtleBorder
    }

    TextMetrics {
        id: keyMetrics
        font: keyText.font
        text: root.label
    }

    Text {
        id: keyText
        anchors.centerIn: parent
        text: root.label
        color: root.accent ? Theme.accent : Theme.textSecondary
        font.family: Theme.monoFont
        font.pixelSize: 10
        font.weight: Font.Medium
    }
}
