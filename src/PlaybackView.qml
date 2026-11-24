import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: root
    color: "#09090e"

    // Time Formatting Helpers
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

    function timeToSecs(timeStr) {
        if (!timeStr) return 0
        var parts = timeStr.trim().split(":")
        if (parts.length < 2) return 0
        var h = parseInt(parts[0]) || 0
        var m = parseInt(parts[1]) || 0
        var s = (parts.length >= 3 ? parseInt(parts[2]) : 0) || 0
        return (h * 3600) + (m * 60) + s
    }

    function secsToTime(totalSecs) {
        totalSecs = Math.max(0, Math.min(86399, Math.floor(totalSecs)))
        var h = Math.floor(totalSecs / 3600)
        var m = Math.floor((totalSecs % 3600) / 60)
        var s = totalSecs % 60
        return (h < 10 ? "0" + h : "" + h) + ":" +
               (m < 10 ? "0" + m : "" + m) + ":" +
               (s < 10 ? "0" + s : "" + s)
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

    function todayDateStr() {
        var today = new Date()
        var y = today.getFullYear()
        var m = today.getMonth() + 1
        var d = today.getDate()
        return y + "-" + (m < 10 ? "0" + m : "" + m) + "-" + (d < 10 ? "0" + d : "" + d)
    }

    readonly property bool isViewingToday: backend.playbackDate === todayDateStr()

    function formatDayDisplay(dateStr) {
        if (!dateStr) return ""
        var today = todayDateStr()
        if (dateStr === today) return dateStr + " (Today)"
        var yest = addDaysToDate(today, -1)
        if (dateStr === yest) return dateStr + " (Yesterday)"
        var parts = dateStr.split("-")
        if (parts.length === 3) {
            var d = new Date(parseInt(parts[0]), parseInt(parts[1]) - 1, parseInt(parts[2]))
            var days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
            if (!isNaN(d.getDay())) return dateStr + " (" + days[d.getDay()] + ")"
        }
        return dateStr
    }

    // Current wall-clock time tracking (updates every second)
    property real currentWallClockSecs: {
        var now = new Date()
        return now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds()
    }

    Timer {
        id: wallClockTimer
        interval: 1000
        running: backend.activeMode === "playback"
        repeat: true
        onTriggered: {
            var now = new Date()
            root.currentWallClockSecs = now.getHours() * 3600 + now.getMinutes() * 60 + now.getSeconds()
        }
    }

    // Playback control and navigation functions
    function togglePlayPause() {
        if (playbackPlayer.playbackState === MediaPlayer.PlayingState) {
            playbackPlayer.pause()
            root.osdText = "⏸ Paused"
        } else {
            playbackPlayer.play()
            root.osdText = "▶ Playing"
        }
        osdTimer.restart()
    }

    function stepPlayback(stepMs) {
        var newPos = playbackPlayer.position + stepMs
        newPos = Math.max(0, Math.min(playbackPlayer.duration, newPos))
        playbackPlayer.position = newPos
        if (stepMs > 0) {
            root.osdText = "⏩ +" + (stepMs / 1000) + "s  (" + root.formatMs(newPos) + ")"
        } else {
            root.osdText = "⏪ " + (stepMs / 1000) + "s  (" + root.formatMs(newPos) + ")"
        }
        osdTimer.restart()
    }

    // Responsive Timeline Scaling:
    // When zoomFactor is 1.0, timelineTotalWidth exactly matches the screen/viewport width.
    // All 24 hours fit completely on screen without horizontal spilling!
    property real zoomFactor: 1.0
    readonly property real fitHourWidth: (timelineFlickable && timelineFlickable.width > 0) ? (timelineFlickable.width / 24.0) : 40.0
    readonly property real timelineHourWidth: fitHourWidth * zoomFactor
    readonly property real timelineTotalWidth: (timelineFlickable && timelineFlickable.width > 0) ? Math.max(timelineFlickable.width, timelineFlickable.width * zoomFactor) : 960.0

    function zoomIn() {
        root.zoomFactor = Math.min(8.0, root.zoomFactor + 0.5)
        root.centerOnPlayhead()
    }

    function zoomOut() {
        root.zoomFactor = Math.max(1.0, root.zoomFactor - 0.5)
        if (root.zoomFactor <= 1.0) {
            timelineFlickable.contentX = 0
        } else {
            root.centerOnPlayhead()
        }
    }

    function fitTimeline() {
        root.zoomFactor = 1.0
        timelineFlickable.contentX = 0
    }

    function centerOnPlayhead() {
        if (root.zoomFactor <= 1.0) {
            timelineFlickable.contentX = 0
            return
        }
        var xPos = (root.currentPlaybackSecs / 86400) * root.timelineTotalWidth
        var targetContentX = Math.max(0, Math.min(timelineFlickable.contentWidth - timelineFlickable.width,
                                                  xPos - (timelineFlickable.width / 2)))
        timelineFlickable.contentX = targetContentX
    }

    function findClipAtSecs(secs) {
        for (var i = 0; i < backend.playbackClips.length; ++i) {
            var clip = backend.playbackClips[i]
            var s = root.timeToSecs(clip.startTimeDisplay)
            var e = root.timeToSecs(clip.endTimeDisplay)
            if (e <= s) e = s + (clip.durationSecs || 3600)
            if (secs >= s && secs <= e) {
                return clip
            }
        }
        return null
    }

    function findNextClipAfter(secs) {
        var closest = null
        for (var i = 0; i < backend.playbackClips.length; ++i) {
            var clip = backend.playbackClips[i]
            var s = root.timeToSecs(clip.startTimeDisplay)
            if (s > secs) {
                if (!closest || s < root.timeToSecs(closest.startTimeDisplay)) {
                    closest = clip
                }
            }
        }
        return closest
    }

    function seekToSecs(targetSecs) {
        targetSecs = Math.max(0, Math.min(86399, targetSecs))
        var currentStart = root.timeToSecs(backend.playbackStartTime)
        var offsetMs = (targetSecs - currentStart) * 1000
        if (playbackPlayer.duration > 0 && targetSecs >= currentStart && offsetMs <= playbackPlayer.duration) {
            playbackPlayer.position = Math.max(0, offsetMs)
            root.osdText = "⏱ " + root.secsToTime(targetSecs)
            osdTimer.restart()
            return
        }

        var clip = root.findClipAtSecs(targetSecs)
        if (clip) {
            backend.startPlayback(backend.playbackChannel, backend.playbackDate, root.secsToTime(targetSecs), clip.endTimeDisplay)
            root.osdText = "▶ " + root.secsToTime(targetSecs)
        } else {
            var nextClip = root.findNextClipAfter(targetSecs)
            if (nextClip) {
                root.osdText = "⏭ Jump to recording at " + nextClip.startTimeDisplay
                backend.startPlayback(backend.playbackChannel, backend.playbackDate, nextClip.startTimeDisplay, nextClip.endTimeDisplay)
            } else {
                var timeStr = root.secsToTime(targetSecs)
                root.osdText = "⏱ " + timeStr + " (No recording)"
                backend.startPlayback(backend.playbackChannel, backend.playbackDate, timeStr, "23:59:59")
            }
        }
        osdTimer.restart()
    }

    property bool isAudioMuted: false
    property string osdText: ""

    // Current wall-clock second of the day (0 to 86400)
    readonly property real currentPlaybackSecs: {
        var startSecs = root.timeToSecs(backend.playbackStartTime)
        var offsetSecs = playbackPlayer.position > 0 ? (playbackPlayer.position / 1000) : 0
        return Math.min(86400, startSecs + offsetSecs)
    }

    Connections {
        target: backend
        function onPlaybackStartTimeChanged() {
            root.centerOnPlayhead()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =========================================================================
        // 1. TOP SCROLLABLE FILTER & SEARCH CONTROLS BAR
        // =========================================================================
        Rectangle {
            Layout.fillWidth: true
            height: 52
            color: "#121218"
            border.color: "#1f1f2a"
            border.width: 1

            Flickable {
                id: topFlickable
                anchors.fill: parent
                contentWidth: controlsRow.implicitWidth + 24
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                RowLayout {
                    id: controlsRow
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    // Back to Live
                    Button {
                        text: qsTr("← Back to Live")
                        flat: true
                        onClicked: backend.setActiveMode("live")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Return to live multi-camera monitoring (Key: Esc)")
                    }

                    Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                    // Camera Picker
                    Text { text: qsTr("Camera:"); color: "#9ca3af"; font.pixelSize: 12 }

                    ComboBox {
                        id: cameraCombo
                        Layout.preferredWidth: 260
                        model: backend.cameras
                        textRole: "displayName"

                        function syncCurrentIndex() {
                            if (!cameraCombo.model) return
                            for (var i = 0; i < cameraCombo.model.length; ++i) {
                                if (cameraCombo.model[i].channel === backend.playbackChannel) {
                                    if (cameraCombo.currentIndex !== i) {
                                        cameraCombo.currentIndex = i
                                    }
                                    return
                                }
                            }
                        }

                        Component.onCompleted: syncCurrentIndex()

                        Connections {
                            target: backend
                            function onPlaybackChannelChanged() {
                                cameraCombo.syncCurrentIndex()
                            }
                        }

                        onActivated: (index) => {
                            if (index >= 0 && cameraCombo.model && index < cameraCombo.model.length) {
                                var selectedCam = cameraCombo.model[index]
                                backend.enterPlayback(selectedCam.channel)
                            }
                        }
                    }

                    Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                    // Date Picker (Only Days With Available Recordings)
                    Text { text: qsTr("Day:"); color: "#9ca3af"; font.pixelSize: 12 }

                    Button {
                        id: prevDayBtn
                        text: "◀"
                        flat: true
                        implicitWidth: 32
                        enabled: dateCombo.currentIndex >= 0 && dateCombo.currentIndex < backend.availableDates.length - 1
                        onClicked: {
                            if (dateCombo.currentIndex < backend.availableDates.length - 1) {
                                var nextIdx = dateCombo.currentIndex + 1
                                var chosenDate = backend.availableDates[nextIdx]
                                backend.setPlaybackDate(chosenDate)
                                backend.queryRecordedClips(backend.playbackChannel, chosenDate)
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Previous recorded date")
                    }

                    ComboBox {
                        id: dateCombo
                        Layout.preferredWidth: 175
                        model: backend.availableDates

                        displayText: {
                            if (backend.queryingDates && backend.availableDates.length === 0) {
                                return qsTr("Scanning dates...")
                            }
                            if (backend.availableDates.length === 0) {
                                return qsTr("No recorded dates")
                            }
                            return root.formatDayDisplay(backend.playbackDate)
                        }

                        delegate: ItemDelegate {
                            width: dateCombo.width
                            text: root.formatDayDisplay(modelData)
                            highlighted: dateCombo.highlightedIndex === index
                        }

                        function syncDateIndex() {
                            if (!backend.availableDates) return
                            for (var i = 0; i < backend.availableDates.length; ++i) {
                                if (backend.availableDates[i] === backend.playbackDate) {
                                    if (currentIndex !== i) {
                                        currentIndex = i
                                    }
                                    return
                                }
                            }
                        }

                        onActivated: function(index) {
                            if (index >= 0 && index < backend.availableDates.length) {
                                var chosenDate = backend.availableDates[index]
                                backend.setPlaybackDate(chosenDate)
                                backend.queryRecordedClips(backend.playbackChannel, chosenDate)
                            }
                        }

                        Connections {
                            target: backend
                            function onPlaybackDateChanged() {
                                dateCombo.syncDateIndex()
                            }
                            function onAvailableDatesChanged() {
                                dateCombo.syncDateIndex()
                            }
                        }

                        Component.onCompleted: syncDateIndex()
                    }

                    Button {
                        id: nextDayBtn
                        text: "▶"
                        flat: true
                        implicitWidth: 32
                        enabled: dateCombo.currentIndex > 0
                        onClicked: {
                            if (dateCombo.currentIndex > 0) {
                                var prevIdx = dateCombo.currentIndex - 1
                                var chosenDate = backend.availableDates[prevIdx]
                                backend.setPlaybackDate(chosenDate)
                                backend.queryRecordedClips(backend.playbackChannel, chosenDate)
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Next recorded date")
                    }

                    BusyIndicator {
                        running: backend.queryingDates
                        visible: backend.queryingDates
                        implicitWidth: 20
                        implicitHeight: 20
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Scanning NVR for recorded days...")
                    }

                    Button {
                        text: qsTr("Today")
                        flat: true
                        enabled: backend.availableDates.indexOf(root.todayDateStr()) !== -1
                        visible: !backend.queryingDates || backend.availableDates.length > 0
                        onClicked: {
                            var todayStr = root.todayDateStr()
                            backend.setPlaybackDate(todayStr)
                            backend.queryRecordedClips(backend.playbackChannel, todayStr)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: enabled ? qsTr("Jump to today's recordings") : qsTr("No recordings available for today on this camera")
                    }

                    Button {
                        text: qsTr("🔄 Refresh")
                        flat: true
                        onClicked: {
                            backend.queryAvailableRecordingDates(backend.playbackChannel)
                            backend.queryRecordedClips(backend.playbackChannel, backend.playbackDate)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Reload available dates and recorded clips from NVR")
                    }

                    Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                    // Standalone MPV
                    Button {
                        text: qsTr("Open MPV")
                        flat: true
                        onClicked: {
                            backend.openExternalPlaybackMpv(backend.playbackChannel, backend.playbackDate, backend.playbackStartTime, backend.playbackEndTime)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Watch recorded footage in standalone hardware-accelerated MPV")
                    }

                    // Save Clip Button
                    Button {
                        text: backend.isSavingRecording ? qsTr("⏳ Saving... (Cancel)") : qsTr("💾 Save Clip")
                        highlighted: !backend.isSavingRecording
                        onClicked: {
                            if (backend.isSavingRecording) {
                                backend.cancelSaveRecording()
                            } else {
                                backend.saveRecording(backend.playbackChannel, backend.playbackDate, backend.playbackStartTime, backend.playbackEndTime)
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: backend.isSavingRecording
                            ? qsTr("Saving recording to disk. Click to cancel.")
                            : qsTr("Save selected recording interval (%1 to %2) to disk").arg(backend.playbackStartTime).arg(backend.playbackEndTime)
                    }

                    // Snapshot Button
                    Button {
                        text: qsTr("📸 Snapshot")
                        flat: true
                        onClicked: backend.takePlaybackSnapshot(backend.playbackChannel, backend.playbackDate, backend.playbackStartTime)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Capture still frame snapshot from playback")
                    }

                    Rectangle { width: 1; height: 24; color: "#2d2d3d" }

                    // Active interval display
                    Text {
                        text: "(" + backend.playbackStartTime + " → " + backend.playbackEndTime + ")"
                        color: "#9ca3af"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                ScrollBar.horizontal: ScrollBar {
                    policy: topFlickable.contentWidth > topFlickable.width ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                    height: 4
                }
            }
        }

        // =========================================================================
        // 2. CENTER CONTENT: VIDEO PLAYER + RECORDINGS DRAWER
        // =========================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left / Main Player Area
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
                    source: (backend.activeMode === "playback") ? backend.playbackStreamUrl : ""
                    playbackRate: backend.playbackSpeed

                    onErrorOccurred: (error, errorString) => {
                        if (backend.activeMode === "playback" && backend.playbackStreamUrl !== "") {
                            console.log("Playback error:", errorString)
                        }
                    }

                    Component.onCompleted: {
                        if (backend.activeMode === "playback" && backend.playbackStreamUrl !== "") {
                            play()
                        }
                    }

                    Component.onDestruction: {
                        stop()
                        source = ""
                    }
                }

                Connections {
                    target: backend
                    function onPlaybackStreamUrlChanged() {
                        playbackPlayer.stop()
                        if (backend.activeMode === "playback" && backend.playbackStreamUrl !== "") {
                            playbackPlayer.source = backend.playbackStreamUrl
                            playbackPlayer.play()
                        } else {
                            playbackPlayer.source = ""
                        }
                    }
                    function onActiveModeChanged() {
                        if (backend.activeMode !== "playback") {
                            playbackPlayer.stop()
                            playbackPlayer.source = ""
                        } else {
                            if (backend.playbackStreamUrl !== "") {
                                playbackPlayer.stop()
                                playbackPlayer.source = backend.playbackStreamUrl
                                playbackPlayer.play()
                            }
                        }
                    }
                }

                VideoOutput {
                    id: playbackVideoOutput
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                }

                // Mouse Wheel Scroll-to-Seek & Click Area over Video
                MouseArea {
                    id: videoMouseArea
                    anchors.fill: parent
                    hoverEnabled: true

                    onWheel: (wheel) => {
                        var delta = wheel.angleDelta.y
                        var stepMs = 5000 // 5 seconds default
                        if (wheel.modifiers & Qt.ShiftModifier) stepMs = 30000 // 30s
                        if (wheel.modifiers & Qt.ControlModifier) stepMs = 60000 // 1m

                        var newPos = playbackPlayer.position
                        if (delta > 0) {
                            newPos = Math.min(playbackPlayer.duration, newPos + stepMs)
                            root.osdText = "⏩ +" + (stepMs / 1000) + "s  (" + root.formatMs(newPos) + ")"
                        } else if (delta < 0) {
                            newPos = Math.max(0, newPos - stepMs)
                            root.osdText = "⏪ -" + (stepMs / 1000) + "s  (" + root.formatMs(newPos) + ")"
                        }
                        playbackPlayer.position = newPos
                        osdTimer.restart()
                    }

                    onDoubleClicked: {
                        win.visibility = (win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen)
                    }

                    onClicked: {
                        if (playbackPlayer.playbackState === MediaPlayer.PlayingState) {
                            playbackPlayer.pause()
                            root.osdText = "⏸ Paused"
                        } else {
                            playbackPlayer.play()
                            root.osdText = "▶ Playing"
                        }
                        osdTimer.restart()
                    }
                }

                // OSD (On-Screen Display) Scrub Indicator
                Rectangle {
                    anchors.centerIn: parent
                    width: osdLabel.implicitWidth + 32
                    height: 40
                    radius: 20
                    color: Qt.rgba(0, 0, 0, 0.85)
                    border.color: theme.accent
                    border.width: 1
                    opacity: osdTimer.running ? 1.0 : 0.0
                    visible: opacity > 0
                    z: 5

                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    Text {
                        id: osdLabel
                        anchors.centerIn: parent
                        text: root.osdText
                        color: "white"
                        font.bold: true
                        font.pixelSize: 14
                    }
                }

                Timer {
                    id: osdTimer
                    interval: 1200
                    repeat: false
                }

                // Loading Spinner / Status Overlay
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
                                  ? qsTr("Select a recorded clip or click on timeline to view footage")
                                  : qsTr("Connecting to NVR Playback Stream...")
                            color: "#9ca3af"
                            font.pixelSize: 13
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
                            text: backend.queryingClips ? qsTr("Querying NVR recorded files...") : qsTr("No recorded clips found.\nPick another date or check camera connection.")
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
                                height: 58
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

                                    Button {
                                        text: "💾"
                                        flat: true
                                        implicitHeight: 28
                                        implicitWidth: 32
                                        padding: 2
                                        onClicked: {
                                            backend.saveRecording(backend.playbackChannel, backend.playbackDate, modelData.startTimeDisplay, modelData.endTimeDisplay)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Save this clip to ~/cctv_recordings with date and time range in filename")
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

        // =========================================================================
        // 3. BOTTOM SCROLLABLE 24-HOUR TIMELINE + TRANSPORT CONTROLS
        // =========================================================================
        Rectangle {
            Layout.fillWidth: true
            height: 126
            color: "#0d0d14"
            border.color: "#1e1e28"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                spacing: 3
                anchors.margins: 6

                // --- 24-Hour Timeline Legend & Live Range Status ---
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: qsTr("24-HOUR TIMELINE • %1").arg(backend.playbackDate)
                        color: "#9ca3af"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    // Legend: Recorded Footage
                    Row {
                        spacing: 5
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            radius: 2
                            color: "#10b981"
                            border.color: "#34d399"
                            border.width: 1
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Recorded Footage (%1 clips)").arg(backend.playbackClips.length)
                            color: "#34d399"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    // Legend: No Recording
                    Row {
                        spacing: 5
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            radius: 2
                            color: "#1c1c28"
                            border.color: "#38384f"
                            border.width: 1
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("No Recording")
                            color: "#858598"
                            font.pixelSize: 11
                        }
                    }

                    // Legend: Current Time (Orange)
                    Row {
                        spacing: 5
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10
                            height: 10
                            radius: 2
                            color: "#f97316"
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Current Time: %1").arg(root.secsToTime(root.currentWallClockSecs))
                            color: "#fb923c"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    Rectangle { width: 1; height: 14; color: "#2d2d3d" }

                    // Status display (Hover time or Current Playhead)
                    Text {
                        text: timelineItem.hoverSecs >= 0 
                              ? qsTr("Hover: %1").arg(root.secsToTime(timelineItem.hoverSecs))
                              : qsTr("Playhead: %1").arg(root.secsToTime(root.currentPlaybackSecs))
                        color: timelineItem.hoverSecs >= 0 ? "#38bdf8" : "#ef4444"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // --- 24-Hour Ruler & Track (Screen Fitted by Default) ---
                Rectangle {
                    Layout.fillWidth: true
                    height: 52
                    color: "#08080c"
                    border.color: "#222232"
                    border.width: 1
                    radius: 3
                    clip: true

                    Flickable {
                        id: timelineFlickable
                        anchors.fill: parent
                        contentWidth: root.timelineTotalWidth
                        contentHeight: height
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        Item {
                            id: timelineItem
                            width: root.timelineTotalWidth
                            height: parent.height
                            property real hoverSecs: -1

                            // Top Hour Marks (y: 0 to 18)
                            Row {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 18

                                Repeater {
                                    model: 24
                                    Item {
                                        width: root.timelineTotalWidth / 24
                                        height: 18

                                        Text {
                                            anchors.top: parent.top
                                            anchors.topMargin: 2
                                            anchors.left: parent.left
                                            anchors.leftMargin: 2
                                            text: {
                                                var w = root.timelineTotalWidth / 24
                                                if (w >= 36) return (index < 10 ? "0" + index : "" + index) + ":00"
                                                if (w >= 20) return (index % 2 === 0 ? (index < 10 ? "0" + index : "" + index) + "h" : "")
                                                return (index % 4 === 0 ? (index < 10 ? "0" + index : "" + index) + "h" : "")
                                            }
                                            color: "#6b7280"
                                            font.pixelSize: 10
                                        }

                                        // 30-minute sub-tick mark
                                        Rectangle {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.bottom: parent.bottom
                                            width: 1
                                            height: 5
                                            color: "#262638"
                                        }
                                    }
                                }
                            }

                            // Full-height 1-hour vertical dividing lines across track
                            Row {
                                anchors.fill: parent
                                Repeater {
                                    model: 24
                                    Item {
                                        width: root.timelineTotalWidth / 24
                                        height: parent.height

                                        Rectangle {
                                            anchors.right: parent.right
                                            width: 1
                                            height: parent.height
                                            color: "#1e1e2d"
                                            z: 3
                                        }
                                    }
                                }
                            }

                            // --- Track Bar: (y: 19 to 47, height 28) ---
                            // 1. "No Recording" Base Track (dark slate/charcoal)
                            Rectangle {
                                id: noRecordingTrack
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: 19
                                height: 28
                                radius: 3
                                color: "#1c1c28"
                                border.color: "#2e2e42"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    visible: backend.playbackClips.length === 0
                                    text: backend.queryingClips ? qsTr("Querying NVR recorded files...") : qsTr("No recorded clips found for this date (All 24 hours unrecorded)")
                                    color: "#525266"
                                    font.pixelSize: 11
                                }
                            }

                            // 2. "Recorded Footage" Segments (vibrant emerald green)
                            Repeater {
                                model: backend.playbackClips
                                delegate: Rectangle {
                                    readonly property real startSec: root.timeToSecs(modelData.startTimeDisplay)
                                    readonly property real endSec: {
                                        var s = startSec
                                        var e = root.timeToSecs(modelData.endTimeDisplay)
                                        if (e <= s) e = s + (modelData.durationSecs || 3600)
                                        return Math.min(86400, e)
                                    }
                                    x: Math.round((startSec / 86400) * root.timelineTotalWidth)
                                    y: 19
                                    width: Math.max(3, Math.round(((endSec - startSec) / 86400) * root.timelineTotalWidth))
                                    height: 28
                                    radius: 2
                                    color: clipHover.hovered ? "#34d399" : "#10b981"
                                    border.color: clipHover.hovered ? "#6ee7b7" : "#059669"
                                    border.width: 1
                                    z: 2

                                    // Subtle top highlight
                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        height: 2
                                        color: "#a7f3d0"
                                        opacity: 0.6
                                    }

                                    ToolTip.visible: clipHover.hovered
                                    ToolTip.text: qsTr("RECORDED FOOTAGE\n%1 → %2\nDuration: %3 • Size: %4 MB").arg(modelData.startTimeDisplay).arg(modelData.endTimeDisplay).arg(modelData.durationStr).arg(modelData.sizeMb)

                                    HoverHandler { id: clipHover }
                                }
                            }

                            // 3. Hover Guide Line & Hover Time Flag
                            Rectangle {
                                id: hoverGuideLine
                                x: timelineItem.hoverSecs >= 0 ? Math.round((timelineItem.hoverSecs / 86400) * root.timelineTotalWidth) : -10
                                y: 0
                                width: 1
                                height: parent.height
                                color: "#38bdf8"
                                opacity: 0.8
                                visible: timelineItem.hoverSecs >= 0 && !timelineMouseArea.pressed
                                z: 4

                                Rectangle {
                                    anchors.bottom: parent.top
                                    anchors.bottomMargin: 1
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 52
                                    height: 16
                                    radius: 3
                                    color: "#0284c7"
                                    Text {
                                        anchors.centerIn: parent
                                        text: root.secsToTime(timelineItem.hoverSecs)
                                        color: "white"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            // 4. Playhead Needle & Red Time Flag
                            Rectangle {
                                id: playheadNeedle
                                x: Math.round((root.currentPlaybackSecs / 86400) * root.timelineTotalWidth) - 1
                                y: 0
                                width: 2
                                height: parent.height
                                color: "#ef4444"
                                z: 5

                                Rectangle {
                                    anchors.bottom: parent.top
                                    anchors.bottomMargin: 1
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 52
                                    height: 16
                                    radius: 3
                                    color: "#ef4444"
                                    Text {
                                        anchors.centerIn: parent
                                        text: root.secsToTime(root.currentPlaybackSecs)
                                        color: "white"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            // 5. Current Wall-Clock Time Needle (Orange)
                            Rectangle {
                                id: currentTimeNeedle
                                x: Math.round((root.currentWallClockSecs / 86400) * root.timelineTotalWidth) - 1
                                y: 0
                                width: 2
                                height: parent.height
                                color: "#f97316"
                                opacity: root.isViewingToday ? 1.0 : 0.45
                                z: 6

                                // Top Flag
                                Rectangle {
                                    anchors.bottom: parent.top
                                    anchors.bottomMargin: 1
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 36
                                    height: 14
                                    radius: 2
                                    color: "#f97316"
                                    opacity: root.isViewingToday ? 1.0 : 0.6
                                    Text {
                                        anchors.centerIn: parent
                                        text: "NOW"
                                        color: "white"
                                        font.pixelSize: 8
                                        font.bold: true
                                    }
                                }

                                // Inverted Triangle pointer on track top
                                Canvas {
                                    anchors.top: parent.top
                                    anchors.topMargin: 15
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 8
                                    height: 5
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.fillStyle = "#f97316"
                                        ctx.beginPath()
                                        ctx.moveTo(0, 0)
                                        ctx.lineTo(8, 0)
                                        ctx.lineTo(4, 5)
                                        ctx.closePath()
                                        ctx.fill()
                                    }
                                }

                                ToolTip.visible: currentTimeHover.hovered
                                ToolTip.text: qsTr("CURRENT TIME (NOW)\n%1").arg(root.secsToTime(root.currentWallClockSecs))

                                HoverHandler { id: currentTimeHover }
                            }

                            // 6. Interactive Timeline Scrubbing MouseArea
                            MouseArea {
                                id: timelineMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                acceptedButtons: Qt.LeftButton
                                z: 10

                                function handleMouse(mouseX) {
                                    var ratio = Math.max(0, Math.min(1.0, mouseX / root.timelineTotalWidth))
                                    var targetSecs = ratio * 86400
                                    root.seekToSecs(targetSecs)
                                }

                                onPositionChanged: (mouse) => {
                                    var ratio = Math.max(0, Math.min(1.0, mouse.x / root.timelineTotalWidth))
                                    timelineItem.hoverSecs = ratio * 86400
                                    if (pressed) {
                                        handleMouse(mouse.x)
                                    }
                                }

                                onExited: {
                                    timelineItem.hoverSecs = -1
                                }

                                onClicked: (mouse) => handleMouse(mouse.x)
                                onPressAndHold: (mouse) => handleMouse(mouse.x)

                                onWheel: (wheel) => {
                                    if (wheel.modifiers & Qt.ControlModifier) {
                                        if (wheel.angleDelta.y > 0) {
                                            root.zoomIn()
                                        } else {
                                            root.zoomOut()
                                        }
                                    } else {
                                        if (root.zoomFactor > 1.0) {
                                            timelineFlickable.contentX = Math.max(0, Math.min(timelineFlickable.contentWidth - timelineFlickable.width,
                                                                                               timelineFlickable.contentX - (wheel.angleDelta.y * 1.5)))
                                        }
                                    }
                                }
                            }
                        }

                        ScrollBar.horizontal: ScrollBar {
                            policy: timelineFlickable.contentWidth > timelineFlickable.width + 2 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                            height: 4
                        }
                    }
                }

                // --- Transport Controls & Stream Slider Row ---
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    // Play / Pause
                    Button {
                        text: playbackPlayer.playbackState === MediaPlayer.PlayingState ? "⏸" : "▶"
                        highlighted: playbackPlayer.playbackState === MediaPlayer.PlayingState
                        implicitWidth: 36
                        implicitHeight: 30
                        onClicked: {
                            if (playbackPlayer.playbackState === MediaPlayer.PlayingState) {
                                playbackPlayer.pause()
                            } else {
                                playbackPlayer.play()
                            }
                        }
                    }

                    // Jump Buttons
                    Button {
                        text: "-1m"
                        flat: true
                        implicitHeight: 28
                        padding: 3
                        onClicked: playbackPlayer.position = Math.max(0, playbackPlayer.position - 60000)
                    }

                    Button {
                        text: "-10s"
                        flat: true
                        implicitHeight: 28
                        padding: 3
                        onClicked: playbackPlayer.position = Math.max(0, playbackPlayer.position - 10000)
                    }

                    Button {
                        text: "+10s"
                        flat: true
                        implicitHeight: 28
                        padding: 3
                        onClicked: playbackPlayer.position = Math.min(playbackPlayer.duration, playbackPlayer.position + 10000)
                    }

                    Button {
                        text: "+1m"
                        flat: true
                        implicitHeight: 28
                        padding: 3
                        onClicked: playbackPlayer.position = Math.min(playbackPlayer.duration, playbackPlayer.position + 60000)
                    }

                    // Stream Progress Slider
                    Text {
                        text: root.formatMs(playbackPlayer.position)
                        color: "#38bdf8"
                        font.pixelSize: 11
                        font.bold: true
                        Layout.preferredWidth: 50
                    }

                    Slider {
                        id: streamSlider
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, playbackPlayer.duration)
                        value: playbackPlayer.position
                        onMoved: playbackPlayer.position = value
                    }

                    Text {
                        text: root.formatMs(playbackPlayer.duration)
                        color: "#9ca3af"
                        font.pixelSize: 11
                        Layout.preferredWidth: 50
                        horizontalAlignment: Text.AlignRight
                    }

                    Rectangle { width: 1; height: 18; color: "#272738" }

                    // Speed Selector
                    Text { text: qsTr("Speed:"); color: "#6b7280"; font.pixelSize: 11 }

                    Row {
                        spacing: 2
                        Repeater {
                            model: [0.5, 1.0, 2.0, 4.0]
                            Button {
                                implicitHeight: 26
                                implicitWidth: 34
                                padding: 2
                                text: modelData + "x"
                                flat: Math.abs(backend.playbackSpeed - modelData) > 0.01
                                highlighted: Math.abs(backend.playbackSpeed - modelData) <= 0.01
                                onClicked: {
                                    backend.setPlaybackSpeed(modelData)
                                    root.osdText = "⏩ Speed: " + modelData + "x"
                                    osdTimer.restart()
                                }
                            }
                        }
                    }

                    Rectangle { width: 1; height: 18; color: "#272738" }

                    // Timeline Zoom, Fit & Center Buttons
                    Button {
                        text: qsTr("Fit 24h")
                        flat: root.zoomFactor > 1.0
                        highlighted: root.zoomFactor <= 1.0
                        implicitHeight: 26
                        padding: 2
                        onClicked: root.fitTimeline()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Fit all 24 hours to screen width (no horizontal scrolling)")
                    }

                    Button {
                        text: "Zoom -"
                        flat: true
                        implicitHeight: 26
                        padding: 2
                        onClicked: root.zoomOut()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Zoom out timeline ruler")
                    }

                    Button {
                        text: "Zoom +"
                        flat: true
                        implicitHeight: 26
                        padding: 2
                        onClicked: root.zoomIn()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Zoom in timeline ruler for minute-level detail")
                    }

                    Button {
                        text: "🎯 Center"
                        flat: true
                        implicitHeight: 26
                        padding: 2
                        onClicked: root.centerOnPlayhead()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Scroll timeline to center on the active playhead")
                    }

                    // Audio Mute Toggle
                    Button {
                        text: root.isAudioMuted ? "🔇" : "🔊"
                        flat: true
                        implicitHeight: 26
                        implicitWidth: 32
                        padding: 2
                        onClicked: root.isAudioMuted = !root.isAudioMuted
                        ToolTip.visible: hovered
                        ToolTip.text: root.isAudioMuted ? qsTr("Unmute Audio") : qsTr("Mute Audio")
                    }
                }
            }
        }
    }
}
