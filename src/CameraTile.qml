import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtMultimedia

Rectangle {
    id: root
    implicitWidth: 320
    implicitHeight: 180
    color: "#131317"
    border.color: isFocused ? theme.accent : (mouseArea.containsMouse ? "#3b4252" : "#1e1e24")
    border.width: isFocused ? 2 : 1
    radius: Math.min(6, theme.radius)
    clip: true

    property var cameraData: null
    property bool isFocused: false
    property bool active: true
    property int staggerIndex: 0
    property bool manualMuteSet: false
    property bool userMuted: false

    readonly property bool effectiveMuted: manualMuteSet ? userMuted : !(isFocused || (backend.gridCols === 1 && backend.gridRows === 1))

    function toggleAudio() {
        manualMuteSet = true
        userMuted = !effectiveMuted
    }

    readonly property var cam: cameraData || ({})
    readonly property string targetUrl: active ? (isFocused ? (cam.mainUrl || "") : (cam.streamUrl || "")) : ""
    readonly property int channel: cam.channel || 0
    readonly property string camName: cam.name || ""
    readonly property string camIp: cam.ip || ""

    readonly property bool isConnecting: active && (player.playbackState !== MediaPlayer.PlayingState)

    AudioOutput {
        id: audioOutput
        muted: root.effectiveMuted
        volume: 1.0
    }

    MediaPlayer {
        id: player
        videoOutput: videoOutput
        audioOutput: audioOutput

        onErrorOccurred: (error, errorString) => {
            if (root.active && root.targetUrl !== "") {
                if (!reconnectTimer.running) {
                    console.log("Stream error ch " + root.channel + ":", errorString)
                    reconnectTimer.start()
                }
            }
        }
    }

    Timer {
        id: connectTimer
        interval: Math.max(20, root.staggerIndex * 120)
        repeat: false
        onTriggered: {
            if (root.active && root.targetUrl !== "") {
                player.source = root.targetUrl
                player.play()
            }
        }
    }

    Timer {
        id: reconnectTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (root.active && root.targetUrl !== "" && player.playbackState !== MediaPlayer.PlayingState) {
                player.stop()
                player.source = ""
                connectTimer.restart()
            }
        }
    }

    function updateStream() {
        connectTimer.stop()
        reconnectTimer.stop()
        player.stop()
        player.source = ""
        if (root.active && root.targetUrl !== "") {
            connectTimer.restart()
        }
    }

    onTargetUrlChanged: updateStream()
    onActiveChanged: updateStream()
    Component.onCompleted: updateStream()
    Component.onDestruction: {
        player.stop()
        player.source = ""
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        z: 0
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        z: 1

        onDoubleClicked: {
            backend.toggleFocus(root.channel)
        }

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton) {
                backend.openExternalMpv(root.channel)
            }
        }
    }

    // Loading / Standby overlay when video is connecting
    Rectangle {
        anchors.fill: parent
        color: "#18181f"
        visible: root.isConnecting
        z: 2

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
        id: headerBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 34
        color: Qt.rgba(0, 0, 0, 0.72)
        visible: !root.isFocused || mouseArea.containsMouse
        opacity: visible ? 1.0 : 0.0
        z: 3

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }

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

        // Right controls: Audio toggle & Live status dot
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // Audio Mute/Unmute icon button
            Rectangle {
                width: 22
                height: 20
                radius: 3
                color: root.effectiveMuted ? "transparent" : Qt.rgba(16, 185, 129, 0.25)
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: root.effectiveMuted ? "🔇" : "🔊"
                    font.pixelSize: 11
                    opacity: root.effectiveMuted ? 0.6 : 1.0
                }

                MouseArea {
                    id: headerAudioMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleAudio()
                }

                ToolTip.visible: headerAudioMouse.containsMouse
                ToolTip.text: root.effectiveMuted ? qsTr("Unmute Audio") : qsTr("Mute Audio")
            }

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
        visible: mouseArea.containsMouse
        opacity: visible ? 1.0 : 0.0
        z: 3

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

            // Playback button
            Button {
                height: 28
                padding: 4
                text: qsTr("⏪ Playback")
                flat: true
                onClicked: backend.enterPlayback(root.channel)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Review historical recordings for this camera")
            }

            // Audio Toggle button
            Button {
                height: 28
                padding: 4
                text: root.effectiveMuted ? qsTr("🔇 Unmute") : qsTr("🔊 Mute")
                flat: true
                highlighted: !root.effectiveMuted
                onClicked: root.toggleAudio()
                ToolTip.visible: hovered
                ToolTip.text: root.effectiveMuted ? qsTr("Enable live audio stream") : qsTr("Mute audio stream")
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
}
