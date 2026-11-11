import QtQuick
import Luma 1.0

Item {
    id: container

    property var node
    property var windowRoot
    property bool sideBySide: node ? node.orientation === "row" : true
    property real ratio: node && node.ratio !== undefined ? node.ratio : 0.5
    readonly property real gap: 8

    function clampRatio() {
        const available = (sideBySide ? width : height) - gap
        if (available <= 40)
            return
        const minRatio = Math.min(0.45, 150 / available)
        ratio = Math.max(minRatio, Math.min(1 - minRatio, ratio))
        if (node)
            node.ratio = ratio
    }

    Component.onCompleted: clampRatio()
    onWidthChanged: clampRatio()
    onHeightChanged: clampRatio()
    onRatioChanged: if (node) node.ratio = ratio

    Item {
        id: first
        x: 0
        y: 0
        width: container.sideBySide
               ? Math.max(0, (container.width - container.gap) * container.ratio)
               : container.width
        height: container.sideBySide
                ? container.height
                : Math.max(0, (container.height - container.gap) * container.ratio)

        PaneRoot {
            anchors.fill: parent
            windowRoot: container.windowRoot
            treeNode: container.node ? container.node.first : null
        }
    }

    Item {
        id: second
        x: container.sideBySide ? first.width + container.gap : 0
        y: container.sideBySide ? 0 : first.height + container.gap
        width: container.sideBySide
               ? Math.max(0, container.width - first.width - container.gap)
               : container.width
        height: container.sideBySide
                ? container.height
                : Math.max(0, container.height - first.height - container.gap)

        PaneRoot {
            anchors.fill: parent
            windowRoot: container.windowRoot
            treeNode: container.node ? container.node.second : null
        }
    }

    Rectangle {
        id: dividerLine
        visible: divider.pressed || divider.containsMouse
        x: container.sideBySide ? first.width + container.gap / 2 - 1 : 0
        y: container.sideBySide ? 0 : first.height + container.gap / 2 - 1
        width: container.sideBySide ? 2 : container.width
        height: container.sideBySide ? container.height : 2
        radius: 1
        color: Theme.accent
        opacity: 0.65
    }

    MouseArea {
        id: divider
        x: container.sideBySide ? first.width - 1 : 0
        y: container.sideBySide ? 0 : first.height - 1
        width: container.sideBySide ? container.gap + 2 : container.width
        height: container.sideBySide ? container.height : container.gap + 2
        cursorShape: container.sideBySide ? Qt.SizeHorCursor : Qt.SizeVerCursor
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        onPositionChanged: {
            if (!pressed)
                return
            const p = mapToItem(container, mouse.x, mouse.y)
            const available = (container.sideBySide ? container.width : container.height)
                              - container.gap
            if (available <= 40)
                return
            const minRatio = Math.min(0.45, 150 / available)
            const raw = (container.sideBySide ? p.x : p.y) / available
            container.ratio = Math.max(minRatio, Math.min(1 - minRatio, raw))
        }
    }
}
