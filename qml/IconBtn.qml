import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: ib
    property string text:       ""
    property string tooltip:    ""
    property bool   enabled:    true
    property int    squareSize: 64
    signal btnClicked()

    height: Math.round(squareSize * 0.65)
    Layout.fillWidth: true

    Rectangle {
        anchors.centerIn: parent
        width:  Math.round(squareSize * 0.65)
        height: Math.round(squareSize * 0.65)
        radius: Math.round(squareSize * 0.1)
        color:  ib.enabled && ibMa.containsMouse ? "#444" : "transparent"

        Text {
            anchors.centerIn: parent
            text:             ib.text
            font.pixelSize:   Math.round(squareSize * 0.32)
            color:            ib.enabled ? (ibMa.containsMouse ? "white" : "#bbb") : "#555"
        }
    }

    property bool holdRepeat: false

    Timer {
        id: holdTimer
        property bool fired: false
        interval: 400
        repeat: false
        onTriggered: {
            fired = true
            ib.btnClicked()
            holdTimer.interval = 80
            holdTimer.repeat = true
            holdTimer.start()
        }
    }

    MouseArea {
        id: ibMa
        anchors.fill: parent
        hoverEnabled: true
        enabled:      ib.enabled
        cursorShape:  ib.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked:    if (!ib.holdRepeat || !holdTimer.fired) ib.btnClicked()
        onPressed:    if (ib.holdRepeat && ib.enabled) { holdTimer.fired = false; holdTimer.start() }
        onReleased:   { holdTimer.stop(); holdTimer.interval = 400; holdTimer.repeat = false }
        onCanceled:   { holdTimer.stop(); holdTimer.interval = 400; holdTimer.repeat = false }
    }

    ToolTip.visible:  ib.tooltip !== "" && ibMa.containsMouse
    ToolTip.text:     ib.tooltip
    ToolTip.delay:    600
}
