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
    title: qsTr("CCTV Live Monitor")
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
            } else if (backend.focusedChannel > 0) {
                backend.exitFocus()
            }
        }
    }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; onActivated: backend.primaryAction() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(1, 1); } }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(2, 2); } }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(3, 3); } }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(4, 4); } }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; onActivated: { backend.exitFocus(); backend.setGridDimensions(5, 6); } }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; onActivated: backend.prevPage() }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; onActivated: backend.nextPage() }
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

            // Main Content Area (Sidebar + Video View)
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Left Camera List Sidebar
                Sidebar {
                    Layout.fillHeight: true
                    visible: win.sidebarVisible
                }

                // Video Grid / Focus Area
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Focused Single Camera View
                    Item {
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
                                modelData: backend.focusedCamera
                                isFocused: true
                            }
                        }
                    }

                    // Multi-camera Grid View
                    Item {
                        anchors.fill: parent
                        anchors.margins: 6
                        visible: backend.focusedChannel <= 0

                        GridLayout {
                            anchors.fill: parent
                            columns: backend.gridCols
                            rows: backend.gridRows
                            rowSpacing: 6
                            columnSpacing: 6

                            Repeater {
                                model: backend.visibleCameras

                                delegate: CameraTile {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    modelData: modelData
                                    isFocused: false
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
