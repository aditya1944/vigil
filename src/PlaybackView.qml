import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root
    color: "#0a0a0f"

    function formatMs(ms) {
        if (!ms || ms <= 0) return "00:00"
        var totalSec = Math.floor(ms / 1000)
        var hr = Math.floor(totalSec / 3600)
        var min = Math.floor((totalSec % 3600) / 60)
        var sec = totalSec % 60
        if (hr > 0) {
            return (hr < 10 ? "0" + hr : "" + hr) + ":" +
                   (min < 10 ? "0" + min : "" + min) + ":" +
                   (sec < 10 ? "0" + sec : "" + sec)
        }
        return (min < 10 ? "0" + min : "" + min) + ":" +
               (sec < 10 ? "0" + sec : "" + sec)
    }

    function addDaysToDate(dateStr, days) {
        var parts = dateStr.split("-")
        if (parts.length !== 3) return dateStr
        var d = new Date(parseInt(parts[0]), parseInt(parts[1]) - 1, parseInt(parts[2]))
        d.setDate(d.getDate() + days)
        var y = d.getFullYear()
        var m = d.getMonth() + 1
        var day = d.getDate()
        return y + "-" + (m < 10 ? "0" + m : "" + m) + "-" + (day < 10 ? "0" + day : "" + day)
    }

    property bool isAudioMuted: false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Top Filter & Search Controls Bar
        Rectangle {
            Layout.fillWidth: true
            height: 52
            color: "#121218"
            border.color: "#1f1f2a"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // Back to Live
                Button {
                    text: qsTr("← Back to Live")
                    flat: true
                    onClicked: backend.setActiveMode("live")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Return to live camera monitoring")
                }

                Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                // Camera Picker
                Text {
                    text: qsTr("Camera:")
                    color: "#9ca3af"
                    font.pixelSize: 12
                }

                ComboBox {
                    id: cameraCombo
                    Layout.preferredWidth: 220
                    model: backend.cameras
                    textRole: "name"
                    currentIndex: {
                        for (var i = 0; i < backend.cameras.length; ++i) {
                            if (backend.cameras[i].channel === backend.playbackChannel) return i
                        }
                        return 0
                    }
                    onActivated: (index) => {
                        var ch = model[index].channel
                        backend.setPlaybackChannel(ch)
                        backend.queryRecordedClips(ch, backend.playbackDate)
                    }
                }

                Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                // Date Picker controls
                Text {
                    text: qsTr("Date:")
                    color: "#9ca3af"
                    font.pixelSize: 12
                }

                Button {
                    text: "◀"
                    flat: true
                    implicitWidth: 32
                    onClicked: {
                        var newDate = root.addDaysToDate(backend.playbackDate, -1)
                        backend.setPlaybackDate(newDate)
                        backend.queryRecordedClips(backend.playbackChannel, newDate)
                    }
                }

                TextField {
                    id: dateField
                    Layout.preferredWidth: 105
                    text: backend.playbackDate
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    onEditingFinished: {
                        backend.setPlaybackDate(text.trim())
                        backend.queryRecordedClips(backend.playbackChannel, text.trim())
                    }
                }

                Button {
                    text: "▶"
                    flat: true
                    implicitWidth: 32
                    onClicked: {
                        var newDate = root.addDaysToDate(backend.playbackDate, 1)
                        backend.setPlaybackDate(newDate)
                        backend.queryRecordedClips(backend.playbackChannel, newDate)
                    }
                }

                Button {
                    text: qsTr("Today")
                    flat: true
                    onClicked: {
                        var today = new Date()
                        var y = today.getFullYear()
                        var m = today.getMonth() + 1
                        var d = today.getDate()
                        var dateStr = y + "-" + (m < 10 ? "0" + m : "" + m) + "-" + (d < 10 ? "0" + d : "" + d)
                        backend.setPlaybackDate(dateStr)
                        backend.queryRecordedClips(backend.playbackChannel, dateStr)
                    }
                }

                Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                // Time Range
                Text {
                    text: qsTr("Time:")
                    color: "#9ca3af"
                    font.pixelSize: 12
                }

                TextField {
                    id: startTimeInput
                    Layout.preferredWidth: 80
                    text: backend.playbackStartTime
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                }

                Text { text: "→"; color: "#6b7280" }

                TextField {
                    id: endTimeInput
                    Layout.preferredWidth: 80
                    text: backend.playbackEndTime
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                }

                // Query / Play range buttons
                Button {
                    text: qsTr("🔍 Search")
                    highlighted: true
                    onClicked: {
                        backend.queryRecordedClips(backend.playbackChannel, backend.playbackDate)
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Query NVR for recordings matching this channel and date")
                }

                Button {
                    text: qsTr("▶ Play")
                    flat: true
                    onClicked: {
                        backend.startPlayback(backend.playbackChannel, backend.playbackDate, startTimeInput.text.trim(), endTimeInput.text.trim())
                    }
                }

                Item { Layout.fillWidth: true }

                // MPV Button
                Button {
                    text: qsTr("Open MPV")
                    flat: true
                    onClicked: {
                        backend.openExternalPlaybackMpv(backend.playbackChannel, backend.playbackDate, startTimeInput.text.trim(), endTimeInput.text.trim())
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Launch selected recording in external hardware-accelerated MPV")
                }
            }
        }

        // Center Content Area: Video Player + Recordings Drawer
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Main Video Player Area
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#08080c"

                AudioOutput {
                    id: playbackAudioOutput
                    muted: root.isAudioMuted
                    volume: 1.0
                }

                MediaPlayer {
                    id: playbackPlayer
                    videoOutput: playbackVideoOutput
                    audioOutput: playbackAudioOutput
                    source: backend.playbackUrl
                    loops: MediaPlayer.Once

                    onErrorOccurred: (error, errorString) => {
                        if (backend.playbackUrl !== "") {
                            console.log("Playback error:", errorString)
                        }
                    }

                    Component.onCompleted: {
                        if (backend.playbackUrl !== "") {
                            play()
                        }
                    }
                }

                Connections {
                    target: backend
                    function onPlaybackUrlChanged() {
                        playbackPlayer.stop()
                        if (backend.playbackUrl !== "") {
                            playbackPlayer.source = backend.playbackUrl
                            playbackPlayer.play()
                        }
                    }
                }

                VideoOutput {
                    id: playbackVideoOutput
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                }

                // Loading Spinner / Status
                Rectangle {
                    anchors.fill: parent
                    color: "#101018"
                    visible: backend.playbackUrl === "" || playbackPlayer.playbackState !== MediaPlayer.PlayingState
                    z: 1

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        BusyIndicator {
                            anchors.horizontalCenter: parent.horizontalCenter
                            running: parent.visible && backend.playbackUrl !== ""
                            width: 42
                            height: 42
                            visible: backend.playbackUrl !== ""
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: backend.playbackUrl === ""
                                  ? qsTr("Select a recorded clip or click Search to view footage")
                                  : qsTr("Connecting to NVR Playback Stream...")
                            color: "#9ca3af"
                            font.pixelSize: 13
                        }
                    }
                }

                // Top Floating Info Bar
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 38
                    color: Qt.rgba(0, 0, 0, 0.75)
                    z: 2

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12

                        // Playback Badge
                        Rectangle {
                            width: 78
                            height: 22
                            radius: 3
                            color: "#f59e0b"
                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text { text: "⏪"; font.pixelSize: 10 }
                                Text { text: qsTr("PLAYBACK"); color: "black"; font.bold: true; font.pixelSize: 10 }
                            }
                        }

                        Text {
                            text: qsTr("Ch %1: %2").arg(backend.playbackChannel).arg(backend.playbackCamera.name || "")
                            color: "white"
                            font.bold: true
                            font.pixelSize: 13
                        }

                        Text {
                            text: "• " + backend.playbackDate + " (" + backend.playbackStartTime + " → " + backend.playbackEndTime + ")"
                            color: "#9ca3af"
                            font.pixelSize: 12
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "📸 Snapshot"
                            flat: true
                            onClicked: backend.takePlaybackSnapshot(backend.playbackChannel, backend.playbackDate, backend.playbackStartTime)
                        }
                    }
                }

                // Bottom Timeline & Playback Controls Bar
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 64
                    color: Qt.rgba(10, 10, 15, 0.92)
                    z: 2

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        // Timeline Slider
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Text {
                                text: root.formatMs(playbackPlayer.position)
                                color: "#38bdf8"
                                font.pixelSize: 12
                                font.bold: true
                                Layout.preferredWidth: 60
                            }

                            Slider {
                                id: timeSlider
                                Layout.fillWidth: true
                                from: 0
                                to: Math.max(1, playbackPlayer.duration)
                                value: playbackPlayer.position
                                onMoved: {
                                    playbackPlayer.seek(value)
                                }
                            }

                            Text {
                                text: root.formatMs(playbackPlayer.duration)
                                color: "#9ca3af"
                                font.pixelSize: 12
                                Layout.preferredWidth: 60
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        // Playback Control Buttons
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            // Play / Pause
                            Button {
                                text: playbackPlayer.playbackState === MediaPlayer.PlayingState ? "⏸ Pause" : "▶ Play"
                                highlighted: playbackPlayer.playbackState === MediaPlayer.PlayingState
                                onClicked: {
                                    if (playbackPlayer.playbackState === MediaPlayer.PlayingState) {
                                        playbackPlayer.pause()
                                    } else {
                                        playbackPlayer.play()
                                    }
                                }
                            }

                            // Jump buttons
                            Button {
                                text: "-1m"
                                flat: true
                                onClicked: playbackPlayer.seek(Math.max(0, playbackPlayer.position - 60000))
                            }

                            Button {
                                text: "-10s"
                                flat: true
                                onClicked: playbackPlayer.seek(Math.max(0, playbackPlayer.position - 10000))
                            }

                            Button {
                                text: "+10s"
                                flat: true
                                onClicked: playbackPlayer.seek(Math.min(playbackPlayer.duration, playbackPlayer.position + 10000))
                            }

                            Button {
                                text: "+1m"
                                flat: true
                                onClicked: playbackPlayer.seek(Math.min(playbackPlayer.duration, playbackPlayer.position + 60000))
                            }

                            Rectangle { width: 1; height: 18; color: "#272738" }

                            // Speed buttons
                            Text { text: qsTr("Speed:"); color: "#6b7280"; font.pixelSize: 11 }

                            Row {
                                spacing: 2
                                Repeater {
                                    model: [0.5, 1.0, 2.0, 4.0]
                                    Button {
                                        implicitHeight: 28
                                        implicitWidth: 38
                                        padding: 2
                                        text: modelData + "x"
                                        flat: playbackPlayer.playbackRate !== modelData
                                        highlighted: playbackPlayer.playbackRate === modelData
                                        onClicked: playbackPlayer.playbackRate = modelData
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // Audio Mute Toggle
                            Button {
                                text: root.isAudioMuted ? "🔇 Unmute" : "🔊 Audio"
                                flat: true
                                onClicked: root.isAudioMuted = !root.isAudioMuted
                            }
                        }
                    }
                }
            }

            // Right Drawer: Recorded Clips from NVR
            Rectangle {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                color: "#111118"
                border.color: "#1e1e28"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Drawer Header
                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        color: "#161622"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12

                            Text {
                                text: qsTr("Recordings (%1)").arg(backend.playbackClips.length)
                                color: "white"
                                font.bold: true
                                font.pixelSize: 13
                                Layout.fillWidth: true
                            }

                            BusyIndicator {
                                running: backend.queryingClips
                                visible: backend.queryingClips
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                            }

                            Button {
                                text: "🔄"
                                flat: true
                                implicitWidth: 32
                                implicitHeight: 32
                                visible: !backend.queryingClips
                                onClicked: backend.queryRecordedClips(backend.playbackChannel, backend.playbackDate)
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Refresh clips from NVR")
                            }
                        }
                    }

                    // Empty State or List of Clips
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Text {
                            anchors.centerIn: parent
                            text: backend.queryingClips ? qsTr("Querying NVR recorded files...") : qsTr("No recorded clips found.\nClick Search or pick another date.")
                            color: "#6b7280"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            visible: backend.playbackClips.length === 0
                        }

                        ListView {
                            id: clipList
                            anchors.fill: parent
                            clip: true
                            model: backend.playbackClips
                            visible: backend.playbackClips.length > 0

                            delegate: Rectangle {
                                width: clipList.width
                                height: 60
                                color: clipMouse.containsMouse ? "#1c1c28" : (index % 2 === 0 ? "#12121a" : "#0e0e14")
                                border.color: clipMouse.containsMouse ? theme.accent : "transparent"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Text {
                                            text: modelData.startTimeDisplay + " → " + modelData.endTimeDisplay
                                            color: "white"
                                            font.bold: true
                                            font.pixelSize: 12
                                        }

                                        Row {
                                            spacing: 6
                                            Text {
                                                text: modelData.durationStr
                                                color: "#38bdf8"
                                                font.pixelSize: 11
                                            }
                                            Text { text: "•"; color: "#4b5563"; font.pixelSize: 10 }
                                            Text {
                                                text: modelData.sizeMb + " MB"
                                                color: "#9ca3af"
                                                font.pixelSize: 11
                                            }
                                        }
                                    }

                                    Button {
                                        text: "▶ Play"
                                        implicitHeight: 28
                                        padding: 4
                                        highlighted: true
                                        onClicked: {
                                            backend.startPlayback(backend.playbackChannel, backend.playbackDate, modelData.startTimeDisplay, modelData.endTimeDisplay)
                                        }
                                    }

                                    Button {
                                        text: "MPV"
                                        flat: true
                                        implicitHeight: 28
                                        implicitWidth: 38
                                        padding: 2
                                        onClicked: {
                                            backend.openExternalPlaybackMpv(backend.playbackChannel, backend.playbackDate, modelData.startTimeDisplay, modelData.endTimeDisplay)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Open this clip in MPV")
                                    }
                                }

                                MouseArea {
                                    id: clipMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onDoubleClicked: {
                                        backend.startPlayback(backend.playbackChannel, backend.playbackDate, modelData.startTimeDisplay, modelData.endTimeDisplay)
                                    }
                                }
                            }

                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        }
                    }
                }
            }
        }
    }
}
