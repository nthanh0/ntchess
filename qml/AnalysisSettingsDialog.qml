import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ── AnalysisSettingsDialog ──────────────────────────────────────────────────
// Popup for analysis-specific settings.
//
// Exposed properties (two-way via parent bindings):
//   showBestMoveArrow  – bool  – draw an arrow for the top engine move
//   maxEngineDepth     – int   – 0 = unlimited, otherwise capped at this depth
// ---------------------------------------------------------------------------
Popup {
    id: root

    // ── settings values ──────────────────────────────────────────────────
    property bool showBestMoveArrow: true
    property int  maxEngineDepth:    0        // 0 = unlimited

    // ── sizing (mirrors ImportDialog) ────────────────────────────────────
    property int squareSize: 64
    readonly property int dialogW:   Math.round(squareSize * 8 * 0.44)
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

    // ── layout ────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing:      sp12

        // Title
        Text {
            text:                "Analysis Settings"
            color:               "white"
            font.pixelSize:      root.fntTitle
            font.bold:           true
            Layout.alignment:    Qt.AlignHCenter
            Layout.bottomMargin: sp8
        }

        // Divider
        Rectangle { height: 1; color: "#555"; Layout.fillWidth: true }

        // ── Show best move arrow ─────────────────────────────────────────
        RowLayout {
            spacing: sp8
            Layout.fillWidth: true

            Text {
                text:            "Show best move arrow"
                color:           "#ccc"
                font.pixelSize:  root.fntNormal
                Layout.fillWidth: true
            }

            Switch {
                id:       arrowSwitch
                checked:  root.showBestMoveArrow
                onToggled: root.showBestMoveArrow = checked

                // Compact dark-themed track + handle
                indicator: Rectangle {
                    implicitWidth:  Math.round(root.rowH * 1.2)
                    implicitHeight: Math.round(root.rowH * 0.60)
                    radius:         height / 2
                    color:          arrowSwitch.checked ? "#4caf50" : "#555"
                    border.color:   arrowSwitch.checked ? "#388e3c" : "#444"
                    border.width:   1

                    Rectangle {
                        x:       arrowSwitch.checked
                                     ? parent.width  - width  - root.sp4
                                     : root.sp4
                        anchors.verticalCenter: parent.verticalCenter
                        width:   Math.round(parent.height * 0.70)
                        height:  width
                        radius:  width / 2
                        color:   "white"

                        Behavior on x { NumberAnimation { duration: 120 } }
                    }
                }
                contentItem: Item {}   // suppress default label (we use our own Text)
            }
        }

        Item { height: sp4 }

        // ── Max engine depth ─────────────────────────────────────────────
        Text {
            text:           "Max engine depth"
            color:          "#ccc"
            font.pixelSize: root.fntNormal
        }

        RowLayout {
            spacing: sp8
            Layout.fillWidth: true

            Slider {
                id:               depthSlider
                Layout.fillWidth: true
                from:             0
                to:               40
                stepSize:         1
                value:            root.maxEngineDepth
                snapMode:         Slider.SnapAlways
                onMoved:          root.maxEngineDepth = Math.round(value)

                // Give the Slider padding equal to half the handle width so
                // the handle sits flush at the endpoints and mouse tracking works.
                readonly property int handleSz: Math.round(root.rowH * 0.58)
                leftPadding:  handleSz / 2
                rightPadding: handleSz / 2

                background: Rectangle {
                    x:      depthSlider.leftPadding
                    y:      depthSlider.topPadding + depthSlider.availableHeight / 2 - height / 2
                    width:  depthSlider.availableWidth
                    height: Math.round(root.sp4 * 2)
                    radius: height / 2
                    color:  "#444"

                    Rectangle {
                        width:  depthSlider.visualPosition * parent.width
                        height: parent.height
                        radius: parent.radius
                        color:  "#4caf50"
                    }
                }

                handle: Rectangle {
                    x: depthSlider.leftPadding
                       + depthSlider.visualPosition * depthSlider.availableWidth
                       - width / 2
                    y: depthSlider.topPadding + depthSlider.availableHeight / 2 - height / 2
                    implicitWidth:  depthSlider.handleSz
                    implicitHeight: depthSlider.handleSz
                    radius: width / 2
                    color:  depthSlider.pressed ? "#ddd" : "white"
                    border.color: "#888"
                    border.width: 1
                }
            }

            Text {
                text:           root.maxEngineDepth === 0 ? "∞" : root.maxEngineDepth
                color:          "#ccc"
                font.pixelSize: root.fntNormal
                font.bold:      true
                horizontalAlignment: Text.AlignRight
                Layout.minimumWidth: Math.round(root.sz * 0.5)
            }
        }

        Text {
            text:           root.maxEngineDepth === 0
                                ? "No depth limit — engine searches indefinitely"
                                : "Engine stops after reaching depth " + root.maxEngineDepth
            color:          "#666"
            font.pixelSize: root.fntLabel
            wrapMode:       Text.WordWrap
            Layout.fillWidth: true
        }

        Item { height: sp4 }

        // Divider
        Rectangle { height: 1; color: "#555"; Layout.fillWidth: true }

        // ── Close button ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: sp4

            Item { Layout.fillWidth: true }

            Rectangle {
                width:  Math.round(root.dialogW * 0.28)
                height: root.rowH
                radius: Math.round(root.sp4 * 1.5)
                color:  closeHover.containsMouse ? "#3a3a3a" : "#2e2e2e"
                border.color: "#666"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text:            "Close"
                    color:           "white"
                    font.pixelSize:  root.fntNormal
                }
                HoverHandler { id: closeHover }
                TapHandler  { onTapped: root.close() }
            }
        }
    }
}
