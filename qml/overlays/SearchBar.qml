import QtQuick
import QtQuick.Controls
import Luma 1.0
import "../components"

Item {
    id: root

    property var windowRoot
    readonly property var view: windowRoot ? windowRoot.activeView : null
    readonly property int matchCount: view ? view.searchMatchCount : 0
    readonly property int currentMatch: view ? view.searchCurrentIndex : 0

    width: 392
    height: 42
    visible: windowRoot ? windowRoot.searchOpen : false
    opacity: visible ? 1 : 0
    scale: visible ? 1 : 0.97
    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    function openWithQuery(query) {
        if (!windowRoot)
            return
        windowRoot.searchOpen = true
        Qt.callLater(function() {
            searchField.text = query
            searchField.forceActiveFocus()
            searchField.selectAll()
            searchTimer.restart()
        })
    }

    function close() {
        if (view)
            view.clearSearch()
        if (windowRoot) {
            windowRoot.searchOpen = false
            if (windowRoot.activeView)
                windowRoot.activeView.forceActiveFocus()
        }
    }

    Connections {
        target: root.windowRoot
        function onSearchOpenChanged() {
            if (!root.windowRoot.searchOpen)
                return
            searchField.text = ""
            Qt.callLater(function() {
                searchField.forceActiveFocus()
                searchField.selectAll()
            })
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: Theme.popoverBg
        border.width: 1
        border.color: Theme.popoverBorder

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: Theme.softTint
        }
    }

    Icon {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        name: "search"
        size: 14
        color: Theme.textTertiary
    }

    TextField {
        id: searchField
        anchors.left: parent.left
        anchors.leftMargin: 34
        anchors.right: countLabel.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        height: 32
        padding: 0
        leftPadding: 0
        rightPadding: 0
        background: null
        color: Theme.textPrimary
        placeholderText: "在终端中查找…"
        placeholderTextColor: Theme.textTertiary
        selectionColor: Qt.rgba(0.357, 0.608, 1, 0.35)
        selectedTextColor: Theme.textPrimary
        font.family: Theme.uiFont
        font.pixelSize: 12
        selectByMouse: true
        verticalAlignment: TextInput.AlignVCenter

        Keys.onEscapePressed: root.close()
        Keys.onReturnPressed: function(event) {
            if (root.view)
                root.view.findNext((event.modifiers & Qt.ShiftModifier) !== 0)
        }
        Keys.onEnterPressed: function(event) {
            if (root.view)
                root.view.findNext((event.modifiers & Qt.ShiftModifier) !== 0)
        }

        onTextChanged: {
            if (text.length === 0) {
                searchTimer.stop()
                if (root.view)
                    root.view.clearSearch()
            } else {
                searchTimer.restart()
            }
        }
    }

    Timer {
        id: searchTimer
        interval: 110
        repeat: false
        onTriggered: {
            if (root.view)
                root.view.setSearchQuery(searchField.text)
        }
    }

    Text {
        id: countLabel
        anchors.right: previousButton.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(34, implicitWidth)
        text: searchField.text.length === 0
              ? ""
              : (root.matchCount > 0 ? root.currentMatch + "/" + root.matchCount : "0/0")
        color: root.matchCount > 0 ? Theme.textSecondary : Theme.textTertiary
        font.family: Theme.monoFont
        font.pixelSize: 10
        horizontalAlignment: Text.AlignRight
    }

    IconButton {
        id: previousButton
        anchors.right: nextButton.left
        anchors.verticalCenter: parent.verticalCenter
        icon: "chevron-up"
        tooltip: "上一个匹配"
        size: 24
        iconSize: 12
        enabled: root.matchCount > 0
        color: Theme.textSecondary
        onClicked: if (root.view) root.view.findNext(true)
    }

    IconButton {
        id: nextButton
        anchors.right: closeButton.left
        anchors.verticalCenter: parent.verticalCenter
        icon: "chevron-down"
        tooltip: "下一个匹配"
        size: 24
        iconSize: 12
        enabled: root.matchCount > 0
        color: Theme.textSecondary
        onClicked: if (root.view) root.view.findNext(false)
    }

    IconButton {
        id: closeButton
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        icon: "close"
        tooltip: "关闭查找"
        size: 24
        iconSize: 11
        color: Theme.textTertiary
        onClicked: root.close()
    }
}
