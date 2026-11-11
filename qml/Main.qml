import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtCore
import Luma 1.0
import "components"
import "panes"
import "overlays"

ApplicationWindow {
    id: window

    width: 1280
    height: 840
    minimumWidth: 940
    minimumHeight: 600
    visible: true
    title: "Luma 终端"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    property int terminalFontSize: 13
    property var tabs: []
    property int currentTab: 0
    property int layoutRevision: 0
    property string activePaneId: ""
    property var activeView: null
    property var activeSession: null
    property var paneViews: ({})
    property bool searchOpen: false
    property bool paletteOpen: false
    property bool helpOpen: false
    property bool settingsOpen: false
    property bool renameOpen: false
    property int renameIndex: -1
    property string renameText: ""
    readonly property bool overlayOpen: paletteOpen || helpOpen || settingsOpen || renameOpen
    property var demoPanes: []
    property var paneTitles: ({})

    Settings {
        id: appSettings
        category: "window"
        property int fontSize: 13
        property int winWidth: 1280
        property int winHeight: 840
    }

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

    function sessionTitle(session) {
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

    function currentTabData() {
        return (currentTab >= 0 && currentTab < tabs.length) ? tabs[currentTab] : null
    }

    function countPanes(node) {
        if (!node)
            return 0
        if (node.type === "leaf")
            return 1
        return countPanes(node.first) + countPanes(node.second)
    }

    function collectPanes(node, out) {
        if (!node)
            return out
        if (node.type === "leaf") {
            out.push(node.pane)
        } else {
            collectPanes(node.first, out)
            collectPanes(node.second, out)
        }
        return out
    }

    function containsPane(node, paneId) {
        if (!node)
            return false
        if (node.type === "leaf")
            return node.pane === paneId
        return containsPane(node.first, paneId) || containsPane(node.second, paneId)
    }

    function splitNode(node, target, newPane, orientation) {
        if (!node)
            return node
        if (node.type === "leaf") {
            if (node.pane === target) {
                return {
                    type: "split",
                    orientation: orientation,
                    ratio: 0.5,
                    first: node,
                    second: { type: "leaf", pane: newPane }
                }
            }
            return node
        }
        node.first = splitNode(node.first, target, newPane, orientation)
        node.second = splitNode(node.second, target, newPane, orientation)
        return node
    }

    function removeNode(node, target) {
        if (!node)
            return null
        if (node.type === "leaf")
            return node.pane === target ? null : node
        const first = removeNode(node.first, target)
        const second = removeNode(node.second, target)
        if (!first)
            return second
        if (!second)
            return first
        node.first = first
        node.second = second
        return node
    }

    function makeTab() {
        const id = Terminal.createSession()
        return {
            id: id,
            root: { type: "leaf", pane: id },
            focus: id,
            customTitle: "",
            displayTitle: "~",
            subtitle: "",
            paneCount: 1,
            shellName: Terminal.shellName
        }
    }

    function updateTabTitles() {
        if (tabs.length === 0) {
            window.title = "Luma 终端"
            return
        }
        const next = tabs.slice()
        for (let i = 0; i < next.length; ++i) {
            const tab = next[i]
            const session = Terminal.session(tab.focus)
            const fallback = session ? sessionTitle(session) : "终端"
            tab.displayTitle = tab.customTitle.length > 0 ? tab.customTitle : fallback
            tab.subtitle = session ? shortCwd(session.cwd) : ""
            tab.paneCount = countPanes(tab.root)
        }
        tabs = next
        const active = currentTabData()
        window.title = (active && active.displayTitle.length > 0
                        ? active.displayTitle + " — "
                        : "") + "Luma 终端"
    }

    function updateActiveRefs() {
        activeView = paneViews[activePaneId] ? paneViews[activePaneId] : null
        activeSession = activePaneId.length > 0 ? Terminal.session(activePaneId) : null
    }

    function createInitialTab() {
        if (tabs.length > 0)
            return
        const tab = makeTab()
        tabs = [tab]
        currentTab = 0
        activePaneId = tab.focus
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
    }

    function newTab() {
        closeSearch()
        const tab = makeTab()
        const next = tabs.slice()
        next.push(tab)
        tabs = next
        currentTab = next.length - 1
        activePaneId = tab.focus
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
    }

    function selectTab(index) {
        if (index < 0 || index >= tabs.length)
            return
        closeSearch()
        if (currentTab === index) {
            focusPane(tabs[index].focus)
            return
        }
        currentTab = index
        const tab = tabs[index]
        activePaneId = tab.focus
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
    }

    function closeTab(index) {
        if (index < 0 || index >= tabs.length)
            return
        closeSearch()
        const tab = tabs[index]
        const panes = collectPanes(tab.root, [])
        for (let i = 0; i < panes.length; ++i)
            Terminal.closeSession(panes[i])

        const next = tabs.slice()
        next.splice(index, 1)
        if (next.length === 0) {
            tabs = next
            window.close()
            return
        }
        if (currentTab > index)
            currentTab -= 1
        else if (currentTab === index)
            currentTab = Math.min(index, next.length - 1)
        tabs = next
        activePaneId = next[currentTab].focus
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
    }

    function splitPane(paneId, orientation) {
        if (!paneId)
            return ""
        let tab = null
        for (let i = 0; i < tabs.length; ++i) {
            if (containsPane(tabs[i].root, paneId)) {
                tab = tabs[i]
                break
            }
        }
        if (!tab)
            return ""
        const newPane = Terminal.createSession()
        tab.root = splitNode(tab.root, paneId, newPane, orientation)
        tab.focus = newPane
        activePaneId = newPane
        closeSearch()
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
        return newPane
    }

    function closePane(paneId) {
        let tabIndex = -1
        let tab = null
        for (let i = 0; i < tabs.length; ++i) {
            if (containsPane(tabs[i].root, paneId)) {
                tab = tabs[i]
                tabIndex = i
                break
            }
        }
        if (!tab)
            return
        if (countPanes(tab.root) <= 1) {
            closeTab(tabIndex)
            return
        }
        const newRoot = removeNode(tab.root, paneId)
        if (!newRoot) {
            closeTab(tabIndex)
            return
        }
        tab.root = newRoot
        Terminal.closeSession(paneId)
        const remaining = collectPanes(newRoot, [])
        tab.focus = remaining.length > 0 ? remaining[0] : ""
        activePaneId = tab.focus
        closeSearch()
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
    }

    function focusPane(paneId) {
        if (!paneId)
            return
        let tab = null
        for (let i = 0; i < tabs.length; ++i) {
            if (containsPane(tabs[i].root, paneId)) {
                tab = tabs[i]
                break
            }
        }
        if (!tab)
            return
        if (activePaneId !== paneId && searchOpen)
            closeSearch()
        tab.focus = paneId
        activePaneId = paneId
        updateTabTitles()
        updateActiveRefs()
        if (activeView)
            activeView.forceActiveFocus()
    }

    function cyclePane(direction) {
        const tab = currentTabData()
        if (!tab)
            return
        const panes = collectPanes(tab.root, [])
        if (panes.length < 2)
            return
        let index = panes.indexOf(activePaneId)
        if (index < 0)
            index = 0
        index = (index + direction + panes.length) % panes.length
        focusPane(panes[index])
    }

    function nextTab(direction) {
        if (tabs.length < 2)
            return
        selectTab((currentTab + direction + tabs.length) % tabs.length)
    }

    function registerPaneView(paneId, view) {
        paneViews[paneId] = view
        if (activePaneId === paneId) {
            activeView = view
            activeSession = view ? view.session : null
        }
    }

    function unregisterPaneView(paneId) {
        delete paneViews[paneId]
        if (activePaneId === paneId && !paneViews[paneId]) {
            activeView = null
            activeSession = Terminal.session(paneId)
        }
    }

    function rebuildPanes() {
        paneLoader.active = false
        paneLoader.active = true
    }

    function openSearch() {
        if (!activeView)
            return
        paletteOpen = false
        helpOpen = false
        settingsOpen = false
        searchOpen = true
    }

    function openRename(index) {
        if (index < 0 || index >= tabs.length)
            return
        const tab = tabs[index]
        renameIndex = index
        renameText = tab.customTitle.length > 0 ? tab.customTitle : tab.displayTitle
        paletteOpen = false
        helpOpen = false
        settingsOpen = false
        renameOpen = true
    }

    function renameTab(index, title) {
        if (index < 0 || index >= tabs.length)
            return
        const next = tabs.slice()
        next[index].customTitle = title.trim()
        tabs = next
        updateTabTitles()
        renameOpen = false
        if (activeView)
            activeView.forceActiveFocus()
    }

    function moveTab(index, delta) {
        const target = index + delta
        if (index < 0 || index >= tabs.length || target < 0 || target >= tabs.length)
            return
        const activeTabObject = tabs[currentTab]
        const next = tabs.slice()
        const item = next.splice(index, 1)[0]
        next.splice(target, 0, item)
        tabs = next
        currentTab = next.indexOf(activeTabObject)
        updateTabTitles()
    }

    function openSettings() {
        if (searchOpen)
            closeSearch()
        paletteOpen = false
        helpOpen = false
        settingsOpen = true
    }

    function closeSearch() {
        if (!searchOpen)
            return
        if (activeView)
            activeView.clearSearch()
        searchOpen = false
        if (activeView)
            activeView.forceActiveFocus()
    }

    function changeFontSize(delta) {
        if (delta === 0) {
            terminalFontSize = 13
        } else {
            terminalFontSize = Math.max(8, Math.min(24, terminalFontSize + delta))
        }
        showToast("字号 " + terminalFontSize)
    }

    function showToast(text) {
        toast.show(text)
    }

    function toggleFullscreen() {
        if (window.visibility === Window.FullScreen)
            window.showNormal()
        else
            window.showFullScreen()
    }

    function toggleMaximize() {
        if (window.visibility === Window.Maximized
            || window.visibility === Window.FullScreen)
            window.showNormal()
        else
            window.showMaximized()
    }

    function runAction(id) {
        switch (id) {
        case "new-tab":
            newTab()
            break
        case "split-right":
            splitPane(activePaneId, "row")
            break
        case "split-down":
            splitPane(activePaneId, "column")
            break
        case "close-pane":
            closePane(activePaneId)
            break
        case "close-tab":
            closeTab(currentTab)
            break
        case "search":
            Qt.callLater(openSearch)
            break
        case "copy":
            if (activeView && activeView.hasSelection) {
                activeView.copySelection()
                showToast("已复制选中内容")
            }
            break
        case "paste":
            if (activeView)
                activeView.pasteClipboard()
            break
        case "next-pane":
            cyclePane(1)
            break
        case "font-in":
            changeFontSize(1)
            break
        case "font-out":
            changeFontSize(-1)
            break
        case "font-reset":
            changeFontSize(0)
            break
        case "help":
            Qt.callLater(function() { window.helpOpen = true })
            break
        case "settings":
            Qt.callLater(openSettings)
            break
        case "rename-tab":
            Qt.callLater(function() { openRename(window.currentTab) })
            break
        case "toggle-theme":
            Theme.toggleMode()
            break
        case "fullscreen":
            toggleFullscreen()
            break
        case "quit":
            window.close()
            break
        }
    }

    function setupDemo() {
        if (tabs.length === 0)
            return
        const p1 = tabs[0].focus
        const p2 = Terminal.createSession()
        const p3 = Terminal.createSession()
        const p4 = Terminal.createSession()

        tabs[0].root = {
            type: "split",
            orientation: "row",
            ratio: 0.57,
            first: { type: "leaf", pane: p1 },
            second: {
                type: "split",
                orientation: "column",
                ratio: 0.52,
                first: { type: "leaf", pane: p2 },
                second: { type: "leaf", pane: p3 }
            }
        }
        tabs[0].customTitle = "开发"
        tabs[0].focus = p1

        const tab2 = {
            id: p4,
            root: { type: "leaf", pane: p4 },
            focus: p4,
            customTitle: "系统",
            displayTitle: "系统",
            subtitle: "",
            paneCount: 1,
            shellName: Terminal.shellName
        }
        const next = tabs.slice()
        next.push(tab2)
        tabs = next

        currentTab = 0
        activePaneId = p1
        demoPanes = [p1, p2, p3, p4]
        const labels = {}
        labels[p1] = "dev"
        labels[p2] = "system"
        labels[p3] = "palette"
        labels[p4] = "logs"
        window.paneTitles = labels
        updateTabTitles()
        updateActiveRefs()
        layoutRevision++
        demoCommandTimer.restart()
    }

    function runDemoCommands() {
        if (demoPanes.length < 4)
            return
        const p1 = demoPanes[0]
        const p2 = demoPanes[1]
        const p3 = demoPanes[2]
        const p4 = demoPanes[3]

        const cmd1 = "clear; printf '\\033]0;dev\\007'; printf '\\033[1;38;2;126;177;255m  Luma\\033[0m  \\033[38;2;107;211;151mTerminal\\033[0m\\n'; printf '\\033[38;2;132;142;162m  现代 macOS 风格终端  ·  libghostty-vt\\033[0m\\n\\n'; ls -1 --color=always | head -14; echo; git status -sb 2>/dev/null | head -4; printf '\\033[0m'\n"
        const cmd2 = "clear; printf '\\033]0;system\\007'; printf '\\033[1;38;2;245;196;81m  ✦ 系统\\033[0m\\n\\n'; printf '\\033[38;2;132;142;162mhost    \\033[0m'; hostname; printf '\\033[38;2;132;142;162mkernel  \\033[0m'; uname -r; printf '\\033[38;2;132;142;162muptime  \\033[0m'; uptime -p; echo; printf '\\033[38;2;132;142;162mdisk    \\033[0m'; df -h / | tail -1; printf '\\033[0m'\n"
        const cmd3 = "clear; printf '\\033]0;palette\\007'; printf '\\033[1;38;2;199;146;234m  ✦ 调色板\\033[0m\\n\\n'; for c in 0 1 2 3 4 5 6 7; do printf \"\\033[4${c}m   \"; done; printf '\\033[0m'; echo; for c in 0 1 2 3 4 5 6 7; do printf \"\\033[10${c}m   \"; done; printf '\\033[0m'; echo; echo; printf '\\033[38;2;110;168;254mtruecolor\\033[0m  \\033[38;2;154;232;172m256-color\\033[0m  \\033[38;2;245;196;81m✔\\033[0m\\n'\n"
        const cmd4 = "clear; printf '\\033]0;logs\\007'; printf '\\033[1;38;2;126;214;160m  ✦ 最近的系统事件\\033[0m\\n\\n'; journalctl -n 9 --no-pager -o short-iso 2>/dev/null | tail -9\n"

        const s1 = Terminal.session(p1)
        const s2 = Terminal.session(p2)
        const s3 = Terminal.session(p3)
        const s4 = Terminal.session(p4)
        if (s1) s1.writeText(cmd1)
        if (s2) s2.writeText(cmd2)
        if (s3) s3.writeText(cmd3)
        if (s4) s4.writeText(cmd4)
    }

    onLayoutRevisionChanged: {
        if (searchOpen)
            closeSearch()
        rebuildPanes()
    }

    onTerminalFontSizeChanged: appSettings.fontSize = terminalFontSize

    onClosing: {
        if (!screenshotMode && visibility !== Window.FullScreen) {
            appSettings.winWidth = width
            appSettings.winHeight = height
        }
    }

    onActivePaneIdChanged: updateActiveRefs()

    Connections {
        target: Terminal
        function onSessionTitleChanged(id, title) { window.updateTabTitles() }
        function onSessionCwdChanged(id, cwd) { window.updateTabTitles() }
        function onSessionExited(id, code) { window.updateTabTitles() }
        function onSessionNotification(id, title, body) {
            window.showToast((title.length > 0 ? title : "终端通知")
                             + (body.length > 0 ? " · " + body : ""))
        }
    }

    Component.onCompleted: {
        terminalFontSize = Math.max(8, Math.min(24, appSettings.fontSize))
        if (appSettings.winWidth >= minimumWidth)
            width = appSettings.winWidth
        if (appSettings.winHeight >= minimumHeight)
            height = appSettings.winHeight
        createInitialTab()
        if (demoMode)
            demoTimer.start()
        if (screenshotUi.length > 0)
            screenshotOverlayTimer.start()
    }

    Timer {
        id: screenshotOverlayTimer
        interval: 1900
        repeat: false
        onTriggered: {
            if (screenshotUi === "palette")
                window.paletteOpen = true
            else if (screenshotUi === "help")
                window.helpOpen = true
            else if (screenshotUi === "search")
                searchBar.openWithQuery("Luma")
            else if (screenshotUi === "selection") {
                if (window.activeView) {
                    window.activeView.selectAll()
                    window.activeView.copySelection()
                }
                window.showToast("已复制全部内容")
            } else if (screenshotUi === "tab2")
                window.selectTab(1)
            else if (screenshotUi === "light")
                Theme.mode = "light"
            else if (screenshotUi === "dark")
                Theme.mode = "dark"
            else if (screenshotUi === "settings")
                window.openSettings()
            else if (screenshotUi === "rename")
                window.openRename(window.currentTab)
            else if (screenshotUi === "light-settings") {
                Theme.mode = "light"
                window.openSettings()
            }
        }
    }

    Timer {
        id: demoTimer
        interval: 420
        repeat: false
        onTriggered: window.setupDemo()
    }

    Timer {
        id: demoCommandTimer
        interval: 700
        repeat: false
        onTriggered: window.runDemoCommands()
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 14
        color: Theme.bgBottom
        border.width: 1
        border.color: Theme.windowBorder

        Image {
            anchors.fill: parent
            source: Theme.dark ? "../assets/backdrop.png" : "../assets/backdrop-light.png"
            fillMode: Image.PreserveAspectCrop
            smooth: true
        }

        Rectangle {
            anchors.fill: parent
            radius: 14
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop {
                    position: 0.62
                    color: Theme.dark ? Qt.rgba(0.035, 0.04, 0.055, 0.18)
                                      : Qt.rgba(0.72, 0.76, 0.84, 0.04)
                }
                GradientStop {
                    position: 1.0
                    color: Theme.dark ? Qt.rgba(0.025, 0.03, 0.042, 0.62)
                                      : Qt.rgba(0.78, 0.81, 0.87, 0.16)
                }
            }
        }

        // ── Sidebar ─────────────────────────────────────────────────────
        Item {
            id: sidebar
            width: 238
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.bottom: parent.bottom

            Rectangle {
                id: sidebarBackground
                anchors.fill: parent
                radius: 14
                color: Theme.sidebarBg
            }
            Rectangle {
                x: 14
                width: parent.width - 14
                height: parent.height
                color: Theme.sidebarBg
            }
            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height - 28
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.sidebarBorder
            }

            MouseArea {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 52
                acceptedButtons: Qt.LeftButton
                onPressed: window.startSystemMove()
                onDoubleClicked: window.toggleMaximize()
            }

            TrafficLights {
                windowRoot: window
                x: 18
                y: 17
                z: 5
            }

            Item {
                id: brand
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 58
                width: parent.width - 36
                height: 32

                Rectangle {
                    id: logoBadge
                    width: 30
                    height: 30
                    radius: 9
                    color: "transparent"

                    Image {
                        anchors.fill: parent
                        source: "../assets/luma.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                }

                Column {
                    anchors.left: logoBadge.right
                    anchors.leftMargin: 10
                    anchors.verticalCenter: logoBadge.verticalCenter
                    spacing: 0

                    Text {
                        text: "Luma"
                        color: Theme.textPrimary
                        font.family: Theme.uiFont
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.2
                    }
                    Text {
                        text: "TERMINAL"
                        color: Theme.textTertiary
                        font.family: Theme.uiFont
                        font.pixelSize: 8
                        font.letterSpacing: 1.8
                        font.weight: Font.Medium
                    }
                }
            }

            Item {
                id: tabsSectionHeader
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.top: parent.top
                anchors.topMargin: 112
                height: 24

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "标签页"
                    color: Theme.textTertiary
                    font.family: Theme.uiFont
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.7
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 44
                    anchors.verticalCenter: parent.verticalCenter
                    text: window.tabs.length
                    color: Theme.textTertiary
                    font.family: Theme.monoFont
                    font.pixelSize: 9
                    opacity: 0.7
                }

                IconButton {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "plus"
                    tooltip: "新建标签页"
                    size: 22
                    iconSize: 13
                    color: Theme.textSecondary
                    onClicked: window.newTab()
                }
            }

            Flickable {
                id: tabsFlick
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.top: tabsSectionHeader.bottom
                anchors.topMargin: 5
                anchors.bottom: newTabButton.top
                anchors.bottomMargin: 9
                clip: true
                contentHeight: tabsColumn.height
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: tabsColumn
                    width: parent.width
                    spacing: 3

                    Repeater {
                        model: window.tabs
                        delegate: SidebarTab {
                            width: tabsColumn.width
                            windowRoot: window
                            tab: modelData
                            tabIndex: index
                            isActive: index === window.currentTab
                            onActivated: window.selectTab(index)
                            onCloseRequested: window.closeTab(index)
                            onRenameRequested: window.openRename(index)
                            onMoveUpRequested: window.moveTab(index, -1)
                            onMoveDownRequested: window.moveTab(index, 1)
                        }
                    }
                }
            }

            Rectangle {
                id: newTabButton
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.bottom: shortcutButton.top
                anchors.bottomMargin: 10
                height: 33
                radius: 8
                color: newTabHover.hovered ? Theme.hoverTint : Theme.softTint
                border.width: 1
                border.color: Theme.subtleBorder
                Behavior on color { ColorAnimation { duration: 120 } }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7

                    Icon {
                        name: "plus"
                        size: 13
                        color: Theme.textSecondary
                    }
                    Text {
                        text: "新建标签页"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 11
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Ctrl ⇧ T"
                    color: Theme.textTertiary
                    font.family: Theme.monoFont
                    font.pixelSize: 8
                }

                HoverHandler { id: newTabHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: window.newTab()
                }
            }

            Rectangle {
                id: shortcutButton
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.bottom: versionLabel.top
                anchors.bottomMargin: 12
                height: 33
                radius: 8
                color: shortcutHover.hovered ? Theme.subtleBorder : Theme.faintTint
                border.width: 1
                border.color: Theme.softTint
                Behavior on color { ColorAnimation { duration: 120 } }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Icon {
                        name: "keyboard"
                        size: 14
                        color: Theme.textSecondary
                    }
                    Text {
                        text: "键盘快捷键"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 11
                    }
                }

                KeyCap {
                    anchors.right: parent.right
                    anchors.rightMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    label: "?"
                }

                HoverHandler { id: shortcutHover }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: window.helpOpen = true
                }
            }

            Text {
                id: versionLabel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 13
                text: "Luma " + appVersion
                color: Theme.textTertiary
                font.family: Theme.monoFont
                font.pixelSize: 8
                opacity: 0.8
            }
        }

        // ── Main area ──────────────────────────────────────────────────
        Item {
            id: mainArea
            anchors.left: sidebar.right
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            Item {
                id: toolbar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 56

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onPressed: window.startSystemMove()
                    onDoubleClicked: window.toggleMaximize()
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1

                    Text {
                        text: window.currentTabData() ? window.currentTabData().displayTitle : "Luma"
                        color: Theme.textPrimary
                        font.family: Theme.uiFont
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: window.currentTabData() && window.currentTabData().subtitle
                              ? window.currentTabData().subtitle
                              : "终端会话"
                        color: Theme.textTertiary
                        font.family: Theme.uiFont
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                        width: Math.min(implicitWidth, 360)
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3

                    IconButton {
                        icon: "split-right"
                        tooltip: "向右分屏  Ctrl+Shift+D"
                        size: 28
                        iconSize: 15
                        color: Theme.textSecondary
                        onClicked: window.splitPane(window.activePaneId, "row")
                    }
                    IconButton {
                        icon: "split-down"
                        tooltip: "向下分屏  Ctrl+Shift+E"
                        size: 28
                        iconSize: 15
                        color: Theme.textSecondary
                        onClicked: window.splitPane(window.activePaneId, "column")
                    }
                    IconButton {
                        icon: "search"
                        tooltip: "查找  Ctrl+Shift+F"
                        size: 28
                        iconSize: 14
                        color: Theme.textSecondary
                        onClicked: window.openSearch()
                    }

                    Rectangle {
                        id: paletteButton
                        width: 82
                        height: 28
                        radius: 7
                        color: paletteHover.hovered ? Theme.hoverTint : Theme.softTint
                        border.width: 1
                        border.color: Theme.subtleBorder
                        Behavior on color { ColorAnimation { duration: 120 } }

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Icon {
                                name: "sparkles"
                                size: 13
                                color: Theme.accent
                            }
                            Text {
                                text: "命令"
                                color: Theme.textSecondary
                                font.family: Theme.uiFont
                                font.pixelSize: 10
                            }
                        }

                        HoverHandler { id: paletteHover }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: window.paletteOpen = true
                        }
                    }

                    IconButton {
                        icon: Theme.dark ? "moon" : "sun"
                        tooltip: Theme.dark ? "切换到浅色外观" : "切换到深色外观"
                        size: 28
                        iconSize: 15
                        color: Theme.textSecondary
                        onClicked: Theme.toggleMode()
                    }

                    IconButton {
                        icon: "settings"
                        tooltip: "设置  Ctrl+,"
                        size: 28
                        iconSize: 15
                        color: Theme.textSecondary
                        onClicked: window.openSettings()
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.softTint
                }
            }

            Item {
                id: contentArea
                anchors.top: toolbar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: statusBar.top
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 6
                anchors.bottomMargin: 4

                Loader {
                    id: paneLoader
                    anchors.fill: parent
                    active: false
                    sourceComponent: Component {
                        PaneRoot {
                            windowRoot: window
                            treeNode: window.currentTabData() ? window.currentTabData().root : null
                        }
                    }
                }

                SearchBar {
                    id: searchBar
                    windowRoot: window
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 4
                    anchors.rightMargin: 10
                    z: 60
                }

                Rectangle {
                    id: scrollToBottomChip
                    visible: window.activeView && !window.activeView.atBottom
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 16
                    anchors.bottomMargin: 14
                    width: 104
                    height: 30
                    radius: 8
                    color: scrollChipHover.hovered
                           ? Qt.rgba(0.10, 0.12, 0.16, 0.98)
                           : Theme.popoverBg
                    border.width: 1
                    border.color: Theme.popoverBorder
                    z: 45

                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        Icon { name: "arrow-down"; size: 12; color: Theme.accent }
                        Text {
                            text: "回到底部"
                            color: Theme.textSecondary
                            font.family: Theme.uiFont
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    HoverHandler { id: scrollChipHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (window.activeView) {
                                window.activeView.resetScroll()
                                window.activeView.forceActiveFocus()
                            }
                        }
                    }
                }
            }

            Item {
                id: statusBar
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 30

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: Theme.softTint
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: window.activeSession && window.activeSession.running
                               ? Theme.success : Theme.textTertiary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.activeSession ? window.activeSession.shellName : "终端"
                        color: Theme.textSecondary
                        font.family: Theme.uiFont
                        font.pixelSize: 10
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "·"
                        color: Theme.textTertiary
                        font.family: Theme.uiFont
                        font.pixelSize: 10
                        opacity: 0.7
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 300)
                        text: window.activeSession ? window.shortCwd(window.activeSession.cwd) : ""
                        color: Theme.textTertiary
                        font.family: Theme.uiFont
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 9

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: window.activeSession
                              ? (window.activeSession.viewportActive
                                 ? "行 " + (window.activeSession.cursorRow + 1)
                                   + ", 列 " + (window.activeSession.cursorCol + 1)
                                 : "历史回看")
                              : ""
                        color: Theme.textTertiary
                        font.family: Theme.monoFont
                        font.pixelSize: 9
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "UTF-8"
                        color: Theme.textTertiary
                        font.family: Theme.monoFont
                        font.pixelSize: 9
                    }

                    Rectangle {
                        width: engineRow.implicitWidth + 14
                        height: 19
                        radius: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(0.357, 0.608, 1, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(0.357, 0.608, 1, 0.14)

                        Row {
                            id: engineRow
                            anchors.centerIn: parent
                            spacing: 5
                            Icon {
                                name: "sparkles"
                                size: 10
                                color: Theme.accent
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "ghostty-vt"
                                color: Theme.accent
                                font.family: Theme.monoFont
                                font.pixelSize: 8
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 14
            color: "transparent"
            border.width: 1
            border.color: Theme.windowBorder
            z: 900
        }
    }

    // ── Overlays ───────────────────────────────────────────────────────
    CommandPalette {
        anchors.fill: parent
        windowRoot: window
        z: 300
    }

    HelpOverlay {
        anchors.fill: parent
        windowRoot: window
        z: 400
    }

    SettingsOverlay {
        anchors.fill: parent
        windowRoot: window
        z: 450
    }

    RenameTabOverlay {
        anchors.fill: parent
        windowRoot: window
        z: 460
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 52
        z: 500
    }

    // ── Shortcuts ──────────────────────────────────────────────────────
    Shortcut { sequence: "Ctrl+Shift+T"; enabled: !overlayOpen; onActivated: window.newTab() }
    Shortcut { sequence: "Ctrl+Shift+D"; enabled: !overlayOpen; onActivated: window.splitPane(window.activePaneId, "row") }
    Shortcut { sequence: "Ctrl+Shift+E"; enabled: !overlayOpen; onActivated: window.splitPane(window.activePaneId, "column") }
    Shortcut { sequence: "Ctrl+Shift+W"; enabled: !overlayOpen; onActivated: window.closePane(window.activePaneId) }
    Shortcut { sequence: "Ctrl+Shift+Q"; onActivated: window.close() }
    Shortcut { sequence: "Ctrl+Shift+C"; enabled: !overlayOpen; onActivated: window.runAction("copy") }
    Shortcut { sequence: "Ctrl+Shift+V"; enabled: !overlayOpen; onActivated: window.runAction("paste") }
    Shortcut { sequence: "Ctrl+Shift+A"; enabled: !overlayOpen; onActivated: if (window.activeView) window.activeView.selectAll() }
    Shortcut { sequence: "Ctrl+Shift+F"; enabled: !overlayOpen; onActivated: window.openSearch() }
    Shortcut { sequence: "Ctrl+Shift+P"; enabled: !overlayOpen; onActivated: window.paletteOpen = !window.paletteOpen }
    Shortcut { sequence: "Ctrl+Shift+K"; enabled: !overlayOpen; onActivated: window.paletteOpen = !window.paletteOpen }
    Shortcut { sequence: "Ctrl+Shift+?"; enabled: !overlayOpen; onActivated: window.helpOpen = !window.helpOpen }
    Shortcut { sequence: "Ctrl+Shift+/"; enabled: !overlayOpen; onActivated: window.helpOpen = !window.helpOpen }
    Shortcut { sequence: "Ctrl+Shift+="; enabled: !overlayOpen; onActivated: window.changeFontSize(1) }
    Shortcut { sequence: "Ctrl+Shift++"; enabled: !overlayOpen; onActivated: window.changeFontSize(1) }
    Shortcut { sequence: "Ctrl+Shift+-"; enabled: !overlayOpen; onActivated: window.changeFontSize(-1) }
    Shortcut { sequence: "Ctrl+Shift+_"; enabled: !overlayOpen; onActivated: window.changeFontSize(-1) }
    Shortcut { sequence: "Ctrl+Shift+0"; enabled: !overlayOpen; onActivated: window.changeFontSize(0) }
    Shortcut { sequence: "Ctrl+Tab"; enabled: !overlayOpen; onActivated: window.nextTab(1) }
    Shortcut { sequence: "Ctrl+Shift+Tab"; enabled: !overlayOpen; onActivated: window.nextTab(-1) }
    Shortcut { sequence: "Ctrl+Shift+]"; enabled: !overlayOpen; onActivated: window.nextTab(1) }
    Shortcut { sequence: "Ctrl+Shift+["; enabled: !overlayOpen; onActivated: window.nextTab(-1) }
    Shortcut { sequence: "Ctrl+Alt+Left"; enabled: !overlayOpen; onActivated: window.cyclePane(-1) }
    Shortcut { sequence: "Ctrl+Alt+Right"; enabled: !overlayOpen; onActivated: window.cyclePane(1) }
    Shortcut { sequence: "Ctrl+,"; enabled: !overlayOpen || settingsOpen; onActivated: window.openSettings() }
    Shortcut { sequence: "F11"; onActivated: window.toggleFullscreen() }
    Shortcut {
        sequence: "Escape"
        enabled: window.helpOpen || window.settingsOpen || window.renameOpen
                 || (window.searchOpen && !window.paletteOpen)
        onActivated: {
            if (window.helpOpen)
                window.helpOpen = false
            else if (window.settingsOpen)
                window.settingsOpen = false
            else if (window.renameOpen)
                window.renameOpen = false
            else if (window.searchOpen)
                window.closeSearch()
        }
    }

    // ── Window resize handles ──────────────────────────────────────────
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 5
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.RightEdge)
    }
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 5
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 5
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.BottomEdge)
    }
}
