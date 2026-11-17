import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtMultimedia

Rectangle {
    id: root
    color: "#131317"
    border.color: isFocused ? theme.accent : (mouseArea.containsMouse ? "#3b4252" : "#1e1e24")
    border.width: isFocused ? 2 : 1
    radius: Math.min(6, theme.radius)
    clip: true

    property var modelData: null
    property bool isFocused: false

    readonly property var cam: modelData || ({})
    readonly property string streamUrl: isFocused ? (cam.mainUrl || "") : (cam.streamUrl || "")
    readonly property int channel: cam.channel || 0
    readonly property string camName: cam.name || ""
    readonly property string camIp: cam.ip || ""

    MediaPlayer {
        id: player
        source: root.streamUrl
        audioOutput: null
        loops: MediaPlayer.Infinite

        onErrorOccurred: (error, errorString) => {
            if (root.streamUrl !== "") {
                console.log("Stream error ch " + root.channel + ":", errorString)
                retryTimer.start()
            }
        }

        Component.onCompleted: {
            if (root.streamUrl !== "") play()
        }
    }

    Connections {
        target: root
        function onStreamUrlChanged() {
            if (root.streamUrl !== "") {
                player.play()
            } else {
                player.stop()
            }
        }
    }

    Timer {
        id: retryTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (player.playbackState !== MediaPlayer.PlayingState) {
                player.play()
            }
        }
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    // Loading / Standby overlay when video is connecting
    Rectangle {
        anchors.fill: parent
        color: "#18181f"
        visible: player.playbackState !== MediaPlayer.PlayingState

        Column {
            anchors.centerIn: parent
            spacing: 10

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: parent.visible
                width: 36
                height: 36
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Connecting Ch %1...").arg(root.channel)
                color: "#888899"
                font.pixelSize: 12
            }
        }
    }

    // Top Header info bar (Channel badge + Name + IP)
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 34
        color: Qt.rgba(0, 0, 0, 0.72)

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // Channel badge
            Rectangle {
                width: 24
                height: 20
                radius: Math.min(3, theme.radius)
                color: theme.accent
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: root.channel < 10 ? "0" + root.channel : "" + root.channel
                    color: theme.accentForeground
                    font.bold: true
                    font.pixelSize: 11
                }
            }

            // Name
            Text {
                text: root.camName
                color: "white"
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
                width: Math.min(implicitWidth, root.width - 150)
                anchors.verticalCenter: parent.verticalCenter
            }

            // IP
            Text {
                text: "(" + root.camIp + ")"
                color: "#9ca3af"
                font.pixelSize: 11
                visible: root.width > 300
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Live status dot
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: player.playbackState === MediaPlayer.PlayingState ? "#10b981" : "#ef4444"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: player.playbackState === MediaPlayer.PlayingState ? "LIVE" : "WAIT"
                color: player.playbackState === MediaPlayer.PlayingState ? "#10b981" : "#ef4444"
                font.pixelSize: 10
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // Bottom action bar (shown on hover)
    Rectangle {
        id: actionBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        color: Qt.rgba(0, 0, 0, 0.75)
        visible: mouseArea.containsMouse || root.isFocused
        opacity: visible ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }

        Row {
            anchors.centerIn: parent
            spacing: 12

            // Snapshot button
            Button {
                height: 28
                padding: 4
                text: qsTr("Snapshot")
                flat: true
                onClicked: backend.takeSnapshot(root.channel)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Capture high-resolution still image")
            }

            // Focus / Grid toggle
            Button {
                height: 28
                padding: 4
                text: root.isFocused ? qsTr("Exit Focus") : qsTr("Focus (HD)")
                flat: true
                highlighted: root.isFocused
                onClicked: backend.toggleFocus(root.channel)
                ToolTip.visible: hovered
                ToolTip.text: root.isFocused ? qsTr("Return to grid view") : qsTr("View in full resolution")
            }

            // Standalone MPV
            Button {
                height: 28
                padding: 4
                text: qsTr("Open MPV")
                flat: true
                onClicked: backend.openExternalMpv(root.channel)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Open in external hardware-accelerated MPV")
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onDoubleClicked: {
            backend.toggleFocus(root.channel)
        }

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                backend.openExternalMpv(root.channel)
            }
        }
    }
}
