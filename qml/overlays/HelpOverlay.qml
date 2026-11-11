import QtQuick
import Luma 1.0
import "../components"

Item {
    id: root

    property var windowRoot

    visible: windowRoot ? windowRoot.helpOpen : false
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    readonly property var groups: [
        {
            title: "标签页与窗格",
            items: [
                { label: "新建标签页", keys: ["Ctrl", "Shift", "T"] },
                { label: "向右分屏", keys: ["Ctrl", "Shift", "D"] },
                { label: "向下分屏", keys: ["Ctrl", "Shift", "E"] },
                { label: "关闭窗格 / 标签页", keys: ["Ctrl", "Shift", "W"] },
                { label: "切换标签页", keys: ["Ctrl", "Tab"] },
                { label: "反向切换标签页", keys: ["Ctrl", "Shift", "Tab"] },
                { label: "聚焦相邻窗格", keys: ["Ctrl", "Alt", "←/→"] }
            ]
        },
        {
            title: "编辑与查找",
            items: [
                { label: "复制选中内容", keys: ["Ctrl", "Shift", "C"] },
                { label: "粘贴", keys: ["Ctrl", "Shift", "V"] },
                { label: "在终端中查找", keys: ["Ctrl", "Shift", "F"] },
                { label: "选择全部内容", keys: ["Ctrl", "Shift", "A"] },
                { label: "选择单词", keys: ["双击"] },
                { label: "选择整行", keys: ["三击"] }
            ]
        },
        {
            title: "界面与视图",
            items: [
                { label: "命令面板", keys: ["Ctrl", "Shift", "P"] },
                { label: "放大字号", keys: ["Ctrl", "Shift", "+"] },
                { label: "缩小字号", keys: ["Ctrl", "Shift", "−"] },
                { label: "重置字号", keys: ["Ctrl", "Shift", "0"] },
                { label: "设置", keys: ["Ctrl", ","] },
                { label: "切换深色 / 浅色", keys: ["设置"] },
                { label: "切换全屏", keys: ["F11"] },
                { label: "打开本帮助", keys: ["Ctrl", "Shift", "?"] },
                { label: "退出 Luma", keys: ["Ctrl", "Shift", "Q"] }
            ]
        },
        {
            title: "鼠标与终端",
            items: [
                { label: "拖动选择文本", keys: ["拖动"] },
                { label: "打开上下文菜单", keys: ["右键"] },
                { label: "打开超链接", keys: ["Ctrl", "单击"] },
                { label: "粘贴", keys: ["中键"] },
                { label: "滚动回看历史", keys: ["滚轮"] },
                { label: "快速滚动", keys: ["Shift", "滚轮"] },
                { label: "翻页", keys: ["Shift", "PgUp/PgDn"] },
                { label: "跳到开头 / 结尾", keys: ["Ctrl", "Shift", "Home/End"] }
            ]
        }
    ]

    function close() {
        if (!windowRoot)
            return
        windowRoot.helpOpen = false
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
        width: Math.min(760, parent.width - 80)
        height: Math.min(74 + columnContent.height + 50, parent.height - 32)
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
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64

            Icon {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                name: "keyboard"
                size: 21
                color: Theme.accent
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 56
                anchors.top: parent.top
                anchors.topMargin: 17
                text: "键盘快捷键"
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
                text: "像 macOS 终端一样工作，但更快、更安静。"
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

        Flickable {
            id: contentFlick
            anchors.top: parent.top
            anchors.topMargin: 82
            anchors.left: parent.left
            anchors.leftMargin: 26
            anchors.right: parent.right
            anchors.rightMargin: 26
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 50
            clip: true
            contentHeight: columnContent.height
            contentWidth: width
            boundsBehavior: Flickable.StopAtBounds

            Row {
            id: columnContent
            width: contentFlick.width
            spacing: 30

            Column {
                width: (columnContent.width - 30) / 2
                spacing: 18

                Repeater {
                    model: [root.groups[0], root.groups[2]]
                    delegate: Column {
                        width: parent.width
                        spacing: 5

                        Text {
                            text: modelData.title
                            color: Theme.textTertiary
                            font.family: Theme.uiFont
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.6
                            bottomPadding: 2
                        }

                        Repeater {
                            model: modelData.items
                            delegate: Item {
                                width: parent.width
                                height: 30

                                Text {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: Theme.textSecondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: 11
                                }

                                Row {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    Repeater {
                                        model: modelData.keys
                                        delegate: KeyCap {
                                            label: modelData
                                            accent: modelData === "Ctrl"
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 1
                                    color: Theme.faintTint
                                }
                            }
                        }
                    }
                }
            }

            Column {
                width: (columnContent.width - 30) / 2
                spacing: 18

                Repeater {
                    model: [root.groups[1], root.groups[3]]
                    delegate: Column {
                        width: parent.width
                        spacing: 5

                        Text {
                            text: modelData.title
                            color: Theme.textTertiary
                            font.family: Theme.uiFont
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.6
                            bottomPadding: 2
                        }

                        Repeater {
                            model: modelData.items
                            delegate: Item {
                                width: parent.width
                                height: 30

                                Text {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: Theme.textSecondary
                                    font.family: Theme.uiFont
                                    font.pixelSize: 11
                                }

                                Row {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    Repeater {
                                        model: modelData.keys
                                        delegate: KeyCap {
                                            label: modelData
                                            accent: modelData === "Ctrl"
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 1
                                    color: Theme.faintTint
                                }
                            }
                        }
                    }
                }
            }
        }

        }

        Item {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 48

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
                text: "VT 内核：libghostty-vt (Ghostty)   ·   UI：Qt Quick"
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
