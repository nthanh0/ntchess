import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── ImportDialog ────────────────────────────────────────────────────────────
// Popup that lets the user paste a FEN string and/or a PGN string.
// If both are provided, PGN takes priority.
//
// Signals:
//   importRequested(string text, bool isPgn)
// ---------------------------------------------------------------------------
Popup {
    id: root

    signal importRequested(string text, bool isPgn)

    // ── sizing ──────────────────────────────────────────────────────────────
    property int squareSize: 64
    readonly property int dialogW:   Math.round(squareSize * 8 * 0.52)
    readonly property int sz:        Math.round(dialogW / 5.5)

    readonly property int fntTitle:  Math.round(sz * 0.31)
    readonly property int fntNormal: Math.round(sz * 0.20)
    readonly property int fntLabel:  Math.round(sz * 0.19)
    readonly property int rowH:      Math.round(sz * 0.56)
    readonly property int sp20:      Math.round(sz * 0.31)
    readonly property int sp12:      Math.round(sz * 0.19)
    readonly property int sp8:       Math.round(sz * 0.125)
    readonly property int sp4:       Math.round(sz * 0.0625)

    modal:       true
    focus:       true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    width:   dialogW
    padding: Math.round(sz * 0.44)

    background: Rectangle {
        color:        "#2a2a2a"
        radius:       Math.round(sz * 0.16)
        border.color: "#555"
        border.width: 1
    }

    // ── layout ───────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing:      sp12

        // Title
        Text {
            text:                   "Import Position"
            color:                  "white"
            font.pixelSize:         root.fntTitle
            font.bold:              true
            Layout.alignment:       Qt.AlignHCenter
            Layout.bottomMargin:    sp8
        }

        // Divider
        Rectangle { height: 1; color: "#555"; Layout.fillWidth: true }

        // ── FEN row ─────────────────────────────────────────────────────────
        Text {
            text:           "FEN"
            color:          "#aaa"
            font.pixelSize: root.fntLabel
        }
        TextField {
            id:                  fenField
            placeholderText:     "Paste FEN string…"
            Layout.fillWidth:    true
            font.pixelSize:      root.fntNormal
            color:               "white"
            placeholderTextColor:"#666"
            leftPadding:         sp8
            rightPadding:        sp8
            background: Rectangle {
                color:        fenField.activeFocus ? "#3a3a3a" : "#303030"
                radius:       sp4
                border.color: fenField.activeFocus ? "#888" : "#555"
                border.width: 1
            }
            Keys.onReturnPressed: importAction()
        }

        Item { height: sp4 }

        // ── PGN row ─────────────────────────────────────────────────────────
        RowLayout {
            spacing: sp8
            Text {
                text:           "PGN"
                color:          "#aaa"
                font.pixelSize: root.fntLabel
            }
            Text {
                text:           "(preferred when both are filled)"
                color:          "#666"
                font.pixelSize: root.fntLabel - 1
            }
        }
        ScrollView {
            Layout.fillWidth:  true
            Layout.preferredHeight: Math.round(root.sz * 2.2)
            clip: true

            TextArea {
                id:                  pgnField
                placeholderText:     "Paste PGN here…"
                font.pixelSize:      root.fntNormal
                color:               "white"
                placeholderTextColor:"#666"
                wrapMode:            TextArea.Wrap
                leftPadding:         sp8
                rightPadding:        sp8
                topPadding:          sp8
                bottomPadding:       sp8
                background: Rectangle {
                    color:        pgnField.activeFocus ? "#3a3a3a" : "#303030"
                    radius:       sp4
                    border.color: pgnField.activeFocus ? "#888" : "#555"
                    border.width: 1
                }
            }
        }

        // ── Error label (shown on bad import) ────────────────────────────────
        Text {
            id:             errorLabel
            text:           ""
            color:          "#e05555"
            font.pixelSize: root.fntLabel
            visible:        text.length > 0
            wrapMode:       Text.WordWrap
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true; Layout.minimumHeight: sp4 }

        // Divider
        Rectangle { height: 1; color: "#555"; Layout.fillWidth: true }

        // ── Buttons ───────────────────────────────────────────────────────────
        RowLayout {
            spacing: sp12
            Layout.topMargin: sp4

            Button {
                text:             "Cancel"
                Layout.fillWidth: true
                height:           root.rowH
                onClicked:        root.close()
                background: Rectangle {
                    color:        parent.hovered ? "#555" : "#3a3a3a"
                    radius:       root.sp4
                    border.color: "#666"
                    border.width: 1
                }
                contentItem: Text {
                    text:               parent.text
                    color:              "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment:   Text.AlignVCenter
                    font.pixelSize:     root.fntNormal
                }
            }

            Button {
                text:             "Import"
                Layout.fillWidth: true
                height:           root.rowH
                onClicked:        importAction()
                background: Rectangle {
                    color:  parent.hovered ? "#5a9a5a" : "#4a8a4a"
                    radius: root.sp4
                }
                contentItem: Text {
                    text:               parent.text
                    color:              "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment:   Text.AlignVCenter
                    font.pixelSize:     root.fntNormal
                }
            }
        }
    }

    // ── Logic ────────────────────────────────────────────────────────────────
    function importAction() {
        errorLabel.text = ""
        const pgn = pgnField.text.trim()
        const fen = fenField.text.trim()

        if (pgn.length > 0) {
            root.importRequested(pgn, true)
        } else if (fen.length > 0) {
            root.importRequested(fen, false)
        } else {
            errorLabel.text = "Please enter a FEN or PGN to import."
        }
    }

    function showError(msg) {
        errorLabel.text = msg
    }

    // Clear fields and error when re-opened
    onOpened: {
        fenField.text  = ""
        pgnField.text  = ""
        errorLabel.text = ""
        pgnField.forceActiveFocus()
    }
}
