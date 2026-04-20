import QtQuick
import QtQuick.Layouts

Item {
    id: pb

    property bool   isActive:      false
    property bool   hasClock:      false
    property int    remainingMs:   -1
    property var    capturedPieces: []
    property var    opponentCapturedPieces: []
    property var    pieceSizeUrl   // function(pid) -> url

    // spacing unit – caller must bind to the same value used in the parent window
    property int sp: 8

    // square size – needed for font / sizing calculations
    property int squareSize: 64

    // material value of a piece id (Q=9 R=5 B=3 N=3 P=1)
    function pieceValue(pid) {
        var v = [0,0,9,5,3,3,1,0,9,5,3,3,1]
        return (pid >= 0 && pid <= 12) ? v[pid] : 0
    }

    // net material advantage: my captures minus opponent captures
    property int materialDiff: {
        var s = 0
        for (var i = 0; i < capturedPieces.length; i++)         s += pieceValue(capturedPieces[i])
        for (var j = 0; j < opponentCapturedPieces.length; j++)  s -= pieceValue(opponentCapturedPieces[j])
        return s
    }
    property string clockText:     ""
    property string playerName:    ""
    property int    playerElo:     0
    property bool   capturesOnTop: false

    // Main layout column, vertically centred in the bar
    Column {
        anchors {
            left:           parent.left
            leftMargin:     sp
            right:          parent.right
            rightMargin:    sp
            verticalCenter: parent.verticalCenter
        }
        spacing: 3

        // Captured pieces ABOVE clock (used when capturesOnTop = true)
        Item {
            id: captureRowTop
            visible: pb.capturesOnTop
            width:  visible ? Math.round(squareSize * 1.7) : 0
            height: visible ? captureIconSzTop : 0

            readonly property int captureIconSzTop: Math.round(squareSize * 0.26)
            readonly property int captureStepTop:   Math.round(squareSize * 0.08)

            property var captureGroupsTop: {
                var order = [2,3,4,5,6,8,9,10,11,12]
                var counts = {}
                for (var i = 0; i < pb.capturedPieces.length; i++) {
                    var p = pb.capturedPieces[i]; counts[p] = (counts[p] || 0) + 1
                }
                var result = []
                for (var j = 0; j < order.length; j++) { var pid = order[j]; if (counts[pid]) result.push({pid: pid, count: counts[pid]}) }
                return result
            }

            Row {
                spacing: Math.round(squareSize * 0.01)
                anchors.verticalCenter: parent.verticalCenter
                Repeater {
                    model: captureRowTop.captureGroupsTop
                    delegate: Item {
                        id: groupItemT
                        required property var modelData
                        width:  captureRowTop.captureIconSzTop + Math.max(0, modelData.count - 1) * captureRowTop.captureStepTop
                        height: captureRowTop.captureIconSzTop
                        Repeater {
                            model: groupItemT.modelData.count
                            delegate: Image {
                                required property int index
                                x: index * captureRowTop.captureStepTop
                                width: captureRowTop.captureIconSzTop; height: captureRowTop.captureIconSzTop
                                source: pb.pieceSizeUrl(groupItemT.modelData.pid)
                                fillMode: Image.PreserveAspectFit; smooth: true
                            }
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: pb.materialDiff > 0
                    text: "+" + pb.materialDiff
                    color: "#aaa"; font.pixelSize: Math.round(squareSize * 0.19)
                    leftPadding: Math.round(squareSize * 0.03)
                }
            }
        }

        // Top row: clock box + name/elo side by side
        Row {
            spacing: 8
            // Clock box
            Rectangle {
                id: clockBox
                visible: pb.hasClock
                width:   clockTextItem.implicitWidth + Math.round(squareSize * 0.3)
                height:  Math.round(squareSize * 0.6)
                radius:  4
                readonly property bool lowTime: pb.isActive && pb.remainingMs >= 0 && pb.remainingMs < 10000
                color:        lowTime ? "#3a0a0a" : (pb.isActive ? "#1a3a1a" : "#1e1e1e")
                border.color: lowTime ? "#cc3333"  : (pb.isActive ? "#6ab46a" : "#444")
                border.width: pb.isActive ? 2 : 1

                Text {
                    id: clockTextItem
                    anchors.centerIn: parent
                    text:             pb.clockText
                    font.pixelSize:   Math.round(squareSize * 0.36)
                    font.family:      "Monospace"
                    font.bold:        pb.isActive
                    color:            clockBox.lowTime ? "#ff6666" : (pb.isActive ? "#c8f0c8" : "#888")
                }
            }

            // Name + elo, vertically centred next to clock
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text {
                    text: pb.playerName
                    color: pb.isActive ? "#e8e8e8" : "#aaa"
                    font.pixelSize: Math.round(squareSize * 0.22)
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    visible: pb.playerElo > 0
                    text: "(" + pb.playerElo + ")"
                    color: "#777"
                    font.pixelSize: Math.round(squareSize * 0.18)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Captured pieces BELOW clock (used when capturesOnTop = false)
        Item {
            id: captureRow
            visible: !pb.capturesOnTop
            width:  visible ? Math.round(squareSize * 1.7) : 0
            height: visible ? captureIconSz : 0

            readonly property int captureIconSz: Math.round(squareSize * 0.26)
            readonly property int captureStep:   Math.round(squareSize * 0.08)

            property var captureGroups: {
                var order = [2,3,4,5,6,8,9,10,11,12]
                var counts = {}
                for (var i = 0; i < pb.capturedPieces.length; i++) {
                    var p = pb.capturedPieces[i]
                    counts[p] = (counts[p] || 0) + 1
                }
                var result = []
                for (var j = 0; j < order.length; j++) {
                    var pid = order[j]
                    if (counts[pid]) result.push({pid: pid, count: counts[pid]})
                }
                return result
            }

            Row {
                spacing: Math.round(squareSize * 0.01)
                anchors.verticalCenter: parent.verticalCenter

                Repeater {
                    model: captureRow.captureGroups
                    delegate: Item {
                        id: groupItem
                        required property var modelData
                        width:  captureRow.captureIconSz + Math.max(0, modelData.count - 1) * captureRow.captureStep
                        height: captureRow.captureIconSz

                        Repeater {
                            model: groupItem.modelData.count
                            delegate: Image {
                                required property int index
                                x: index * captureRow.captureStep
                                width:  captureRow.captureIconSz
                                height: captureRow.captureIconSz
                                source: pb.pieceSizeUrl(groupItem.modelData.pid)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                            }
                        }
                    }
                }

                // +N material advantage label
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: pb.materialDiff > 0
                    text:    "+" + pb.materialDiff
                    color:   "#aaa"
                    font.pixelSize: Math.round(squareSize * 0.19)
                    font.bold: false
                    leftPadding: Math.round(squareSize * 0.03)
                }
            }
        }
    }
}
