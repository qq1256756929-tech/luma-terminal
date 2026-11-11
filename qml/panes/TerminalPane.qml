import QtQuick
import QtQuick.Controls
import Luma 1.0
import "../components"

Rectangle {
    id: pane

    property var windowRoot
    property string paneId: ""
    property var session: Terminal.session(paneId)
    property bool isActive: windowRoot ? windowRoot.activePaneId === paneId : false

    function shortCwd(path) {
        if (!path)
            return ""
        if (homeDir && (path === homeDir || path.indexOf(homeDir + "/") === 0))
            return "~" + path.substring(homeDir.length)
        return path
    }

    function baseName(path) {
        if (!path)
            return ""
        const clean = path.endsWith("/") ? path.slice(0, -1) : path
        const index = clean.lastIndexOf("/")
        return index >= 0 ? clean.substring(index + 1) : clean
    }

    property string paneTitle: {
        if (windowRoot && windowRoot.paneTitles && windowRoot.paneTitles[paneId])
            return windowRoot.paneTitles[paneId]
        if (!session)
            return "终端"
        const title = session.title || ""
        if (title.length > 0 && title.length <= 32
            && !(title.indexOf("@") >= 0 && title.indexOf(":") >= 0))
            return title
        if (session.cwd === homeDir)
            return "~"
        return baseName(session.cwd) || session.shellName || "终端"
    }

    radius: 12
    color: "transparent"
    border.width: 1
    border.color: isActive ? Qt.rgba(0.357, 0.608, 1, 0.52) : Theme.paneBorder
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.paneTop }
        GradientStop { position: 1.0; color: Theme.paneBottom }
    }

    Behavior on border.color { ColorAnimation { duration: 160 } }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 1
        radius: 1
        color: Theme.paneHighlight
        opacity: pane.isActive ? 1 : 0.5
    }

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 31

        Rectangle {
            anchors.fill: parent
            radius: 11
            color: headerHover.hovered ? Theme.faintTint : "transparent"
        }

        Rectangle {
            width: 6
            height: 6
            radius: 3
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            color: pane.isActive ? Theme.accent : Theme.textTertiary
            opacity: pane.isActive ? 1 : 0.55
        }

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.leftMargin: 27
            anchors.right: cwdText.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: pane.paneTitle
            color: pane.isActive ? Theme.textSecondary : Theme.textTertiary
            font.family: Theme.uiFont
            font.pixelSize: 11
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Text {
            id: cwdText
            anchors.right: headerActions.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Math.max(0, parent.width * 0.34))
            text: pane.shortCwd(pane.session ? pane.session.cwd : "")
            color: Theme.textTertiary
            font.family: Theme.uiFont
            font.pixelSize: 10
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignRight
            visible: text.length > 0 && parent.width > 280
        }

        Row {
            id: headerActions
            anchors.right: parent.right
            anchors.rightMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            opacity: headerHover.hovered || pane.isActive ? 1 : 0
            enabled: headerHover.hovered || pane.isActive
            Behavior on opacity { NumberAnimation { duration: 120 } }

            IconButton {
                icon: "split-right"
                tooltip: "向右分屏"
                size: 22
                iconSize: 12
                color: Theme.textTertiary
                onClicked: pane.windowRoot.splitPane(pane.paneId, "row")
            }
            IconButton {
                icon: "split-down"
                tooltip: "向下分屏"
                size: 22
                iconSize: 12
                color: Theme.textTertiary
                onClicked: pane.windowRoot.splitPane(pane.paneId, "column")
            }
            IconButton {
                icon: "close"
                tooltip: "关闭窗格"
                size: 22
                iconSize: 11
                color: Theme.textTertiary
                onClicked: pane.windowRoot.closePane(pane.paneId)
            }
        }

        HoverHandler { id: headerHover }

        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 78
            cursorShape: Qt.PointingHandCursor
            onClicked: pane.windowRoot.focusPane(pane.paneId)
        }
    }

    TerminalView {
        id: terminal
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        session: pane.session
        fontSize: pane.windowRoot ? pane.windowRoot.terminalFontSize : 13
        active: pane.isActive

        onActivated: pane.windowRoot.focusPane(pane.paneId)
        onBell: bellFlash.restart()
        onLinkActivated: pane.windowRoot.showToast("已打开链接")

        onContextMenuRequested: function(x, y) {
            contextMenu.x = Math.min(x, pane.width - contextMenu.width - 8)
            contextMenu.y = Math.min(header.height + y, pane.height - contextMenu.height - 8)
            contextMenu.open()
        }
    }

    Rectangle {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Theme.inactiveOverlay
        opacity: pane.isActive ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: 180 } }
    }

    Rectangle {
        id: exitedOverlay
        anchors.fill: parent
        anchors.margins: 1
        radius: 11
        color: Theme.exitedOverlay
        visible: pane.session && !pane.session.running
        z: 20

        MouseArea { anchors.fill: parent }

        Column {
            anchors.centerIn: parent
            spacing: 10

            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: "terminal"
                size: 30
                color: Theme.textTertiary
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "进程已退出"
                color: Theme.textPrimary
                font.family: Theme.uiFont
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: pane.session ? "退出码 " + pane.session.exitCode : ""
                color: Theme.textTertiary
                font.family: Theme.monoFont
                font.pixelSize: 10
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Rectangle {
                    width: 92
                    height: 30
                    radius: 8
                    color: Qt.rgba(0.357, 0.608, 1, 0.18)
                    border.width: 1
                    border.color: Qt.rgba(0.357, 0.608, 1, 0.34)
                    Text {
                        anchors.centerIn: parent
                        text: "重新启动"
                        color: Theme.accent
                        font.family: Theme.uiFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: pane.session.restart()
                    }
                }

                Rectangle {
                    width: 92
                    height: 30
                    radius: 8
                    color: Theme.softTint
                    border.width: 1
                    border.color: Theme.subtleBorder
                    Text {
                        anchors.centerIn: parent
                        text: "关闭窗格"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: pane.windowRoot.closePane(pane.paneId)
                    }
                }
            }
        }
    }

    Rectangle {
        id: bellFlashRect
        anchors.fill: parent
        anchors.margins: 1
        radius: 11
        color: "transparent"
        border.width: 1.5
        border.color: Theme.warning
        opacity: 0
        z: 30

        SequentialAnimation {
            id: bellFlash
            NumberAnimation { target: bellFlashRect; property: "opacity"; to: 0.8; duration: 80 }
            NumberAnimation { target: bellFlashRect; property: "opacity"; to: 0; duration: 320 }
        }
    }

    Menu {
        id: contextMenu
        width: 206
        padding: 4
        palette.window: Theme.popoverBg
        palette.windowText: Theme.textPrimary
        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.subtleBorder
        palette.highlightedText: Theme.textPrimary
        palette.mid: Theme.popoverBorder
        background: Rectangle {
            implicitWidth: 206
            color: Theme.popoverBg
            radius: 10
            border.width: 1
            border.color: Theme.popoverBorder
        }
        MenuItem {
            text: "复制"
            enabled: terminal.hasSelection
            implicitHeight: 32
            onClicked: { terminal.copySelection(); pane.windowRoot.showToast("已复制选中内容") }
        }
        MenuItem {
            text: "粘贴"
            implicitHeight: 32
            onClicked: terminal.pasteClipboard()
        }
        MenuItem {
            text: "全选"
            implicitHeight: 32
            onClicked: terminal.selectAll()
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.subtleBorder } }
        MenuItem {
            text: "清空选择"
            enabled: terminal.hasSelection
            implicitHeight: 32
            onClicked: terminal.clearSelection()
        }
    }

    Component.onCompleted: {
        if (pane.windowRoot)
            pane.windowRoot.registerPaneView(pane.paneId, terminal)
        if (pane.isActive)
            Qt.callLater(function() { terminal.forceActiveFocus() })
    }

    Component.onDestruction: {
        if (pane.windowRoot)
            pane.windowRoot.unregisterPaneView(pane.paneId)
    }

    onIsActiveChanged: {
        if (isActive)
            Qt.callLater(function() { terminal.forceActiveFocus() })
    }
}
