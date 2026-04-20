import QtQuick
import QtQuick.Controls
import QtMultimedia
import ntchess

Item {
    id: root

    // ── public API ─────────────────────────────────────────────────────────
    // Actual display size per square – a clean fraction of screen height
    property int squareSize: { const s = [32,64,96,128,256,512,1024]; const t = Screen.height / 12; for (let i = 0; i < s.length; i++) if (s[i] >= t) return s[i]; return s[s.length-1] }

    // Choose the best-resolution image folder for the current squareSize
    property string pieceSizeFolder: {
        const sizes = [32, 64, 96, 128, 256, 512, 1024]
        let best = sizes[0], bestDiff = Math.abs(squareSize - best)
        for (let sz of sizes) {
            const d = Math.abs(squareSize - sz)
            if (d < bestDiff) { best = sz; bestDiff = d }
        }
        return best + "x" + best
    }
    property string boardTheme : "green"
    property string pieceTheme : "cburnett"
    property bool   interactive: true          // set false to block user moves
    property int    computerSide: -1           // -1=none, 0=white, 1=black

    width : squareSize * 8
    height: squareSize * 8

    // ── expose bridge data to parent ────────────────────────────────────────
    readonly property alias moveHistorySan:   bridge.moveHistorySan
    readonly property alias capturedByWhite:  bridge.capturedByWhite
    readonly property alias capturedByBlack:  bridge.capturedByBlack
    readonly property alias currentTurn:      bridge.currentTurn
    readonly property alias hasClock:         bridge.hasClock
    readonly property alias whiteRemainingMs: bridge.whiteRemainingMs
    readonly property alias blackRemainingMs: bridge.blackRemainingMs
    readonly property alias engineName:       bridge.engineName
    readonly property alias engineNameBlack:  bridge.engineNameBlack
    readonly property alias enginePath:       bridge.enginePath
    readonly property alias enginePathError:  bridge.enginePathError
    readonly property alias engineThreads:    bridge.engineThreads
    readonly property alias engineEloWhite:   bridge.engineEloWhite
    readonly property alias engineEloBlack:   bridge.engineEloBlack
    readonly property alias atLatestMove:     bridge.atLatestMove
    readonly property alias viewMoveIndex:    bridge.viewMoveIndex
    property alias undoAllowed:               bridge.undoAllowed

    // Analysis / eval bar
    readonly property alias analysisEvalCp:     bridge.analysisEvalCp
    readonly property alias analysisEvalMate:   bridge.analysisEvalMate
    readonly property alias analysisEvalMateIn: bridge.analysisEvalMateIn
    readonly property alias analysisTopLines:   bridge.analysisTopLines
    readonly property alias analysisDepth:      bridge.analysisDepth
    readonly property alias analysisEngineName: bridge.analysisEngineName
    function startAnalysis() { bridge.startAnalysis() }
    function stopAnalysis()  { bridge.stopAnalysis()  }

    function stepBack()    { root.selectedSquare = -1; root.legalTargets = []; bridge.stepBack() }
    function stepForward() { root.selectedSquare = -1; root.legalTargets = []; bridge.stepForward() }
    function takeBack()    { root.selectedSquare = -1; root.legalTargets = []; bridge.takeBack() }
    function initAsAnalysisBridge()  { bridge.initAsAnalysisBridge() }
    function saveAnalysisPosition()  { bridge.saveAnalysisPosition() }

    // ── Persistent game helpers ────────────────────────────────────────────
    readonly property alias hasSavedGame: bridge.hasSavedGame
    function saveGame(wn, bn, cs, we, be) { bridge.saveGame(wn, bn, cs, we, be) }
    function loadSavedGame()              { return bridge.loadSavedGame() }
    function clearSavedGame()             { bridge.clearSavedGame() }
    function pauseForMenu()               { bridge.pauseForMenu() }
    function savedBoardTheme()            { return bridge.savedBoardTheme() }
    function savedPieceTheme()            { return bridge.savedPieceTheme() }

    // ── Forwarded signals ─────────────────────────────────────────────────
    signal movePlayed(bool wasCapture, bool isGameOver)
    signal playerResigned(int loserColor)

    // ── backend ────────────────────────────────────────────────────────────
    GameBridge {
        id: bridge
        boardTheme: root.boardTheme
        pieceTheme: root.pieceTheme
        onMovePlayed: function(wasCapture, isGameOver) {
            root.movePlayed(wasCapture, isGameOver)
            if (isGameOver)        gameEndSound.play()
            else if (wasCapture)   captureSound.play()
            else                   moveSound.play()
        }
        onPlayerResigned: function(loserColor) {
            root.playerResigned(loserColor)
        }
    }

    // ── sounds ─────────────────────────────────────────────────────────────
    SoundEffect { id: moveSound;    source: assetsPath + "/sound/Move.wav" }
    SoundEffect { id: captureSound; source: assetsPath + "/sound/Capture.wav" }
    SoundEffect { id: gameEndSound; source: assetsPath + "/sound/GameEnd.wav" }

    // ── helpers ────────────────────────────────────────────────────────────
    readonly property var pieceFileName: [
        "",    // 0 – NO_PIECE
        "wK",  // 1 – W_KING
        "wQ",  // 2 – W_QUEEN
        "wR",  // 3 – W_ROOK
        "wB",  // 4 – W_BISHOP
        "wN",  // 5 – W_KNIGHT
        "wP",  // 6 – W_PAWN
        "bK",  // 7 – B_KING
        "bQ",  // 8 – B_QUEEN
        "bR",  // 9 – B_ROOK
        "bB",  // 10 – B_BISHOP
        "bN",  // 11 – B_KNIGHT
        "bP"   // 12 – B_PAWN
    ]

    function squareIndex(col, row) { return (7 - row) * 8 + col }

    // ── board coordinate colours ───────────────────────────────────────────
    // Returns { onLight, onDark } text colours matching the current board theme.
    // Text drawn on a light square uses onLight; text on a dark square uses onDark.
    function coordColors() {
        const map = {
            "blue":          { onLight: "#6f8fa9", onDark: "#d0d9e8" },
            "blue-marble":   { onLight: "#5b7fa0", onDark: "#ccd8e8" },
            "blue2":         { onLight: "#5b7fa0", onDark: "#cee3f8" },
            "blue3":         { onLight: "#4a6f90", onDark: "#bed5e8" },
            "brown":         { onLight: "#b58863", onDark: "#f0d9b5" },
            "canvas2":       { onLight: "#8a7560", onDark: "#dfd3c0" },
            "green":         { onLight: "#769656", onDark: "#ffffdd" },
            "green-plastic": { onLight: "#5a8060", onDark: "#ddf0dd" },
            "grey":          { onLight: "#777777", onDark: "#dddddd" },
            "horsey":        { onLight: "#7c6840", onDark: "#f0deb4" },
            "ic":            { onLight: "#705c2e", onDark: "#fffacd" },
            "leather":       { onLight: "#8a6040", onDark: "#e8c89a" },
            "maple":         { onLight: "#9c6030", onDark: "#f8d8a8" },
            "maple2":        { onLight: "#9c6030", onDark: "#f8d8a8" },
            "marble":        { onLight: "#888888", onDark: "#eeeeee" },
            "metal":         { onLight: "#606060", onDark: "#d0d0d0" },
            "olive":         { onLight: "#7a8a50", onDark: "#e8e8c0" },
            "pink-pyramid":  { onLight: "#b06080", onDark: "#f8d8e8" },
            "purple":        { onLight: "#7050a0", onDark: "#e8d8f8" },
            "purple-diag":   { onLight: "#7050a0", onDark: "#e8d8f8" },
            "wood":          { onLight: "#8a6040", onDark: "#f0d8b0" },
            "wood2":         { onLight: "#8a6040", onDark: "#f0d8b0" },
            "wood3":         { onLight: "#8a6040", onDark: "#f0d8b0" },
            "wood4":         { onLight: "#8a6040", onDark: "#f0d8b0" },
        }
        return map[root.boardTheme] ?? { onLight: "rgba(0,0,0,0.50)", onDark: "rgba(255,255,255,0.65)" }
    }

    // Returns the coordinate text colour for a square at display (col, row).
    // Light square: (col+row)%2===0; dark square: (col+row)%2===1
    function coordColor(col, row) {
        const cc = coordColors()
        return (col + row) % 2 === 0 ? cc.onLight : cc.onDark
    }

    function pieceImageUrl(pieceId) {
        if (pieceId <= 0) return ""
        return assetsPath + "/piece/" + root.pieceTheme + "/"
             + root.pieceSizeFolder + "/" + pieceFileName[pieceId] + ".png"
    }

    // Display column (0-7) and row (0-7, 0=top) for a board square index
    function sqDisplayCol(sq) { return sq % 8 }
    function sqDisplayRow(sq) { return 7 - Math.floor(sq / 8) }
    function sqPixelX(sq)     { return sqDisplayCol(sq) * root.squareSize }
    function sqPixelY(sq)     { return sqDisplayRow(sq) * root.squareSize }

    // Convert pixel position to board square index (-1 if out of bounds)
    function pixelToSquare(px, py) {
        const col = Math.floor(px / root.squareSize)
        const row = Math.floor(py / root.squareSize)
        if (col < 0 || col > 7 || row < 0 || row > 7) return -1
        return squareIndex(col, row)
    }

    function isOwnPiece(pieceId) {
        if (bridge.currentTurn === 0) return pieceId >= 1 && pieceId <= 6
        return pieceId >= 7 && pieceId <= 12
    }

    // ── interaction state ───────────────────────────────────────────────────
    property int  selectedSquare: -1
    property var  legalTargets:   []
    property bool gameOver:       bridge.gameStatus !== "ongoing" || bridge.resigned

    // last move highlight
    property int lastMoveFrom: -1
    property int lastMoveTo:   -1

    // drag state
    property bool dragging:    false
    property int  dragFromSq:  -1
    property int  dragPieceId: 0
    property real dragX:       0
    property real dragY:       0

    // slide animation state
    property bool animating:   false
    property int  animFromSq:  -1
    property int  animToSq:    -1
    property int  animPieceId: 0

    // ── board background ───────────────────────────────────────────────────
    Image {
        id: boardImage
        anchors.fill: parent
        source: assetsPath + "/board/" + root.boardTheme + "/" + root.boardTheme + ".png"
        fillMode: Image.Stretch
        cache: false
    }

    // ── square overlays (last-move / selection / legal-move hints) ──────────
    Grid {
        anchors.fill: parent
        rows: 8
        columns: 8

        Repeater {
            model: 64
            delegate: Rectangle {
                required property int index
                width:  root.squareSize
                height: root.squareSize

                property int sq: squareIndex(index % 8, Math.floor(index / 8))

                // Last-move highlight (subtle yellow)
                color: (sq === bridge.lastMoveFrom || sq === bridge.lastMoveTo)
                       ? "#44eecc00"
                       : (sq === root.selectedSquare ? "#55ffdd00" : "transparent")

                // Check highlight – faint red on the king's square
                Rectangle {
                    visible: bridge.checkedKingSquare === parent.sq
                    anchors.fill: parent
                    color: "#55ee1111"
                }

                // Legal-move dot (empty square)
                Rectangle {
                    visible: root.legalTargets.indexOf(parent.sq) >= 0
                             && bridge.pieces[parent.sq] === 0
                    anchors.centerIn: parent
                    width:  parent.width  * 0.32
                    height: parent.height * 0.32
                    radius: width / 2
                    color: "#44000000"
                }

                // Capture ring (occupied square)
                Rectangle {
                    visible: root.legalTargets.indexOf(parent.sq) >= 0
                             && bridge.pieces[parent.sq] !== 0
                    anchors.centerIn: parent
                    width:  parent.width  * 0.9
                    height: parent.height * 0.9
                    radius: width / 2
                    color: "transparent"
                    border.color: "#66000000"
                    border.width: parent.width * 0.08
                }
            }
        }
    }

    // ── board coordinates (rank numbers + file letters) ────────────────────
    Grid {
        anchors.fill: parent
        rows: 8
        columns: 8

        Repeater {
            model: 64
            delegate: Item {
                required property int index
                width:  root.squareSize
                height: root.squareSize

                property int col: index % 8         // display column 0=a … 7=h
                property int row: Math.floor(index / 8) // display row 0=top … 7=bottom
                property int rank: 8 - row           // chess rank 1-8
                property string file: "abcdefgh"[col]

                // Rank number: top-left corner of the a-file squares (col==0)
                Text {
                    visible: col === 0
                    text: parent.rank
                    font.pixelSize: Math.max(9, root.squareSize * 0.19)
                    font.bold: true
                    color: coordColor(parent.col, parent.row)
                    anchors { top: parent.top; left: parent.left; margins: root.squareSize * 0.05 }
                }

                // File letter: bottom-right corner of the rank-1 squares (row==7)
                Text {
                    visible: row === 7
                    text: parent.file
                    font.pixelSize: Math.max(9, root.squareSize * 0.19)
                    font.bold: true
                    color: coordColor(parent.col, parent.row)
                    anchors { bottom: parent.bottom; right: parent.right; margins: root.squareSize * 0.05 }
                }
            }
        }
    }

    // ── pieces ─────────────────────────────────────────────────────────────
    Grid {
        id: pieceGrid
        anchors.fill: parent
        rows: 8
        columns: 8

        Repeater {
            model: 64
            delegate: Item {
                required property int index
                width:  root.squareSize
                height: root.squareSize

                property int sq:      squareIndex(index % 8, Math.floor(index / 8))
                property int pieceId: bridge.pieces[sq] ?? 0

                Image {
                    anchors.fill: parent
                    source: pieceImageUrl(parent.pieceId)
                    // Hide while being dragged, or hide src/dst squares during slide animation
                    visible: parent.pieceId > 0
                             && !(root.dragging  && parent.sq === root.dragFromSq)
                             && !(root.animating && (parent.sq === root.animFromSq
                                                  || parent.sq === root.animToSq))
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    cache: true
                }
            }
        }
    }

    // ── sliding animation piece ─────────────────────────────────────────────
    Image {
        id: slidingPiece
        width:  root.squareSize
        height: root.squareSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: true
        visible: root.animating
        z: 10
        // No x/y bindings here – position is set exclusively by startSlide()
        // to avoid the binding fighting the NumberAnimation (which causes ghosting)

        NumberAnimation on x {
            id: slideAnimX
            duration: 150
            easing.type: Easing.OutCubic
        }
        NumberAnimation on y {
            id: slideAnimY
            duration: 150
            easing.type: Easing.OutCubic
            onFinished: root.animating = false
        }

        function startSlide(fromSq, toSq) {
            slideAnimX.stop()
            slideAnimY.stop()
            slidingPiece.source = pieceImageUrl(root.animPieceId)
            slidingPiece.x = sqPixelX(fromSq)
            slidingPiece.y = sqPixelY(fromSq)
            slideAnimX.from = sqPixelX(fromSq)
            slideAnimX.to   = sqPixelX(toSq)
            slideAnimY.from = sqPixelY(fromSq)
            slideAnimY.to   = sqPixelY(toSq)
            slideAnimX.start()
            slideAnimY.start()
        }
    }

    // ── best-move arrow overlay ────────────────────────────────────────────
    property bool showBestMoveArrow: true

    // Best-move square indices fed from outside (AnalysisView)
    property int  bestMoveFrom: -1
    property int  bestMoveTo:   -1

    // Max engine depth (0 = unlimited).  Setting this forwards to the bridge.
    property int  maxEngineDepth: 0
    onMaxEngineDepthChanged: bridge.setAnalysisMaxDepth(maxEngineDepth)

    Canvas {
        id: arrowCanvas
        anchors.fill: parent
        visible: root.showBestMoveArrow && root.bestMoveFrom >= 0 && root.bestMoveTo >= 0
        z: 15  // above pieces, below dragged piece

        // Redraw whenever the relevant inputs change
        onVisibleChanged: requestPaint()
        Connections {
            target: root
            function onBestMoveFromChanged() { arrowCanvas.requestPaint() }
            function onBestMoveToChanged()   { arrowCanvas.requestPaint() }
            function onShowBestMoveArrowChanged() { arrowCanvas.requestPaint() }
        }

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.showBestMoveArrow || root.bestMoveFrom < 0 || root.bestMoveTo < 0) return

            const ss   = root.squareSize
            const half = ss / 2

            // Centre of each square
            const x1 = sqPixelX(root.bestMoveFrom) + half
            const y1 = sqPixelY(root.bestMoveFrom) + half
            const x2 = sqPixelX(root.bestMoveTo)   + half
            const y2 = sqPixelY(root.bestMoveTo)    + half

            const dx = x2 - x1
            const dy = y2 - y1
            const len = Math.sqrt(dx * dx + dy * dy)
            if (len < 1) return

            const ux = dx / len
            const uy = dy / len

            // Arrow geometry
            const shaftW    = ss * 0.14
            const headLen   = ss * 0.38
            const headWidth = ss * 0.68
            // Shorten shaft so it doesn't overdraw the arrowhead
            const shaftEnd  = len - headLen

            const px = -uy, py = ux  // perpendicular unit vector

            ctx.globalAlpha = 0.42
            ctx.fillStyle   = "#3a5a3a"
            ctx.lineJoin    = "round"

            ctx.beginPath()
            // Shaft rectangle corners
            ctx.moveTo(x1 + px * shaftW / 2,          y1 + py * shaftW / 2)
            ctx.lineTo(x1 + ux * shaftEnd + px * shaftW / 2,
                       y1 + uy * shaftEnd + py * shaftW / 2)
            // Arrowhead
            ctx.lineTo(x1 + ux * shaftEnd + px * headWidth / 2,
                       y1 + uy * shaftEnd + py * headWidth / 2)
            ctx.lineTo(x2, y2)
            ctx.lineTo(x1 + ux * shaftEnd - px * headWidth / 2,
                       y1 + uy * shaftEnd - py * headWidth / 2)
            ctx.lineTo(x1 + ux * shaftEnd - px * shaftW / 2,
                       y1 + uy * shaftEnd - py * shaftW / 2)
            ctx.lineTo(x1 - px * shaftW / 2, y1 - py * shaftW / 2)
            ctx.closePath()
            ctx.fill()
        }
    }

    // ── dragged piece (follows cursor) ──────────────────────────────────────
    Image {
        id: draggedPiece
        width:  root.squareSize
        height: root.squareSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: true
        visible: root.dragging
        z: 20
        source: root.dragging ? pieceImageUrl(root.dragPieceId) : ""
        x: root.dragX - width  / 2
        y: root.dragY - height / 2
    }

    // ── unified input handler ───────────────────────────────────────────────
    MouseArea {
        anchors.fill: parent
        hoverEnabled: false

        property real pressX: 0
        property real pressY: 0
        property bool mayDrag: false
        // true when selection happened in this press – prevents onReleased from
        // immediately deselecting the piece on the same mouse-up
        property bool justSelected: false
        readonly property real dragThreshold: 6

        onPressed: function(mouse) {
            if (!root.interactive || root.gameOver || root.animating) return
            // Block input when it's the computer's turn
            if (root.computerSide === 2) return
            if (root.computerSide >= 0 && bridge.currentTurn === root.computerSide) return
            pressX = mouse.x
            pressY = mouse.y
            mayDrag = false
            justSelected = false

            const sq = root.pixelToSquare(mouse.x, mouse.y)
            if (sq < 0) return
            const piece = bridge.pieces[sq]

            // ── second click on a legal target → click-to-move with animation ──
            if (root.selectedSquare >= 0 && sq !== root.selectedSquare
                    && root.legalTargets.indexOf(sq) >= 0) {
                root.executeMove(root.selectedSquare, sq, true)
                return
            }

            // ── own piece: select and prepare for potential drag ──────────────
            if (root.isOwnPiece(piece)) {
                const reselect = (sq === root.selectedSquare)
                root.selectedSquare = sq
                root.legalTargets   = bridge.legalMovesFrom(sq)
                root.dragFromSq     = sq
                root.dragPieceId    = piece
                root.dragX = mouse.x
                root.dragY = mouse.y
                mayDrag = true
                if (!reselect) justSelected = true  // fresh selection – don't deselect on release
            } else {
                root.selectedSquare = -1
                root.legalTargets   = []
            }
        }

        onPositionChanged: function(mouse) {
            if (!mayDrag) return
            const dx = mouse.x - pressX
            const dy = mouse.y - pressY
            if (!root.dragging && (Math.abs(dx) > dragThreshold || Math.abs(dy) > dragThreshold))
                root.dragging = true
            if (root.dragging) {
                root.dragX = mouse.x
                root.dragY = mouse.y
            }
        }

        onReleased: function(mouse) {
            if (!mayDrag) return
            const wasDragging = root.dragging
            root.dragging = false
            mayDrag = false

            const toSq = root.pixelToSquare(mouse.x, mouse.y)

            if (wasDragging) {
                // ── drag-and-drop: no slide animation ─────────────────────────
                if (toSq >= 0 && toSq !== root.dragFromSq
                        && root.legalTargets.indexOf(toSq) >= 0) {
                    root.executeMove(root.dragFromSq, toSq, false)
                }
                // else: invalid drop → selection stays, user can still click a target
            } else {
                // ── pure click release ─────────────────────────────────────────
                if (justSelected) {
                    // We just selected a piece; leave it selected
                    justSelected = false
                    return
                }
                justSelected = false
                // Clicking the already-selected piece again → deselect
                if (toSq === root.selectedSquare) {
                    root.selectedSquare = -1
                    root.legalTargets   = []
                }
            }
        }
    }

    // ── promotion state ────────────────────────────────────────────────────
    property int  _promoFromSq:   -1
    property int  _promoToSq:     -1
    property bool _promoAnimate:  false

    // ── move execution ──────────────────────────────────────────────────────
    // withAnimation: true for click-to-move, false for drag-and-drop
    function executeMove(fromSq, toSq, withAnimation) {
        const piece = bridge.pieces[fromSq]
        // Detect promotion: white pawn (6) to rank 8 (sq 56-63), black pawn (12) to rank 1 (sq 0-7)
        const isPromo = (piece === 6 && toSq >= 56) || (piece === 12 && toSq < 8)
        if (isPromo) {
            root._promoFromSq  = fromSq
            root._promoToSq    = toSq
            root._promoAnimate = withAnimation
            promotionPicker.sideColor = (piece === 6) ? 0 : 1
            promotionPicker.promoCol  = toSq % 8
            promotionPicker.open()
            root.selectedSquare = -1
            root.legalTargets   = []
            root.dragFromSq     = -1
            return
        }
        root._commitMove(fromSq, toSq, 2 /*QUEEN default, unused*/, withAnimation)
    }

    function _commitMove(fromSq, toSq, promotionPieceType, withAnimation) {
        const piece = bridge.pieces[fromSq]
        if (withAnimation) {
            root.animFromSq  = fromSq
            root.animToSq    = toSq
            root.animPieceId = piece
            root.animating   = true
        }

        bridge.makeMove(fromSq, toSq, promotionPieceType)

        root.lastMoveFrom   = fromSq
        root.lastMoveTo     = toSq
        root.selectedSquare = -1
        root.legalTargets   = []
        root.dragFromSq     = -1

        if (withAnimation)
            slidingPiece.startSlide(fromSq, toSq)
    }

    // ── promotion picker ────────────────────────────────────────────────────
    Item {
        id: promotionPicker
        property int sideColor: 0   // 0=white, 1=black
        property int promoCol:  0   // board column 0-7

        // PieceType values: QUEEN=2,ROOK=3,BISHOP=4,KNIGHT=5
        readonly property var pieceTypes:  [2, 3, 4, 5]
        // Piece enum IDs for images
        readonly property var whitePieces: [2, 3, 4, 5]   // wQ,wR,wB,wN
        readonly property var blackPieces: [8, 9, 10, 11] // bQ,bR,bB,bN

        anchors.fill: parent
        visible: false
        z: 100

        function open()  { visible = true  }
        function close() { visible = false }

        // Semi-transparent dim over whole board
        Rectangle {
            anchors.fill: parent
            color: "#66000000"
        }

        // Click anywhere (including dim) → cancel
        MouseArea {
            anchors.fill: parent
            onClicked: promotionPicker.close()
        }

        // The 4 piece squares, stacked in-column
        Repeater {
            model: 4
            delegate: Rectangle {
                required property int index
                // White: stack downward from row 0; Black: stack upward from row 7
                property int displayRow: promotionPicker.sideColor === 0
                                         ? index
                                         : (7 - index)

                x: promotionPicker.promoCol * root.squareSize
                y: displayRow * root.squareSize
                width:  root.squareSize
                height: root.squareSize

                color: "transparent"
                border.color: "transparent"
                border.width: 0
                radius: 0

                // Circular backing (Lichess-style)
                Rectangle {
                    anchors.centerIn: parent
                    width:  parent.width  * 0.92
                    height: parent.height * 0.92
                    radius: width / 2
                    color:  pieceHover.containsMouse ? "#cc4a8a4a" : "#cc1a1a1a"
                    border.color: pieceHover.containsMouse ? "#6ab46a" : "#555"
                    border.width: 1
                }

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: {
                        const ids = promotionPicker.sideColor === 0
                            ? promotionPicker.whitePieces
                            : promotionPicker.blackPieces
                        const names = ["","wK","wQ","wR","wB","wN","wP",
                                       "bK","bQ","bR","bB","bN","bP"]
                        return assetsPath + "/piece/" + root.pieceTheme
                             + "/" + root.pieceSizeFolder + "/"
                             + names[ids[index]] + ".png"
                    }
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                MouseArea {
                    id: pieceHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        mouse.accepted = true
                        promotionPicker.close()
                        root._commitMove(root._promoFromSq, root._promoToSq,
                                         promotionPicker.pieceTypes[index],
                                         root._promoAnimate)
                    }
                }
            }
        }
    }

    // ── public methods ──────────────────────────────────────────────────────
    function newGame(fen) {
        bridge.newGame(fen || "")
        root.selectedSquare = -1
        root.legalTargets   = []
        root.lastMoveFrom   = -1
        root.lastMoveTo     = -1
        root.animating      = false
        root.dragging       = false
    }
    function setComputerSide(color) { root.computerSide = color; bridge.setComputerSide(color) }
    function setBoardTheme(t)    { bridge.boardTheme = t }
    function setPieceTheme(t)    { bridge.pieceTheme = t }
    function configureClock(wMs, bMs, wIncMs, bIncMs) { bridge.configureClock(wMs, bMs, wIncMs, bIncMs) }
    function resign()            { bridge.resign() }
    function gamePgn(w, b)       { return bridge.gamePgn(w || "?", b || "?") }
    function loadPgn(pgn)        { return bridge.loadPgn(pgn) }
    function loadFen(fen)        { newGame(fen) }
    function setEnginePath(p)           { bridge.setEnginePath(p) }
    function setEnginePathBlack(p)      { bridge.setEnginePathBlack(p) }
    function setEnginePathWhiteGame(p)  { bridge.setEnginePathWhiteGame(p) }
    function setEngineThreads(n)   { bridge.setEngineThreads(n) }
    function setEngineElos(w, b)   { bridge.setEngineElos(w, b) }
    function copyToClipboard(text) { bridge.copyToClipboard(text) }

    readonly property string gameStatus: bridge.gameStatus
    readonly property string statusText: {
        switch(bridge.gameStatus) {
        case "checkmate":        return (bridge.currentTurn === 0 ? "Black" : "White") + " wins by checkmate"
        case "stalemate":        return "Draw – Stalemate"
        case "draw_repetition":  return "Draw – Repetition"
        case "draw_fifty":       return "Draw – 50-move rule"
        case "draw_material":    return "Draw – Insufficient material"
        case "timeout":          return (bridge.currentTurn === 0 ? "Black" : "White") + " wins on time"
        default:                 return bridge.currentTurn === 0 ? "White to move" : "Black to move"
        }
    }

    signal resigned(int loserColor)
    Connections {
        target: bridge
        function onPlayerResigned(loserColor) { root.resigned(loserColor) }
    }
}
