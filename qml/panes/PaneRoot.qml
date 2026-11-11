import QtQuick

Item {
    id: root

    property var treeNode
    property var windowRoot
    readonly property bool isLeaf: treeNode !== null && treeNode !== undefined && treeNode.type === "leaf"
    readonly property bool isSplit: treeNode !== null && treeNode !== undefined && treeNode.type === "split"

    Loader {
        anchors.fill: parent
        active: root.isLeaf
        sourceComponent: leafComponent
    }

    Loader {
        anchors.fill: parent
        active: root.isSplit
        source: root.isSplit ? "SplitContainer.qml" : ""
        onLoaded: {
            item.windowRoot = root.windowRoot
            item.node = root.treeNode
        }
    }

    Component {
        id: leafComponent
        TerminalPane {
            windowRoot: root.windowRoot
            paneId: root.treeNode ? root.treeNode.pane : ""
        }
    }
}
