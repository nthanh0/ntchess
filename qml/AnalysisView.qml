import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── AnalysisView ────────────────────────────────────────────────────────────
// Full-layout analysis screen:
//   ChessBoard | EvalBar | Panel (player bars + move history + ← → nav)
//
// Signals:
//   backToMenu()        – user chose "Back to Menu" from hamburger
//   settingsRequested() – user chose "Settings" from hamburger
// ---------------------------------------------------------------------------
Item {
    id: analysisView

    // ── inputs from the parent window ──────────────────────────────────────
    property int    squareSize:      64
    property string pieceSizeFolder: "64x64"
    property string boardTheme:      "brown"
    property string pieceTheme:      "cburnett"
    property int    sp:              12   // small spacing unit

    // eval score in centipawns (white-positive). 0 = balanced.
    // Driven by the background analysis engine; falls back to 0 when unavailable.
    property real evalCp: {
        var s = analysisBoard.gameStatus
        if (s === "checkmate" || s === "timeout")
            return analysisBoard.currentTurn === 0 ? -10000 : 10000
        if (s === "stalemate" || s === "draw_repetition" ||
                s === "draw_fifty" || s === "draw_material")
            return 0
        if (analysisBoard.analysisEvalMate)
            return analysisBoard.analysisEvalMateIn >= 0 ? 10000 : -10000
        return analysisBoard.analysisEvalCp
    }

    // ── signals ────────────────────────────────────────────────────────────
    signal backToMenu()
    signal settingsRequested()

    // ── Analysis settings (controlled by AnalysisSettingsDialog) ──────────
    property bool showBestMoveArrow: true
    property int  maxEngineDepth:    0        // 0 = unlimited

    // Best-move squares from the top engine line (-1 = none)
    readonly property int bestMoveFrom: {
        const l = analysisBoard.analysisTopLines
        return (l && l.length > 0) ? (l[0].move_from ?? -1) : -1
    }
    readonly property int bestMoveTo: {
        const l = analysisBoard.analysisTopLines
        return (l && l.length > 0) ? (l[0].move_to ?? -1) : -1
    }

    // ── derived sizes ──────────────────────────────────────────────────────
    readonly property int boardPx:   squareSize * 8
    readonly property int evalBarW:  Math.round(squareSize * 0.25)
    readonly property int panelW:    Math.max(260, Math.round(squareSize * 4.5))

    width:  boardPx + evalBarW + panelW
    height: boardPx

    // ── helpers ────────────────────────────────────────────────────────────
    function smallPieceUrl(pid) {
        if (pid <= 0) return ""
        const names = ["","wK","wQ","wR","wB","wN","wP","bK","bQ","bR","bB","bN","bP"]
        return assetsPath + "/piece/" + analysisView.pieceTheme + "/32x32/" + names[pid] + ".png"
    }

    // Clamp eval to display range (±10 pawns = ±1000 cp)
    // Returns white fill fraction 0..1
    function evalFrac() {
        const clamped = Math.max(-1000, Math.min(1000, evalCp))
        return 0.5 + clamped / 2000.0
    }

    // Format a line's eval as a displayable string.
    function lineEvalText(line) {
        if (line.score_mate) {
            const n = line.mate_in
            return n > 0 ? "#" + n : "#-" + (-n)
        }
        const cp = line.score_cp
        const pawns = (cp / 100.0).toFixed(2)
        return cp >= 0 ? "+" + pawns : pawns
    }

    // ── Chess board ─────────────────────────────────────────────────────────
    ChessBoard {
        id: analysisBoard
        x: 0; y: 0
        squareSize:      analysisView.squareSize
        boardTheme:      analysisView.boardTheme
        pieceTheme:      analysisView.pieceTheme
        pieceSizeFolder: analysisView.pieceSizeFolder
        interactive:     true
        // no engine in analysis
        showBestMoveArrow: analysisView.showBestMoveArrow
        bestMoveFrom:      analysisView.bestMoveFrom
        bestMoveTo:        analysisView.bestMoveTo
        maxEngineDepth:    analysisView.maxEngineDepth
    }

    // ── Eval bar ─────────────────────────────────────────────────────────────
    // A flush rectangle abutting the board on the left and the panel on the right.
    // Black portion (top) + white portion (bottom), animated.
    Rectangle {
        id: evalBarContainer
        x:      analysisView.boardPx
        y:      0
        width:  analysisView.evalBarW
        height: analysisView.boardPx
        color:  "#333333"   // default / black side fill

        // White fill (grows from the bottom)
        Rectangle {
            id: whiteFill
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: parent.height * analysisView.evalFrac()
            color:  "white"
            Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        }
    }

    // ── Right panel ─────────────────────────────────────────────────────────
    Item {
        id: panel
        x: analysisView.boardPx + analysisView.evalBarW
        y: 0
        width:  analysisView.panelW
        height: analysisView.boardPx

        Rectangle { anchors.fill: parent; color: "#252525" }

        // ── Top bar: hamburger menu ───────────────────────────────────────
        Item {
            id: topBar
            anchors { top: parent.top; right: parent.right }
            width:  Math.round(analysisView.squareSize * 0.80)
            height: Math.round(analysisView.squareSize * 0.78)
            z: 1

            Item {
                id: stripeBtn
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: analysisView.sp * 2 }
                width:  Math.round(analysisView.squareSize * 0.30)
                height: Math.round(analysisView.squareSize * 0.22)

                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        x: 0
                        y: index * Math.round(stripeBtn.height / 2.5)
                        width:  stripeBtn.width
                        height: Math.round(analysisView.squareSize * 0.04)
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
                    width: Math.round(analysisView.squareSize * 2.4)
                    padding: 0
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                    background: Rectangle {
                        color: "#2a2a2a"; radius: 4; border.color: "#555"
                    }

                    Column {
                        width: panelMenu.width

                        Rectangle {
                            width: panelMenu.width
                            height: Math.round(analysisView.squareSize * 0.35)
                            color: settingsMa.containsMouse ? "#3a3a3a" : "transparent"
                            Text { anchors.fill: parent; leftPadding: 8; text: "Settings"; color: "#ddd"; font.pixelSize: Math.round(analysisView.squareSize * 0.175); verticalAlignment: Text.AlignVCenter }
                            MouseArea { id: settingsMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { panelMenu.close(); analysisView.settingsRequested() } }
                        }
                        Rectangle {
                            width: panelMenu.width
                            height: Math.round(analysisView.squareSize * 0.35)
                            color: backMa.containsMouse ? "#3a3a3a" : "transparent"
                            Text { anchors.fill: parent; leftPadding: 8; text: "Back to Menu"; color: "#ddd"; font.pixelSize: Math.round(analysisView.squareSize * 0.175); verticalAlignment: Text.AlignVCenter }
                            MouseArea { id: backMa; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { panelMenu.close(); analysisView.backToMenu() } }
                        }
                    }
                }
            }
        }

        // ── Top lines (MultiPV) ───────────────────────────────────
        Item {
            id: topLinesArea
            anchors {
                top:   topBar.bottom
                left:  parent.left
                right: parent.right
            }
            // header row + 3 line rows + small vertical padding
            readonly property int lineH:   Math.round(analysisView.squareSize * 0.40)
            readonly property int headerH: Math.round(analysisView.squareSize * 0.30)
            readonly property int padV:    Math.round(analysisView.squareSize * 0.08)
            height: headerH + lineH * 3 + padV * 2

            Rectangle {
                anchors { fill: parent; leftMargin: analysisView.sp; rightMargin: analysisView.sp }
                color:        "#1a1a1a"
                radius:       4
                border.color: "#333"
                border.width: 1

                Column {
                    anchors {
                        fill:         parent
                        topMargin:    topLinesArea.padV
                        bottomMargin: topLinesArea.padV
                        leftMargin:   1
                        rightMargin:  1
                    }
                    spacing: 0

                    // ── Engine name + depth header ──────────────────────
                    Item {
                        width:  parent.width
                        height: topLinesArea.headerH

                        RowLayout {
                            anchors {
                                fill:        parent
                                leftMargin:  Math.round(analysisView.squareSize * 0.12)
                                rightMargin: Math.round(analysisView.squareSize * 0.12)
                            }
                            spacing: Math.round(analysisView.squareSize * 0.10)

                            Text {
                                text:             analysisBoard.analysisEngineName || "Engine"
                                color:            "#888"
                                font.pixelSize:   Math.round(analysisView.squareSize * 0.17)
                                elide:            Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                readonly property int d: analysisBoard.analysisDepth
                                text:                d > 0 ? "depth " + d : ""
                                color:               "#555"
                                font.pixelSize:      Math.round(analysisView.squareSize * 0.17)
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }

                    // thin separator
                    Rectangle {
                        width:  parent.width - 2
                        x:      1
                        height: 1
                        color:  "#2e2e2e"
                    }

                    Repeater {
                        model: 3

                        delegate: Rectangle {
                            required property int index
                            width:  parent.width
                            height: topLinesArea.lineH
                            color:  "transparent"

                            // Grab the line data (may be missing while engine is warming up)
                            readonly property var lineData: analysisBoard.analysisTopLines[index] ?? null

                            RowLayout {
                                anchors {
                                    fill:        parent
                                    leftMargin:  Math.round(analysisView.squareSize * 0.12)
                                    rightMargin: Math.round(analysisView.squareSize * 0.12)
                                }
                                spacing: Math.round(analysisView.squareSize * 0.12)

                                // Rank badge (1 / 2 / 3)
                                Text {
                                    text:           (parent.parent.index + 1) + "."
                                    color:          "#666"
                                    font.pixelSize: Math.round(analysisView.squareSize * 0.17)
                                    Layout.preferredWidth: Math.round(analysisView.squareSize * 0.22)
                                }

                                // Eval badge
                                Rectangle {
                                    readonly property bool isWhiteAdv: !lineData || (!lineData.score_mate && lineData.score_cp >= 0)
                                                                     || (lineData.score_mate && lineData.mate_in > 0)
                                    Layout.preferredWidth:  Math.round(analysisView.squareSize * 0.90)
                                    Layout.preferredHeight: Math.round(analysisView.squareSize * 0.30)
                                    Layout.alignment:       Qt.AlignVCenter
                                    color:        isWhiteAdv ? "white" : "#303030"
                                    radius:       3
                                    border.color: isWhiteAdv ? "#bbb" : "#555"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text:           lineData ? analysisView.lineEvalText(lineData) : "…"
                                        color:          parent.isWhiteAdv ? "#111" : "#ddd"
                                        font.pixelSize: Math.round(analysisView.squareSize * 0.175)
                                        font.bold:      true
                                    }
                                }

                                // SAN continuation
                                Text {
                                    text:           lineData ? lineData.pv_san : ""
                                    color:          "#ccc"
                                    font.pixelSize: Math.round(analysisView.squareSize * 0.175)
                                    font.family:    "monospace"
                                    elide:          Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Middle: player bars + move history ───────────────────────────
        Item {
            id: middleArea
            anchors {
                top:    topLinesArea.bottom
                bottom: bottomBar.top
                left:   parent.left
                right:  parent.right
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                spacing: 0

                // Black player bar
                PlayerBar {
                    id: blackBar
                    width:      parent.width
                    height:     Math.round(analysisView.squareSize * 1.1)
                    squareSize: analysisView.squareSize
                    sp:         analysisView.sp
                    capturesOnTop: true
                    isActive:   false
                    hasClock:   false
                    capturedPieces:         analysisBoard.capturedByBlack
                    opponentCapturedPieces: analysisBoard.capturedByWhite
                    pieceSizeUrl:           analysisView.smallPieceUrl
                    playerName: "Black"
                }

                // Move history – same fixed height as the playing screen
                Rectangle {
                    id: historyArea
                    width:  parent.width - analysisView.sp * 2
                    x:      analysisView.sp
                    height: Math.round(analysisView.squareSize * 2.8)
                    color:  "#1e1e1e"
                    radius: 4

                    ListView {
                        id: moveList
                        anchors { fill: parent; margins: 4 }
                        clip: true
                        model: Math.ceil(analysisBoard.moveHistorySan.length / 2)

                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 6; radius: 3; color: "#4a7ae8"
                            }
                            background: Rectangle { color: "#333"; radius: 3 }
                        }

                        onCountChanged: positionViewAtEnd()

                        delegate: Rectangle {
                            required property int index
                            width:  moveList.width - (moveList.ScrollBar.vertical.visible ? 10 : 0)
                            height: Math.round(analysisView.squareSize * 0.32)
                            color:  index % 2 === 0 ? "#272727" : "#222222"
                            radius: 2

                            property int    moveNum: index + 1
                            property string wMove:   analysisBoard.moveHistorySan[index * 2]     ?? ""
                            property string bMove:   analysisBoard.moveHistorySan[index * 2 + 1] ?? ""

                            // viewMoveIndex is 1-based (number of half-moves played to reach that display state)
                            readonly property int  viewIdx:  analysisBoard.viewMoveIndex
                            readonly property bool wActive: (index * 2)     === viewIdx - 1
                            readonly property bool bActive: (index * 2 + 1) === viewIdx - 1

                            RowLayout {
                                anchors { fill: parent; leftMargin: 6; rightMargin: 4 }
                                spacing: 4

                                Text {
                                    text: moveNum + "."
                                    color: "#666"
                                    font.pixelSize: Math.round(analysisView.squareSize * 0.18)
                                    Layout.preferredWidth: Math.round(analysisView.squareSize * 0.5)
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: parent.height - 4
                                    color:  wActive ? "#3a5a3a" : "transparent"
                                    radius: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text:             wMove
                                        color:            "white"
                                        font.pixelSize:   Math.round(analysisView.squareSize * 0.18)
                                        font.bold:        wActive

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape:  Qt.PointingHandCursor
                                            onClicked: {
                                                const target = index * 2 + 1  // 1-based half-move count
                                                const cur    = analysisBoard.viewMoveIndex
                                                const diff   = target - cur
                                                if (diff > 0)      for (var i = 0; i < diff;  i++) analysisBoard.stepForward()
                                                else if (diff < 0) for (var j = 0; j < -diff; j++) analysisBoard.stepBack()
                                            }
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: parent.height - 4
                                    color:  bActive ? "#3a5a3a" : "transparent"
                                    radius: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text:             bMove
                                        color:            "#ddd"
                                        font.pixelSize:   Math.round(analysisView.squareSize * 0.18)
                                        font.bold:        bActive

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape:  Qt.PointingHandCursor
                                            onClicked: {
                                                const target = index * 2 + 2
                                                const cur    = analysisBoard.viewMoveIndex
                                                const diff   = target - cur
                                                if (diff > 0)      for (var i = 0; i < diff;  i++) analysisBoard.stepForward()
                                                else if (diff < 0) for (var j = 0; j < -diff; j++) analysisBoard.stepBack()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // White player bar
                PlayerBar {
                    id: whiteBar
                    width:      parent.width
                    height:     Math.round(analysisView.squareSize * 1.1)
                    squareSize: analysisView.squareSize
                    sp:         analysisView.sp
                    isActive:   false
                    hasClock:   false
                    capturedPieces:         analysisBoard.capturedByWhite
                    opponentCapturedPieces: analysisBoard.capturedByBlack
                    pieceSizeUrl:           analysisView.smallPieceUrl
                    playerName: "White"
                }
            }
        }

        // ── Bottom bar: eval | nav | … | import | settings ───────────────
        Rectangle {
            id: bottomBar
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: Math.round(analysisView.squareSize * 0.78)
            color:  "#222"

            RowLayout {
                anchors { fill: parent; leftMargin: analysisView.sp; rightMargin: analysisView.sp }
                spacing: 0

                // ── Eval chip ──────────────────────────────────────────
                Rectangle {
                    id: evalChip
                    Layout.preferredWidth:  Math.round(analysisView.squareSize * 1.1)
                    Layout.preferredHeight: Math.round(analysisView.squareSize * 0.46)
                    Layout.alignment:       Qt.AlignVCenter
                    color:   evalCp >= 0 ? "white" : "#333333"
                    radius:  4
                    border.color: evalCp >= 0 ? "#ccc" : "#444"
                    border.width: 1

                    Text {
                        id: evalLabel
                        anchors.centerIn: parent
                        text: {
                            var s = analysisBoard.gameStatus
                            if (s === "checkmate" || s === "timeout") {
                                // side to move is the loser
                                return analysisBoard.currentTurn === 0 ? "0-1" : "1-0"
                            }
                            if (s === "stalemate" || s === "draw_repetition" ||
                                    s === "draw_fifty" || s === "draw_material") {
                                return "½-½"
                            }
                            if (analysisBoard.analysisEvalMate) {
                                var m = analysisBoard.analysisEvalMateIn
                                return (m >= 0 ? "M" : "-M") + Math.abs(m)
                            }
                            var cp = analysisBoard.analysisEvalCp
                            if (Math.abs(cp) < 10) return "0.0"
                            var pawns = (Math.abs(cp) / 100).toFixed(1)
                            return (cp < 0 ? "-" : "+") + pawns
                        }
                        color: evalCp >= 0 ? "#111" : "#eee"
                        font.pixelSize: Math.round(analysisView.squareSize * 0.22)
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // ── Separator ──────────────────────────────────────────
                Rectangle {
                    Layout.preferredWidth:  1
                    Layout.preferredHeight: Math.round(analysisView.squareSize * 0.46)
                    Layout.alignment:       Qt.AlignVCenter
                    // Left gap: sp*0.75 + half of (IconBtn width - icon rect width) so both
                    // sides are visually equal (the "←" icon is centred inside its container)
                    Layout.leftMargin:      Math.round((analysisView.squareSize * 1.2 - analysisView.squareSize * 0.65) / 2)
                    Layout.rightMargin:     0
                    color: "#444"
                }

                // ── Previous move ──────────────────────────────────────
                IconBtn {
                    text:       "←"
                    tooltip:    "Previous move"
                    squareSize: analysisView.squareSize
                    enabled:    analysisBoard.viewMoveIndex > 0
                    holdRepeat: true
                    onBtnClicked: analysisBoard.stepBack()
                    Layout.preferredWidth: Math.round(analysisView.squareSize * 1.2)
                }

                // ── Next move ──────────────────────────────────────────
                IconBtn {
                    text:       "→"
                    tooltip:    "Next move"
                    squareSize: analysisView.squareSize
                    enabled:    !analysisBoard.atLatestMove
                    holdRepeat: true
                    onBtnClicked: analysisBoard.stepForward()
                    Layout.preferredWidth: Math.round(analysisView.squareSize * 1.2)
                }

                // ── Reset to start ────────────────────────────────────
                IconBtn {
                    text:       "⟳"
                    tooltip:    "Reset to starting position"
                    squareSize: analysisView.squareSize
                    enabled:    analysisBoard.moveHistorySan.length > 0 || analysisBoard.viewMoveIndex >= 0
                    onBtnClicked: resetConfirmDialog.open()
                    Layout.preferredWidth: Math.round(analysisView.squareSize * 1.0)
                }

                // ── filler ─────────────────────────────────────────────
                Item { Layout.fillWidth: true }

                // ── Import button ──────────────────────────────────────
                IconBtn {
                    id: importBtn
                    text:       "⤒"
                    tooltip:    "Import"
                    squareSize: analysisView.squareSize
                    onBtnClicked: importDialog.open()
                    Layout.preferredWidth: Math.round(analysisView.squareSize * 1.0)
                }

                // ── Analysis settings ─────────────────────────────────
                IconBtn {
                    id: settingsBtn
                    text:       "⚙"
                    tooltip:    "Analysis settings"
                    squareSize: analysisView.squareSize
                    onBtnClicked: analysisSettingsDialog.open()
                    Layout.preferredWidth: Math.round(analysisView.squareSize * 1.0)
                }
            }
        }
    }

    // ── Keyboard navigation ─────────────────────────────────────────────────
    Keys.onLeftPressed:  if (analysisBoard.viewMoveIndex > 0)          analysisBoard.stepBack()
    Keys.onRightPressed: if (!analysisBoard.atLatestMove)               analysisBoard.stepForward()
    focus: true

    // ── Analysis engine lifecycle ────────────────────────────────────────────
    // Start/stop the engine based on visibility so it doesn't run in the
    // background while the user is on the menu or playing screen.
    onVisibleChanged: {
        if (visible) {
            forceActiveFocus()
            analysisBoard.startAnalysis()
        } else {
            analysisBoard.stopAnalysis()
            analysisBoard.saveAnalysisPosition()
        }
    }
    Component.onCompleted: analysisBoard.initAsAnalysisBridge()
    Component.onDestruction: { analysisBoard.stopAnalysis(); analysisBoard.saveAnalysisPosition() }

    // ── Analysis settings dialog ────────────────────────────────────────────
    AnalysisSettingsDialog {
        id:          analysisSettingsDialog
        parent:      Overlay.overlay
        anchors.centerIn: parent
        squareSize:  analysisView.squareSize

        showBestMoveArrow: analysisView.showBestMoveArrow
        maxEngineDepth:    analysisView.maxEngineDepth

        onShowBestMoveArrowChanged: analysisView.showBestMoveArrow = showBestMoveArrow
        onMaxEngineDepthChanged:    analysisView.maxEngineDepth    = maxEngineDepth
    }

    // ── Reset confirm dialog ────────────────────────────────────────────────
    Dialog {
        id:          resetConfirmDialog
        parent:      Overlay.overlay
        anchors.centerIn: parent
        title:       "Reset board?"
        modal:       true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            text: "Reset to starting position? All moves will be lost."
            wrapMode: Text.WordWrap
            width: parent ? parent.width : implicitWidth
        }
        onAccepted: {
            analysisBoard.newGame()
            analysisBoard.startAnalysis()
        }
    }

    // ── Import dialog ───────────────────────────────────────────────────────
    ImportDialog {
        id:          importDialog
        parent:      Overlay.overlay
        anchors.centerIn: parent
        squareSize:  analysisView.squareSize

        onImportRequested: function(text, isPgn) {
            if (isPgn) {
                const ok = analysisBoard.loadPgn(text)
                if (!ok) {
                    importDialog.showError("Could not parse PGN \u2014 please check the text and try again.")
                } else {
                    importDialog.close()
                    analysisBoard.startAnalysis()
                }
            } else {
                analysisBoard.loadFen(text)
                importDialog.close()
                analysisBoard.startAnalysis()
            }
        }
    }
}
