import QtQuick
import QtQuick.Controls
import Luma 1.0

Item {
    id: root

    property var windowRoot
    property var tab
    property int tabIndex: 0
    property bool isActive: false

    signal activated()
    signal closeRequested()
    signal renameRequested()
    signal moveUpRequested()
    signal moveDownRequested()

    height: 48

    Rectangle {
        id: background
        anchors.fill: parent
        radius: 9
        color: root.isActive ? Theme.hoverTint
                             : (hover.hovered ? Theme.softTint : "transparent")
        border.width: root.isActive ? 1 : 0
        border.color: Theme.hoverTint
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Rectangle {
        width: 2.5
        height: 18
        radius: 1.5
        x: 8
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.accent
        opacity: root.isActive ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }
    }

    Rectangle {
        id: iconBadge
        width: 26
        height: 26
        radius: 7
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        color: root.isActive ? Qt.rgba(0.357, 0.608, 1, 0.16) : Theme.softTint
        border.width: 1
        border.color: root.isActive ? Qt.rgba(0.357, 0.608, 1, 0.22) : Theme.softTint

        Icon {
            anchors.centerIn: parent
            name: "terminal"
            size: 14
            color: root.isActive ? Theme.accent : Theme.textSecondary
        }
    }

    Column {
        anchors.left: iconBadge.right
        anchors.leftMargin: 10
        anchors.right: countBadge.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            width: parent.width
            text: root.tab && root.tab.displayTitle ? root.tab.displayTitle : "终端"
            color: root.isActive ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.uiFont
            font.pixelSize: 12
            font.weight: root.isActive ? Font.DemiBold : Font.Medium
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: root.tab && root.tab.subtitle ? root.tab.subtitle : (root.tab && root.tab.shellName ? root.tab.shellName : "")
            color: Theme.textTertiary
            font.family: Theme.uiFont
            font.pixelSize: 10
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: countBadge
        anchors.right: closeButton.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        visible: root.tab && root.tab.paneCount > 1
        width: visible ? countText.implicitWidth + 10 : 0
        height: 17
        radius: 5
        color: Theme.softTint

        Text {
            id: countText
            anchors.centerIn: parent
            text: root.tab ? root.tab.paneCount : ""
            color: Theme.textTertiary
            font.family: Theme.monoFont
            font.pixelSize: 9
        }
    }

    IconButton {
        id: closeButton
        anchors.right: parent.right
        anchors.rightMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        width: 21
        height: 21
        size: 21
        iconSize: 11
        icon: "close"
        tooltip: "关闭标签页"
        opacity: root.isActive || hover.hovered ? 1 : 0
        enabled: root.isActive || hover.hovered
        color: Theme.textTertiary
        onClicked: root.closeRequested()
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: 30
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                tabMenu.x = Math.min(mouse.x, root.width - tabMenu.width)
                tabMenu.y = mouse.y
                tabMenu.open()
            } else {
                root.activated()
            }
        }
    }

    Menu {
        id: tabMenu
        width: 190
        padding: 4
        palette.window: Theme.popoverBg
        palette.windowText: Theme.textPrimary
        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.hoverTint
        palette.highlightedText: Theme.textPrimary
        background: Rectangle {
            implicitWidth: 190
            color: Theme.popoverBg
            radius: 10
            border.width: 1
            border.color: Theme.popoverBorder
        }

        MenuItem {
            text: "重命名标签页"
            implicitHeight: 32
            onClicked: root.renameRequested()
        }
        MenuItem {
            text: "上移"
            enabled: root.tabIndex > 0
            implicitHeight: 32
            onClicked: root.moveUpRequested()
        }
        MenuItem {
            text: "下移"
            enabled: root.windowRoot && root.tabIndex < root.windowRoot.tabs.length - 1
            implicitHeight: 32
            onClicked: root.moveDownRequested()
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.subtleBorder } }
        MenuItem {
            text: "关闭标签页"
            implicitHeight: 32
            onClicked: root.closeRequested()
        }
    }
}
