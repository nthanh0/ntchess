import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: mainWindow
    visible: true
    title: qsTr("ntchess")
    color: "#1e1e1e"

    // ── square size: closest available piece resolution to Screen.height/12 ──
    property int squareSize: { const s = [32,64,96,128,256,512,1024]; const t = Screen.height / 12; for (let i = 0; i < s.length; i++) if (s[i] >= t) return s[i]; return s[s.length-1] }
    readonly property string pieceSizeFolder: {
        const sizes = [32, 64, 96, 128, 256, 512, 1024]
        let best = sizes[0], bestDiff = Math.abs(squareSize - best)
        for (let sz of sizes) {
            const d = Math.abs(squareSize - sz)
            if (d < bestDiff) { best = sz; bestDiff = d }
        }
        return best + "x" + best
    }

    // panel is wider now to hold history, clocks, controls
    readonly property int boardPx:   squareSize * 8
    readonly property int panelW:    Math.max(260, Math.round(squareSize * 4.5))
    readonly property int sp:        Math.round(squareSize * 0.18)  // small spacing unit
    readonly property int evalBarW:  Math.round(squareSize * 0.25) // eval bar (analysis only)

    // analysis screen is slightly wider (eval bar added between board and panel)
    readonly property int targetWidth:  appScreen === "analysis" ? boardPx + evalBarW + panelW : boardPx + panelW
    readonly property int targetHeight: boardPx
    width:  targetWidth
    height: targetHeight
    // Drive min/max via change handlers to avoid the bind-ordering deadlock
    // (minimumWidth > maximumWidth momentarily) when switching screens.
    onTargetWidthChanged: { minimumWidth = targetWidth; maximumWidth = targetWidth }
    onTargetHeightChanged: { minimumHeight = targetHeight; maximumHeight = targetHeight }
    Component.onCompleted: {
        minimumWidth  = targetWidth;  maximumWidth  = targetWidth
        minimumHeight = targetHeight; maximumHeight = targetHeight
        activeBoardTheme = chessboard.savedBoardTheme()
        activePieceTheme = chessboard.savedPieceTheme()
    }

    // current theme state – loaded from settings.ini via C++ backend
    property string activeBoardTheme: "brown"
    property string activePieceTheme: "cburnett"

    // engine display name (updated once engine initialises)
    property string computerName: chessboard.engineName !== "" ? chessboard.engineName : "Computer"

    // ── screen state: "menu" | "playing" ────────────────────────────────────
    property string appScreen: "menu"

    // ── player info ────────────────────────────────────────────────────────────
    property string whitePlayerName: "White"
    property string blackPlayerName: "Black"
    property int    whitePlayerElo:  0
    property int    blackPlayerElo:  0
    property int    resignedSide:     -1  // -1=none, 0=white resigned, 1=black resigned
    // -1=HvH, 0=engine white, 1=engine black, 2=CvC
    property int    gameComputerSide: -1

    // ── helpers ─────────────────────────────────────────────────────────────
    // piece enum (1-12) → small image url using 32x32 folder
    function smallPieceUrl(pid) {
        if (pid <= 0) return ""
        const names = ["","wK","wQ","wR","wB","wN","wP","bK","bQ","bR","bB","bN","bP"]
        return assetsPath + "/piece/" + activePieceTheme + "/32x32/" + names[pid] + ".png"
    }

    function formatClock(ms) {
        if (ms < 0) return "--:--"
        const clamped = Math.max(0, ms)
        const totalSec = Math.floor(clamped / 1000)
        const h = Math.floor(totalSec / 3600)
        const m = Math.floor((totalSec % 3600) / 60)
        const s = totalSec % 60
        const mmss = (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
        const base = h > 0 ? (h < 10 ? "0" : "") + h + ":" + mmss : mmss
        if (clamped < 10000) {
            const t = Math.floor((clamped % 1000) / 100)
            return base + "." + t
        }
        return base
    }

    // ── Chess board (left side) ─────────────────────────────────────────────
    ChessBoard {
        id: chessboard
        x: 0; y: 0
        squareSize:      mainWindow.squareSize
        boardTheme:      mainWindow.activeBoardTheme
        pieceTheme:      mainWindow.activePieceTheme
        pieceSizeFolder: mainWindow.pieceSizeFolder
        interactive:     appScreen === "playing" && chessboard.atLatestMove

        onPlayerResigned: function(loserColor) {
            mainWindow.resignedSide = loserColor
            chessboard.clearSavedGame()
        }
    }

    // ── Right panel ─────────────────────────────────────────────────────────
    Item {
        id: panel
        x: boardPx; y: 0
        width: panelW; height: boardPx

        Rectangle { anchors.fill: parent; color: "#252525" }

        // ══════════════════════════════════════════════════════════════════
        // MENU PANEL  (visible only in "menu" screen)
        // ══════════════════════════════════════════════════════════════════
        Item {
            id: menuPanel
            anchors.fill: parent
            visible: appScreen === "menu"

            // Title
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Math.round(squareSize * 0.6)
                text: "ntchess"
                font.pixelSize: Math.round(squareSize * 0.55)
                font.bold: true
                color: "#e0e0e0"
                font.letterSpacing: 2
            }

            // Centred menu buttons
            Column {
                anchors.centerIn: parent
                spacing: Math.round(squareSize * 0.28)
                width: parent.width - sp * 4

                Repeater {
                    model: [
                        { label: "New Game",  action: "newgame"                    },
                        { label: "Continue",  action: "continue", conditional: true },
                        { label: "Analysis",  action: "analysis"                   },
                        { label: "Settings",  action: "settings"                   }
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        visible: !modelData.conditional || chessboard.hasSavedGame
                        width:  parent.width
                        height: visible ? Math.round(squareSize * 0.78) : 0
                        radius: 6
                        color:  mBtnMa.containsMouse ? "#3d3d3d" : (modelData.action === "continue" ? "#2a3d2a" : "#303030")
                        border.color: mBtnMa.containsMouse ? "#555" : (modelData.action === "continue" ? "#4a7a4a" : "transparent")

                        Text {
                            anchors.centerIn: parent
                            text: parent.modelData.label
                            font.pixelSize: Math.round(squareSize * 0.28)
                            color: parent.modelData.action === "continue" ? "#8fca8f" : "#ddd"
                        }

                        MouseArea {
                            id: mBtnMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const a = parent.modelData.action
                                if (a === "newgame") {
                                    newGameDialog.open()
                                } else if (a === "continue") {
                                    const meta = chessboard.loadSavedGame()
                                    if (Object.keys(meta).length === 0) return
                                    mainWindow.resignedSide = -1
                                    const cs = meta.computerSide
                                    const we = meta.whiteElo
                                    const be = meta.blackElo
                                    mainWindow.gameComputerSide = cs
                                    chessboard.setEngineElos(we, be)
                                    if (cs === 0) {
                                        mainWindow.whitePlayerName = mainWindow.computerName
                                        mainWindow.blackPlayerName = "Player"
                                    } else if (cs === 1) {
                                        mainWindow.whitePlayerName = "Player"
                                        mainWindow.blackPlayerName = mainWindow.computerName
                                    } else {
                                        mainWindow.whitePlayerName = meta.whiteName
                                        mainWindow.blackPlayerName = meta.blackName
                                    }
                                    mainWindow.whitePlayerElo = we
                                    mainWindow.blackPlayerElo = be
                                    if (cs >= 0)
                                        chessboard.setComputerSide(cs)
                                    appScreen = "playing"
                                } else if (a === "settings") {
                                    settingsDialog.open()
                                } else if (a === "analysis") {
                                    appScreen = "analysis"
                                }
                            }
                        }
                    }
                }
            }
        }

        // ══════════════════════════════════════════════════════════════════
        // GAME PANEL  (visible only in "playing" screen)
        // ══════════════════════════════════════════════════════════════════
        Item {
            id: gamePanel
            anchors.fill: parent
            visible: appScreen === "playing"
            focus: visible

            onVisibleChanged: if (visible) forceActiveFocus()

            Keys.onLeftPressed:  if (chessboard.viewMoveIndex > 0) chessboard.stepBack()
            Keys.onRightPressed: if (!chessboard.atLatestMove)     chessboard.stepForward()

        // ── Top bar (floating overlay – does not consume layout space) ───
        Item {
            id: topBar
            anchors { top: parent.top; right: parent.right }
            width: Math.round(squareSize * 0.80)
            height: Math.round(squareSize * 0.78)
            z: 1

            // Hamburger / stripe menu button
            Item {
                id: stripeBtn
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: sp * 2 }
                width:  Math.round(squareSize * 0.30)
                height: Math.round(squareSize * 0.22)

                // three horizontal bars
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        x: 0
                        y: index * Math.round(stripeBtn.height / 2.5)
                        width:  stripeBtn.width
                        height: Math.round(squareSize * 0.04)
                        radius: height
                        color:  stripeMa.containsMouse ? "white" : "#aaa"
                    }
                }

                MouseArea {
                    id: stripeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: panelMenu.open()
                }

                Popup {
                    id: panelMenu
                    y: stripeBtn.height + 4
                    x: stripeBtn.width - width
                    width: Math.round(squareSize * 2.4)
                    padding: 0
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    background: Rectangle {
                        color: "#2a2a2a"
                        radius: 4
                        border.color: "#555"
                    }

                    Column {
                        width: panelMenu.width

                        Rectangle {
                            id: settingsRow
                            width: panelMenu.width
                            height: Math.round(squareSize * 0.35)
                            color: settingsMa.containsMouse ? "#3a3a3a" : "transparent"
                            Text { anchors.fill: parent; leftPadding: 8; text: "Settings"; color: "#ddd"; font.pixelSize: Math.round(squareSize * 0.175); verticalAlignment: Text.AlignVCenter }
                            MouseArea { id: settingsMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { panelMenu.close(); settingsDialog.open() } }
                        }
                        Rectangle {
                            id: backRow
                            width: panelMenu.width
                            height: Math.round(squareSize * 0.35)
                            color: backMa.containsMouse ? "#3a3a3a" : "transparent"
                            Text { anchors.fill: parent; leftPadding: 8; text: "Back to Menu"; color: "#ddd"; font.pixelSize: Math.round(squareSize * 0.175); verticalAlignment: Text.AlignVCenter }
                            MouseArea {
                                id: backMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    panelMenu.close()
                                    if (chessboard.gameStatus === "ongoing" && mainWindow.resignedSide < 0)
                                        chessboard.saveGame(
                                            mainWindow.whitePlayerName,
                                            mainWindow.blackPlayerName,
                                            mainWindow.gameComputerSide,
                                            mainWindow.whitePlayerElo,
                                            mainWindow.blackPlayerElo
                                        )
                                    chessboard.pauseForMenu()
                                    appScreen = "menu"
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Middle content: centred between topBar and bottomBar ──────────
        Item {
            id: middleArea
            anchors {
                top:    topBar.bottom
                bottom: bottomBar.top
                left:   parent.left
                right:  parent.right
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                spacing: 0

                // ── Black player bar ───────────────────────────────────────
                PlayerBar {
                    id: blackBar
                    width: parent.width
                    height: Math.round(squareSize * 1.1)
                    squareSize: mainWindow.squareSize
                    sp:         mainWindow.sp
                    capturesOnTop: true
                    isActive:    chessboard.currentTurn === 1 && chessboard.gameStatus === "ongoing"
                    hasClock:    chessboard.hasClock
                    remainingMs: chessboard.blackRemainingMs
                    capturedPieces:         chessboard.capturedByBlack
                    opponentCapturedPieces: chessboard.capturedByWhite
                    pieceSizeUrl:           smallPieceUrl
                    clockText:      mainWindow.formatClock(chessboard.blackRemainingMs)
                    playerName:     mainWindow.blackPlayerName
                    playerElo:      mainWindow.blackPlayerElo
                }

                // ── Move history ───────────────────────────────────────────
                Rectangle {
                    id: historyArea
                    width: parent.width - sp * 2
                    x: sp
                    height: Math.round(squareSize * 2.8)
                    color: "#1e1e1e"
                    radius: 4

                    ListView {
                        id: moveList
                        anchors { fill: parent; margins: 4 }
                        clip: true
                        model: Math.ceil(chessboard.moveHistorySan.length / 2)

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6
                                radius: 3
                                color: "#4a7ae8"
                            }
                            background: Rectangle { color: "#333"; radius: 3 }
                        }

                        onCountChanged: positionViewAtEnd()

                        delegate: Rectangle {
                            required property int index
                            width:  moveList.width - (moveList.ScrollBar.vertical.visible ? 10 : 0)
                            height: Math.round(squareSize * 0.32)
                            color: index % 2 === 0 ? "#272727" : "#222222"
                            radius: 2

                            property int moveNum:  index + 1
                            property string wMove: chessboard.moveHistorySan[index * 2]     ?? ""
                            property string bMove: chessboard.moveHistorySan[index * 2 + 1] ?? ""

                            readonly property int  viewIdx:  chessboard.viewMoveIndex
                            readonly property bool wActive: (index * 2)     === viewIdx - 1
                            readonly property bool bActive: (index * 2 + 1) === viewIdx - 1

                            RowLayout {
                                anchors { fill: parent; leftMargin: 6; rightMargin: 4 }
                                spacing: 4

                                Text {
                                    text: moveNum + "."
                                    color: "#666"
                                    font.pixelSize: Math.round(squareSize * 0.18)
                                    Layout.preferredWidth: Math.round(squareSize * 0.5)
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: parent.height - 4
                                    color: wActive ? "#3a5a3a" : "transparent"
                                    radius: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text: wMove
                                        color: "white"
                                        font.pixelSize: Math.round(squareSize * 0.18)
                                        font.bold: wActive
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: parent.height - 4
                                    color: bActive ? "#3a5a3a" : "transparent"
                                    radius: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text: bMove
                                        color: "#ddd"
                                        font.pixelSize: Math.round(squareSize * 0.18)
                                        font.bold: bActive
                                    }
                                }
                            }
                        }
                    }
                }

                // ── White player bar + result banner (tight together) ─────
                Column {
                    width: parent.width
                    spacing: 0

                PlayerBar {
                    id: whiteBar
                    width: parent.width
                    height: Math.round(squareSize * 1.1)
                    squareSize: mainWindow.squareSize
                    sp:         mainWindow.sp
                    isActive:    chessboard.currentTurn === 0 && chessboard.gameStatus === "ongoing"
                    hasClock:    chessboard.hasClock
                    remainingMs: chessboard.whiteRemainingMs
                    capturedPieces:         chessboard.capturedByWhite
                    opponentCapturedPieces: chessboard.capturedByBlack
                    pieceSizeUrl:           smallPieceUrl
                    clockText:      mainWindow.formatClock(chessboard.whiteRemainingMs)
                    playerName:     mainWindow.whitePlayerName
                    playerElo:      mainWindow.whitePlayerElo
                }

                // ── Result banner ──────────────────────────────────────────
                Item {
                    id: resultBar
                    width: parent.width
                    height: (chessboard.gameStatus !== "ongoing" || mainWindow.resignedSide >= 0)
                        ? Math.round(squareSize * 1.2) : 0
                    visible: height > 0

                    Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: {
                                const s = chessboard.gameStatus
                                const loser = chessboard.currentTurn
                                if (mainWindow.resignedSide >= 0) {
                                    const winner = mainWindow.resignedSide === 0 ? "Black" : "White"
                                    return winner + " wins by resignation"
                                }
                                if (s === "checkmate")       return (loser === 0 ? "Black" : "White") + " wins by checkmate"
                                if (s === "stalemate")       return "½–½  Stalemate"
                                if (s === "draw_repetition") return "½–½  Threefold repetition"
                                if (s === "draw_fifty")      return "½–½  Fifty-move rule"
                                if (s === "draw_material")   return "½–½  Insufficient material"
                                if (s === "timeout")         return (chessboard.currentTurn === 0 ? "Black" : "White") + " wins on time"
                                return ""
                            }
                            color: (chessboard.gameStatus === "checkmate" || chessboard.gameStatus === "timeout" || mainWindow.resignedSide >= 0) ? "#e8e8e8" : "#aaddaa"
                            font.pixelSize: Math.round(squareSize * 0.23)
                            font.bold: true
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width:  Math.round(squareSize * 1.4)
                            height: Math.round(squareSize * 0.32)
                            radius: 4
                            color:  copyPgnMa.containsMouse ? "#3a6a3a" : "#2e522e"
                            border.color: "#6ab46a"
                            border.width: 1

                            property bool copied: false

                            Text {
                                anchors.centerIn: parent
                                text: parent.copied ? "✓ Copied!" : "Copy PGN"
                                color: "white"
                                font.pixelSize: Math.round(squareSize * 0.17)
                            }

                            MouseArea {
                                id: copyPgnMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    const pgn = chessboard.gamePgn(
                                        mainWindow.whitePlayerName,
                                        mainWindow.blackPlayerName
                                    )
                                    chessboard.copyToClipboard(pgn)
                                    parent.copied = true
                                    copyPgnResetTimer.restart()
                                }
                            }

                            Timer {
                                id: copyPgnResetTimer
                                interval: 2000
                                onTriggered: parent.copied = false
                            }
                        }
                    }
                }
                } // inner Column (whiteBar + resultBar)
            } // Column
        } // middleArea


        // ── Bottom bar: nav + actions combined ────────────────────────────
        Rectangle {
            id: bottomBar
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: Math.round(squareSize * 0.78)
            color: "#222"

            RowLayout {
                anchors { fill: parent; leftMargin: sp; rightMargin: sp }
                spacing: 0

                // New game
                IconBtn {
                    text: "⊕"
                    tooltip: "New Game"
                    squareSize: mainWindow.squareSize
                    onBtnClicked: newGameDialog.open()
                    Layout.fillWidth: true
                }

                // Prev move
                IconBtn {
                    text: "←"
                    tooltip: "Previous move"
                    squareSize: mainWindow.squareSize
                    enabled: chessboard.viewMoveIndex > 0
                    holdRepeat: true
                    onBtnClicked: chessboard.stepBack()
                    Layout.fillWidth: true
                }

                // Next move
                IconBtn {
                    text: "→"
                    tooltip: "Next move"
                    squareSize: mainWindow.squareSize
                    enabled: !chessboard.atLatestMove
                    holdRepeat: true
                    onBtnClicked: chessboard.stepForward()
                    Layout.fillWidth: true
                }

                // Undo
                IconBtn {
                    text: "↩"
                    tooltip: "Take back"
                    squareSize: mainWindow.squareSize
                    enabled: chessboard.undoAllowed
                             && chessboard.atLatestMove
                             && chessboard.viewMoveIndex > 0
                             && chessboard.gameStatus === "ongoing"
                    onBtnClicked: chessboard.takeBack()
                    Layout.fillWidth: true
                }

                // Resign – hidden in CvC; disabled in HvC when it's the computer's turn
                IconBtn {
                    text: "🏳"
                    tooltip: "Resign"
                    squareSize: mainWindow.squareSize
                    visible: mainWindow.gameComputerSide !== 2
                    enabled: chessboard.gameStatus === "ongoing"
                        && (mainWindow.gameComputerSide < 0
                            || chessboard.currentTurn !== mainWindow.gameComputerSide)
                    onBtnClicked: resignConfirmPopup.open()
                    Layout.fillWidth: true
                }
            }
        }
        } // end gamePanel
    } // end panel

    // ── Analysis view ───────────────────────────────────────────────────────
    AnalysisView {
        id: analysisViewInst
        visible:         appScreen === "analysis"
        x: 0; y: 0
        squareSize:      mainWindow.squareSize
        pieceSizeFolder: mainWindow.pieceSizeFolder
        boardTheme:      mainWindow.activeBoardTheme
        pieceTheme:      mainWindow.activePieceTheme
        sp:              mainWindow.sp
        onBackToMenu:        appScreen = "menu"
        onSettingsRequested: settingsDialog.open()
    }

    // ── Resign confirmation ─────────────────────────────────────────────────
    Popup {
        id: resignConfirmPopup
        modal: true; focus: true
        anchors.centerIn: parent
        width: 280; padding: 24
        background: Rectangle { color: "#2a2a2a"; radius: 8; border.color: "#555" }
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Column {
            spacing: 20; width: parent.width
            Text {
                text: "Resign?"
                font.pixelSize: 16; font.bold: true; color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: {
                    // In HvC always name the human side
                    const cs = mainWindow.gameComputerSide
                    const side = (cs === 0) ? 1 : (cs === 1) ? 0 : chessboard.currentTurn
                    return (side === 0 ? "White" : "Black") + " will forfeit the game."
                }
                color: "#aaa"; font.pixelSize: 12; wrapMode: Text.WordWrap; width: parent.width
                horizontalAlignment: Text.AlignHCenter
            }
            Row {
                spacing: 12; anchors.horizontalCenter: parent.horizontalCenter
                Button {
                    text: "Cancel"; width: 100
                    onClicked: resignConfirmPopup.close()
                    background: Rectangle { color: parent.hovered ? "#555" : "#3a3a3a"; radius: 4; border.color: "#666" }
                    contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 13 }
                }
                Button {
                    text: "Resign"; width: 100
                    onClicked: { resignConfirmPopup.close(); chessboard.resign() }
                    background: Rectangle { color: parent.hovered ? "#aa4444" : "#884444"; radius: 4 }
                    contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 13 }
                }
            }
        }
    }

    // ── Settings dialog ─────────────────────────────────────────────────────
    SettingsDialog {
        id: settingsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        squareSize: mainWindow.squareSize
        onOpened: {
            boardTheme      = mainWindow.activeBoardTheme
            pieceTheme      = mainWindow.activePieceTheme
            enginePath      = chessboard.enginePath
            enginePathError = chessboard.enginePathError
            engineThreads   = chessboard.engineThreads
        }
        onApplied: {
            chessboard.setBoardTheme(settingsDialog.boardTheme)
            chessboard.setPieceTheme(settingsDialog.pieceTheme)
            mainWindow.activeBoardTheme = settingsDialog.boardTheme
            mainWindow.activePieceTheme = settingsDialog.pieceTheme
            chessboard.setEnginePath(settingsDialog.enginePath)
            chessboard.setEngineThreads(settingsDialog.engineThreads)
        }
    }

    // Update engine path error live while settings dialog is open
    Connections {
        target: chessboard
        function onEnginePathErrorChanged() {
            if (settingsDialog.visible)
                settingsDialog.enginePathError = chessboard.enginePathError
        }
        function onEngineNameChanged() {
            mainWindow.computerName = chessboard.engineName !== "" ? chessboard.engineName : "Computer"
            // Update running game player names if applicable
            if (mainWindow.whitePlayerName === "Computer" || mainWindow.whitePlayerName === mainWindow.computerName)
                mainWindow.whitePlayerName = mainWindow.computerName
            if (mainWindow.blackPlayerName === "Computer" || mainWindow.blackPlayerName === mainWindow.computerName)
                mainWindow.blackPlayerName = mainWindow.computerName
        }
        function onEngineNameBlackChanged() {
            if (chessboard.engineNameBlack !== "")
                mainWindow.blackPlayerName = chessboard.engineNameBlack
        }
        function onMovePlayed(wasCapture, isGameOver) {
            if (isGameOver) {
                chessboard.clearSavedGame()
            } else {
                chessboard.saveGame(
                    mainWindow.whitePlayerName,
                    mainWindow.blackPlayerName,
                    mainWindow.gameComputerSide,
                    mainWindow.whitePlayerElo,
                    mainWindow.blackPlayerElo
                )
            }
        }
    }

    // ── New game dialog ─────────────────────────────────────────────────────
    NewGameDialog {
        id: newGameDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        squareSize:   mainWindow.squareSize
        windowHeight: mainWindow.height
        onStartGame: function(mode, playerColor, timeMsW, incMsW, timeMsB, incMsB, fen, allowUndo, eloWhite, eloBlack) {
            chessboard.clearSavedGame()
            chessboard.newGame(fen)
            chessboard.undoAllowed = allowUndo
            mainWindow.resignedSide = -1
            if (timeMsW > 0 || timeMsB > 0)
                chessboard.configureClock(timeMsW, timeMsB, incMsW, incMsB)
            if (mode === 0) {         // Human vs Human
                mainWindow.gameComputerSide = -1
                chessboard.setComputerSide(-1)
                chessboard.setEngineElos(0, 0)
                mainWindow.whitePlayerName = "White"
                mainWindow.blackPlayerName = "Black"
                mainWindow.whitePlayerElo  = 0
                mainWindow.blackPlayerElo  = 0
            } else if (mode === 1) {  // Human vs Computer
                let humanColor = playerColor === 2
                    ? (Math.random() < 0.5 ? 0 : 1)
                    : playerColor
                let engineColor = 1 - humanColor
                let engineElo = eloWhite  // same value passed for both in HvC
                let wElo = (engineColor === 0) ? engineElo : 0
                let bElo = (engineColor === 1) ? engineElo : 0
                chessboard.setEngineElos(wElo, bElo)
                mainWindow.gameComputerSide = engineColor
                chessboard.setComputerSide(engineColor)
                if (humanColor === 0) {
                    mainWindow.whitePlayerName = "Player"
                    mainWindow.blackPlayerName = mainWindow.computerName
                } else {
                    mainWindow.whitePlayerName = mainWindow.computerName
                    mainWindow.blackPlayerName = "Player"
                }
                mainWindow.whitePlayerElo = wElo
                mainWindow.blackPlayerElo = bElo
            } else {                 // Computer vs Computer
                chessboard.setEngineElos(eloWhite, eloBlack)
                chessboard.setEnginePathWhiteGame(newGameDialog.whiteEnginePath)
                chessboard.setEnginePathBlack(newGameDialog.blackEnginePath)
                mainWindow.whitePlayerName = mainWindow.computerName
                mainWindow.blackPlayerName = mainWindow.computerName
                mainWindow.whitePlayerElo  = eloWhite
                mainWindow.blackPlayerElo  = eloBlack
                mainWindow.gameComputerSide = 2
                chessboard.setComputerSide(2)  // starts engines; onEngineNameBlackChanged will update blackPlayerName
            }
            appScreen = "playing"
        }
    }

// PlayerBar and IconBtn are now in PlayerBar.qml / IconBtn.qml
}

