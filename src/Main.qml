import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: backend.activeMode === "playback" ? qsTr("Vigil — Historical Playback") : qsTr("Vigil — Live Monitor")
    color: "#09090c"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool sidebarVisible: true

    // Shortcuts
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible) {
                win.helpVisible = false
            } else if (backend.activeMode === "playback") {
                backend.setActiveMode("live")
            } else if (backend.focusedChannel > 0) {
                backend.exitFocus()
            }
        }
    }
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") {
                playbackView.togglePlayPause()
            } else {
                backend.primaryAction()
            }
        }
    }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(1, 1); } }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(2, 2); } }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(3, 3); } }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(4, 4); } }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setAllCamerasGrid(); } }
    Shortcut {
        sequence: "Left"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") {
                playbackView.stepPlayback(-5000)
            } else {
                backend.prevPage()
            }
        }
    }
    Shortcut {
        sequence: "Right"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") {
                playbackView.stepPlayback(5000)
            } else {
                backend.nextPage()
            }
        }
    }
    Shortcut {
        sequence: "+"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") playbackView.zoomIn()
        }
    }
    Shortcut {
        sequence: "="
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") playbackView.zoomIn()
        }
    }
    Shortcut {
        sequence: "-"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") playbackView.zoomOut()
        }
    }
    Shortcut {
        sequence: "0"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.activeMode === "playback") playbackView.fitTimeline()
        }
    }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; onActivated: backend.togglePatrol() }
    Shortcut { sequence: "B"; context: Qt.ApplicationShortcut; onActivated: win.sidebarVisible = !win.sidebarVisible }
    Shortcut {
        sequence: "F"
        context: Qt.ApplicationShortcut
        onActivated: win.visibility = (win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen)
    }
    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: win.visibility = (win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen)
    }
    Shortcut {
        sequence: "S"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.focusedChannel > 0) {
                backend.takeSnapshot(backend.focusedChannel)
            } else {
                backend.showMessage(qsTr("Click or focus a camera first to snapshot"))
            }
        }
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top Toolbar
            Toolbar {
                Layout.fillWidth: true
                onToggleSidebar: win.sidebarVisible = !win.sidebarVisible
                onToggleFullscreen: win.visibility = (win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen)
                onToggleHelp: win.helpVisible = !win.helpVisible
            }

            // Main Content Area
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Live Stream View (Sidebar + Live Video Grid / Focus)
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    visible: backend.activeMode === "live"

                    // Left Camera List Sidebar
                    Sidebar {
                        Layout.fillHeight: true
                        visible: win.sidebarVisible
                    }

                    // Video Grid / Focus Area
                    Item {
                        id: viewArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                    // Focused Single Camera View
                    Item {
                        id: focusedContainer
                        anchors.fill: parent
                        anchors.margins: 6
                        visible: backend.focusedChannel > 0

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6

                            // Focused mode top header bar
                            RowLayout {
                                Layout.fillWidth: true
                                height: 32

                                Button {
                                    text: qsTr("← Back to Grid")
                                    flat: true
                                    onClicked: backend.exitFocus()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Return to multi-camera grid (Key: Esc / Space)")
                                }

                                Text {
                                    text: backend.focusedCamera ? (qsTr("Focus: Ch %1 — %2").arg(backend.focusedCamera.channel).arg(backend.focusedCamera.name)) : ""
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 14
                                    Layout.fillWidth: true
                                }

                                Button {
                                    text: qsTr("Snapshot (HD)")
                                    flat: true
                                    onClicked: backend.takeSnapshot(backend.focusedChannel)
                                }

                                Button {
                                    text: qsTr("Open MPV")
                                    flat: true
                                    onClicked: backend.openExternalMpv(backend.focusedChannel)
                                }
                            }

                            // Full screen player
                            CameraTile {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                cameraData: backend.focusedCamera
                                isFocused: true
                                active: backend.focusedChannel > 0
                            }
                        }
                    }

                    // Multi-camera Grid View
                    Item {
                        id: gridContainer
                        anchors.fill: parent
                        anchors.margins: 6
                        visible: backend.focusedChannel <= 0

                        Grid {
                            id: cameraGrid
                            width: gridContainer.width
                            height: gridContainer.height
                            columns: Math.max(1, backend.gridCols)
                            rows: Math.max(1, backend.gridRows)
                            spacing: 6

                            readonly property real cellWidth: Math.max(10, Math.floor((width - spacing * (columns - 1)) / columns))
                            readonly property real cellHeight: Math.max(10, Math.floor((height - spacing * (rows - 1)) / rows))

                            Repeater {
                                model: backend.visibleCameras

                                delegate: CameraTile {
                                    width: cameraGrid.cellWidth
                                    height: cameraGrid.cellHeight
                                    cameraData: modelData
                                    isFocused: false
                                    staggerIndex: index
                                    active: backend.focusedChannel <= 0
                                }
                            }
                        }

                        // Empty State if no cameras match search/group
                        Rectangle {
                            anchors.centerIn: parent
                            width: 320
                            height: 140
                            radius: Math.min(8, theme.radius)
                            color: "#161622"
                            border.color: "#272738"
                            visible: backend.visibleCameras.length === 0

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("No cameras found")
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: 15
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("Try selecting another zone or clearing search")
                                    color: "#9ca3af"
                                    font.pixelSize: 12
                                }

                                Button {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("Show All Cameras")
                                    onClicked: {
                                        backend.setCurrentGroup("All Cameras")
                                        backend.setSearchQuery("")
                                    }
                                }
                            }
                        }
                    }
                }
            }

                // Historical Playback View
                PlaybackView {
                    id: playbackView
                    anchors.fill: parent
                    visible: backend.activeMode === "playback"
                }
            }
        }

        // Floating Toast Notification
        Rectangle {
            id: toast
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            anchors.horizontalCenter: parent.horizontalCenter
            height: 38
            width: toastText.implicitWidth + 32
            radius: Math.min(19, theme.radius)
            color: Qt.rgba(0, 0, 0, 0.88)
            border.color: theme.accent
            border.width: 1
            visible: backend.statusMessage.length > 0
            opacity: visible ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: 200 }
            }

            Text {
                id: toastText
                anchors.centerIn: parent
                text: backend.statusMessage
                color: "white"
                font.bold: true
                font.pixelSize: 13
            }
        }

        // Help Overlay
        HelpOverlay {
            visible: win.helpVisible
            onClose: win.helpVisible = false
        }
    }
}
