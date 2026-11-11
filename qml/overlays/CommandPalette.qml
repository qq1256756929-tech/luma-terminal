import QtQuick
import QtQuick.Controls
import Luma 1.0
import "../components"

Item {
    id: root

    property var windowRoot
    property int selectedIndex: 0
    property var filteredActions: []

    readonly property var allActions: [
        { id: "new-tab", title: "新建标签页", shortcut: "Ctrl ⇧ T", icon: "plus", keywords: "tab new" },
        { id: "split-right", title: "向右分屏", shortcut: "Ctrl ⇧ D", icon: "split-right", keywords: "split vertical pane" },
        { id: "split-down", title: "向下分屏", shortcut: "Ctrl ⇧ E", icon: "split-down", keywords: "split horizontal pane" },
        { id: "close-pane", title: "关闭当前窗格", shortcut: "Ctrl ⇧ W", icon: "close", keywords: "close pane" },
        { id: "close-tab", title: "关闭当前标签页", shortcut: "", icon: "close", keywords: "close tab" },
        { id: "rename-tab", title: "重命名当前标签页", shortcut: "", icon: "terminal", keywords: "rename tab title" },
        { id: "search", title: "在终端中查找", shortcut: "Ctrl ⇧ F", icon: "search", keywords: "find search" },
        { id: "copy", title: "复制选中内容", shortcut: "Ctrl ⇧ C", icon: "copy", keywords: "copy" },
        { id: "paste", title: "粘贴", shortcut: "Ctrl ⇧ V", icon: "paste", keywords: "paste" },
        { id: "next-pane", title: "聚焦下一个窗格", shortcut: "Ctrl ⌥ →", icon: "arrow-right", keywords: "pane focus next" },
        { id: "font-in", title: "放大字号", shortcut: "Ctrl ⇧ +", icon: "zoom-in", keywords: "font larger zoom" },
        { id: "font-out", title: "缩小字号", shortcut: "Ctrl ⇧ −", icon: "zoom-out", keywords: "font smaller zoom" },
        { id: "font-reset", title: "重置字号", shortcut: "Ctrl ⇧ 0", icon: "refresh", keywords: "font reset" },
        { id: "settings", title: "打开设置", shortcut: "Ctrl ,", icon: "settings", keywords: "settings preferences config" },
        { id: "toggle-theme", title: "切换深色 / 浅色外观", shortcut: "", icon: "moon", keywords: "theme dark light appearance" },
        { id: "help", title: "键盘快捷键", shortcut: "Ctrl ⇧ ?", icon: "help", keywords: "help shortcuts" },
        { id: "fullscreen", title: "切换全屏", shortcut: "F11", icon: "maximize", keywords: "fullscreen" },
        { id: "quit", title: "退出 Luma", shortcut: "Ctrl ⇧ Q", icon: "close", keywords: "quit exit" }
    ]

    visible: windowRoot ? windowRoot.paletteOpen : false
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    function refilter() {
        const q = filterField.text.trim().toLowerCase()
        if (q.length === 0) {
            filteredActions = allActions
        } else {
            filteredActions = allActions.filter(function(item) {
                return (item.title + " " + item.keywords).toLowerCase().indexOf(q) >= 0
            })
        }
        selectedIndex = 0
        actionList.currentIndex = 0
    }

    function openPalette() {
        if (!windowRoot)
            return
        filterField.text = ""
        refilter()
        Qt.callLater(function() { filterField.forceActiveFocus() })
    }

    function closePalette() {
        if (!windowRoot)
            return
        windowRoot.paletteOpen = false
        if (windowRoot.activeView)
            windowRoot.activeView.forceActiveFocus()
    }

    function execute(index) {
        if (index < 0 || index >= filteredActions.length)
            return
        const id = filteredActions[index].id
        closePalette()
        if (windowRoot)
            windowRoot.runAction(id)
    }

    Connections {
        target: root.windowRoot
        function onPaletteOpenChanged() {
            if (root.windowRoot.paletteOpen)
                root.openPalette()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.dimOverlay

        MouseArea {
            anchors.fill: parent
            onClicked: root.closePalette()
        }
    }

    Rectangle {
        id: card
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(56, (parent.height - height) * 0.16)
        width: Math.min(560, parent.width - 80)
        height: 56 + listHeight + 38
        radius: 16
        color: Theme.popoverBg
        border.width: 1
        border.color: Theme.popoverBorder

        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

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
            id: inputRow
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 56

            Icon {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                name: "sparkles"
                size: 17
                color: Theme.accent
            }

            TextField {
                id: filterField
                anchors.left: parent.left
                anchors.leftMargin: 48
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                height: 30
                padding: 0
                leftPadding: 0
                rightPadding: 0
                background: null
                color: Theme.textPrimary
                placeholderText: "输入命令、操作或搜索…"
                placeholderTextColor: Theme.textTertiary
                selectionColor: Qt.rgba(0.357, 0.608, 1, 0.35)
                selectedTextColor: Theme.textPrimary
                font.family: Theme.uiFont
                font.pixelSize: 13
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter

                onTextChanged: root.refilter()

                Keys.onDownPressed: function(event) {
                    if (root.filteredActions.length === 0)
                        return
                    root.selectedIndex = Math.min(root.filteredActions.length - 1, root.selectedIndex + 1)
                    actionList.currentIndex = root.selectedIndex
                    actionList.positionViewAtIndex(root.selectedIndex, ListView.Contain)
                    event.accepted = true
                }
                Keys.onUpPressed: function(event) {
                    if (root.filteredActions.length === 0)
                        return
                    root.selectedIndex = Math.max(0, root.selectedIndex - 1)
                    actionList.currentIndex = root.selectedIndex
                    actionList.positionViewAtIndex(root.selectedIndex, ListView.Contain)
                    event.accepted = true
                }
                Keys.onReturnPressed: function(event) {
                    root.execute(root.selectedIndex)
                    event.accepted = true
                }
                Keys.onEnterPressed: function(event) {
                    root.execute(root.selectedIndex)
                    event.accepted = true
                }
                Keys.onEscapePressed: function(event) {
                    root.closePalette()
                    event.accepted = true
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.subtleBorder
            }
        }

        readonly property int listHeight: Math.min(
            Math.min(540, root.filteredActions.length * 30),
            Math.max(180, root.height - 220))

        ListView {
            id: actionList
            anchors.top: inputRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: card.listHeight
            clip: true
            model: root.filteredActions
            currentIndex: root.selectedIndex
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: row
                width: ListView.view.width
                height: 30

                property bool hovered: rowHover.hovered
                property bool selected: index === root.selectedIndex

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.topMargin: 2
                    anchors.bottomMargin: 2
                    radius: 8
                    color: row.selected || row.hovered ? Theme.hoverTint : "transparent"
                    border.width: row.selected ? 1 : 0
                    border.color: Theme.subtleBorder
                }

                Icon {
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    name: modelData.icon
                    size: 15
                    color: row.selected ? Theme.accent : Theme.textSecondary
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 47
                    anchors.right: shortcutText.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.title
                    color: row.selected ? Theme.textPrimary : Theme.textSecondary
                    font.family: Theme.uiFont
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Text {
                    id: shortcutText
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.shortcut
                    color: Theme.textTertiary
                    font.family: Theme.monoFont
                    font.pixelSize: 9
                    visible: text.length > 0
                }

                HoverHandler { id: rowHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.execute(index)
                }
            }
        }

        Text {
            anchors.top: inputRow.bottom
            anchors.topMargin: 26
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.filteredActions.length === 0
            text: "没有匹配的操作"
            color: Theme.textTertiary
            font.family: Theme.uiFont
            font.pixelSize: 12
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 30
            color: Theme.faintTint
            radius: 16

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.softTint
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                KeyCap { label: "↑"; accent: false }
                KeyCap { label: "↓"; accent: false }
                Text {
                    text: "选择"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 9
                    anchors.verticalCenter: parent.verticalCenter
                }
                Item { width: 4; height: 1 }
                KeyCap { label: "↵"; accent: true }
                Text {
                    text: "执行"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 9
                    anchors.verticalCenter: parent.verticalCenter
                }
                Item { width: 4; height: 1 }
                KeyCap { label: "esc"; accent: false }
                Text {
                    text: "关闭"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 9
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: "Luma 命令"
                color: Theme.textTertiary
                font.family: Theme.uiFont
                font.pixelSize: 9
            }
        }
    }

}
