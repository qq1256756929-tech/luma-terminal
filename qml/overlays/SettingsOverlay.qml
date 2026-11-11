import QtQuick
import QtQuick.Controls
import Luma 1.0
import "../components"

Item {
    id: root

    property var windowRoot
    property var shellModel: []

    visible: windowRoot ? windowRoot.settingsOpen : false
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    function close() {
        if (!windowRoot)
            return
        windowRoot.settingsOpen = false
        if (windowRoot.activeView)
            windowRoot.activeView.forceActiveFocus()
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
        width: Math.min(660, parent.width - 80)
        height: 64 + body.height + 44
        radius: 16
        color: Theme.popoverBg
        border.width: 1
        border.color: Theme.popoverBorder

        MouseArea { anchors.fill: parent; onClicked: {} }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 1
            height: 1
            radius: 1
            color: Theme.subtleBorder
        }

        Item {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64

            Icon {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                name: "settings"
                size: 21
                color: Theme.accent
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 56
                anchors.top: parent.top
                anchors.topMargin: 16
                text: "设置"
                color: Theme.textPrimary
                font.family: Theme.uiFont
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 56
                anchors.top: parent.top
                anchors.topMargin: 37
                text: "个性化你的终端与工作方式"
                color: Theme.textTertiary
                font.family: Theme.uiFont
                font.pixelSize: 10
            }

            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                icon: "close"
                tooltip: "关闭"
                size: 26
                iconSize: 12
                color: Theme.textTertiary
                onClicked: root.close()
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.subtleBorder
            }
        }

        Item {
            id: body
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: bodyColumn.implicitHeight + 32

            Column {
                id: bodyColumn
                anchors.top: parent.top
                anchors.topMargin: 16
                anchors.left: parent.left
                anchors.leftMargin: 26
                anchors.right: parent.right
                anchors.rightMargin: 26
                spacing: 15

                Text {
                    text: "外观"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }

                Item {
                    width: parent.width
                    height: 38

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "主题"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 12
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 220
                        height: 36
                        radius: 9
                        color: Theme.softTint
                        border.width: 1
                        border.color: Theme.subtleBorder

                        Row {
                            anchors.fill: parent
                            anchors.margins: 3
                            spacing: 4

                            Repeater {
                                model: [
                                    { mode: "dark", label: "深色", icon: "moon" },
                                    { mode: "light", label: "浅色", icon: "sun" }
                                ]
                                delegate: Rectangle {
                                    width: (parent.width - 4) / 2
                                    height: parent.height
                                    radius: 7
                                    color: Theme.mode === modelData.mode ? Theme.accentSoft : "transparent"
                                    border.width: Theme.mode === modelData.mode ? 1 : 0
                                    border.color: Qt.rgba(0.357, 0.608, 1, 0.35)

                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 6
                                        Icon {
                                            name: modelData.icon
                                            size: 13
                                            color: Theme.mode === modelData.mode
                                                   ? Theme.accent : Theme.textSecondary
                                        }
                                        Text {
                                            text: modelData.label
                                            color: Theme.mode === modelData.mode
                                                   ? Theme.accent : Theme.textSecondary
                                            font.family: Theme.uiFont
                                            font.pixelSize: 11
                                            font.weight: Theme.mode === modelData.mode
                                                         ? Font.DemiBold : Font.Normal
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Theme.mode = modelData.mode
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.faintTint
                }

                Text {
                    text: "终端"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }

                Item {
                    width: parent.width
                    height: 36

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "字体大小"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 12
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        IconButton {
                            icon: "zoom-out"
                            tooltip: "减小字号"
                            size: 28
                            iconSize: 13
                            color: Theme.textSecondary
                            enabled: root.windowRoot.terminalFontSize > 8
                            onClicked: root.windowRoot.changeFontSize(-1)
                        }

                        Rectangle {
                            width: 42
                            height: 28
                            radius: 7
                            color: Theme.softTint
                            border.width: 1
                            border.color: Theme.subtleBorder
                            Text {
                                anchors.centerIn: parent
                                text: root.windowRoot.terminalFontSize
                                color: Theme.textPrimary
                                font.family: Theme.monoFont
                                font.pixelSize: 11
                            }
                        }

                        IconButton {
                            icon: "zoom-in"
                            tooltip: "增大字号"
                            size: 28
                            iconSize: 13
                            color: Theme.textSecondary
                            enabled: root.windowRoot.terminalFontSize < 24
                            onClicked: root.windowRoot.changeFontSize(1)
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 36

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "新标签 Shell"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 12
                    }

                    Item {
                        id: shellPicker
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 238
                        height: 32

                        Rectangle {
                            id: shellButton
                            anchors.fill: parent
                            radius: 7
                            color: shellHover.hovered ? Theme.hoverTint : Theme.softTint
                            border.width: 1
                            border.color: Theme.subtleBorder

                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: chevron.left
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 7

                                Icon {
                                    name: "terminal"
                                    size: 13
                                    color: Theme.textSecondary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: Terminal.shellName
                                    color: Theme.textPrimary
                                    font.family: Theme.monoFont
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Icon {
                                id: chevron
                                anchors.right: parent.right
                                anchors.rightMargin: 9
                                anchors.verticalCenter: parent.verticalCenter
                                name: "chevron-down"
                                size: 11
                                color: Theme.textTertiary
                            }

                            HoverHandler { id: shellHover }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.shellModel = Terminal.availableShells
                                    shellPopup.open()
                                }
                            }
                        }

                        Popup {
                            id: shellPopup
                            parent: shellPicker
                            x: 0
                            y: shellPicker.height + 5
                            width: shellPicker.width
                            height: Math.min(root.shellModel.length * 32 + 10, 270)
                            padding: 5
                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                            background: Rectangle {
                                color: Theme.popoverBg
                                radius: 9
                                border.width: 1
                                border.color: Theme.popoverBorder
                            }

                            contentItem: Flickable {
                                clip: true
                                width: shellPopup.width - 10
                                height: shellPopup.height - 10
                                contentHeight: shellColumn.height
                                boundsBehavior: Flickable.StopAtBounds

                                Column {
                                    id: shellColumn
                                    width: shellPopup.width - 10
                                    spacing: 1

                                    Repeater {
                                        model: root.shellModel
                                        delegate: Rectangle {
                                            width: shellColumn.width
                                            height: 31
                                            radius: 6
                                            color: shellRowHover.hovered ? Theme.hoverTint : "transparent"

                                            Row {
                                                anchors.left: parent.left
                                                anchors.leftMargin: 9
                                                anchors.right: checkIcon.left
                                                anchors.verticalCenter: parent.verticalCenter
                                                spacing: 7
                                                Text {
                                                    text: modelData
                                                    color: Terminal.shellPath === modelData
                                                           ? Theme.accent : Theme.textPrimary
                                                    font.family: Theme.monoFont
                                                    font.pixelSize: 10
                                                    elide: Text.ElideMiddle
                                                }
                                            }

                                            Icon {
                                                id: checkIcon
                                                anchors.right: parent.right
                                                anchors.rightMargin: 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                name: "check"
                                                size: 12
                                                color: Theme.accent
                                                visible: Terminal.shellPath === modelData
                                            }

                                            HoverHandler { id: shellRowHover }
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    Terminal.shellPath = modelData
                                                    shellPopup.close()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 36

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "回看行数"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 12
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Repeater {
                            model: [2000, 8000, 20000, 50000]
                            delegate: Rectangle {
                                width: 62
                                height: 28
                                radius: 6
                                color: Terminal.scrollbackLines === modelData
                                       ? Theme.accentSoft : Theme.softTint
                                border.width: 1
                                border.color: Terminal.scrollbackLines === modelData
                                              ? Qt.rgba(0.357, 0.608, 1, 0.35)
                                              : Theme.subtleBorder

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData >= 1000
                                          ? (modelData / 1000) + "k" : modelData
                                    color: Terminal.scrollbackLines === modelData
                                           ? Theme.accent : Theme.textSecondary
                                    font.family: Theme.monoFont
                                    font.pixelSize: 9
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Terminal.scrollbackLines = modelData
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.faintTint
                }

                Text {
                    text: "关于"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }

                Item {
                    width: parent.width
                    height: 34

                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10
                        Image {
                            width: 30
                            height: 30
                            source: "../../assets/luma.png"
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1
                            Text {
                                text: "Luma Terminal " + appVersion
                                color: Theme.textPrimary
                                font.family: Theme.uiFont
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }
                            Text {
                                text: "VT 内核 libghostty-vt (Ghostty)  ·  UI Qt Quick"
                                color: Theme.textTertiary
                                font.family: Theme.uiFont
                                font.pixelSize: 9
                            }
                        }
                    }

                    Rectangle {
                        width: 112
                        height: 28
                        radius: 7
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        color: resetHover.hovered ? Theme.hoverTint : "transparent"
                        border.width: 1
                        border.color: Theme.subtleBorder

                        Text {
                            anchors.centerIn: parent
                            text: "恢复默认设置"
                            color: Theme.textSecondary
                            font.family: Theme.uiFont
                            font.pixelSize: 10
                        }
                        HoverHandler { id: resetHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                Theme.mode = "dark"
                                root.windowRoot.terminalFontSize = 13
                                Terminal.resetSettings()
                                root.windowRoot.showToast("已恢复默认设置")
                            }
                        }
                    }
                }
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 44

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.subtleBorder
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: "设置会自动保存"
                color: Theme.textTertiary
                font.family: Theme.uiFont
                font.pixelSize: 10
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: "Luma " + appVersion
                color: Theme.textTertiary
                font.family: Theme.monoFont
                font.pixelSize: 9
            }
        }
    }
}
