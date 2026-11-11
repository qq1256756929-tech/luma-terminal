import QtQuick
import Luma 1.0

Row {
    id: root

    property var windowRoot
    property bool windowActive: (windowRoot ? windowRoot.active : true) || screenshotMode
    spacing: 8

    Repeater {
        model: [
            { base: "#ff5f57", hover: "#ff7b72", glyph: "×", action: "close", tip: "关闭" },
            { base: "#febc2e", hover: "#ffd34d", glyph: "−", action: "minimize", tip: "最小化" },
            { base: "#28c840", hover: "#4ee06a", glyph: "＋", action: "zoom", tip: "缩放" }
        ]

        delegate: Item {
            id: light
            width: 12
            height: 12

            Rectangle {
                anchors.fill: parent
                radius: 6
                color: root.windowActive ? modelData.base : Qt.rgba(0.45, 0.47, 0.52, 0.75)
                Behavior on color { ColorAnimation { duration: 160 } }
            }

            Text {
                anchors.centerIn: parent
                text: modelData.glyph
                color: Qt.rgba(0.20, 0.12, 0.10, 0.9)
                font.family: Theme.uiFont
                font.pixelSize: 9
                font.bold: true
                visible: hover.hovered && root.windowActive
            }

            HoverHandler { id: hover }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (modelData.action === "close")
                        root.windowRoot.close()
                    else if (modelData.action === "minimize")
                        root.windowRoot.showMinimized()
                    else
                        root.windowRoot.toggleMaximize()
                }
            }
        }
    }
}
