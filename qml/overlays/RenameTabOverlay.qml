import QtQuick
import QtQuick.Controls
import Luma 1.0
import "../components"

Item {
    id: root

    property var windowRoot

    visible: windowRoot ? windowRoot.renameOpen : false
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }

    function close() {
        if (windowRoot)
            windowRoot.renameOpen = false
        if (windowRoot && windowRoot.activeView)
            windowRoot.activeView.forceActiveFocus()
    }

    function accept() {
        if (windowRoot)
            windowRoot.renameTab(windowRoot.renameIndex, field.text)
    }

    Connections {
        target: root.windowRoot
        function onRenameOpenChanged() {
            if (!root.windowRoot.renameOpen)
                return
            field.text = root.windowRoot.renameText
            Qt.callLater(function() {
                field.forceActiveFocus()
                field.selectAll()
            })
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.dimOverlay

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 440
        height: 218
        radius: 14
        color: Theme.popoverBg
        border.width: 1
        border.color: Theme.popoverBorder

        MouseArea { anchors.fill: parent; onClicked: {} }

        Icon {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 22
            name: "terminal"
            size: 18
            color: Theme.accent
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 54
            anchors.top: parent.top
            anchors.topMargin: 19
            text: "重命名标签页"
            color: Theme.textPrimary
            font.family: Theme.uiFont
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 54
            anchors.top: parent.top
            anchors.topMargin: 40
            text: "留空则恢复自动标题"
            color: Theme.textTertiary
            font.family: Theme.uiFont
            font.pixelSize: 10
        }

        Rectangle {
            id: fieldBackground
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 72
            height: 38
            radius: 8
            color: Theme.softTint
            border.width: 1
            border.color: field.activeFocus ? Qt.rgba(0.357, 0.608, 1, 0.55)
                                            : Theme.subtleBorder

            TextField {
                id: field
                anchors.fill: parent
                anchors.leftMargin: 11
                anchors.rightMargin: 11
                padding: 0
                background: null
                color: Theme.textPrimary
                placeholderText: "标签页名称"
                placeholderTextColor: Theme.textTertiary
                selectionColor: Qt.rgba(0.357, 0.608, 1, 0.35)
                selectedTextColor: Theme.textPrimary
                font.family: Theme.uiFont
                font.pixelSize: 12
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter

                Keys.onReturnPressed: root.accept()
                Keys.onEnterPressed: root.accept()
                Keys.onEscapePressed: root.close()
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            spacing: 8

            Rectangle {
                width: 80
                height: 32
                radius: 8
                color: cancelHover.hovered ? Theme.hoverTint : "transparent"
                border.width: 1
                border.color: Theme.subtleBorder

                Text {
                    anchors.centerIn: parent
                    text: "取消"
                    color: Theme.textSecondary
                    font.family: Theme.uiFont
                    font.pixelSize: 11
                }
                HoverHandler { id: cancelHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }

            Rectangle {
                width: 92
                height: 32
                radius: 8
                color: Qt.rgba(0.357, 0.608, 1, 0.20)
                border.width: 1
                border.color: Qt.rgba(0.357, 0.608, 1, 0.38)

                Text {
                    anchors.centerIn: parent
                    text: "重命名"
                    color: Theme.accent
                    font.family: Theme.uiFont
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.accept()
                }
            }
        }
    }
}
