import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    height: 48
    color: "#0f0f14"
    border.color: "#1c1c24"
    border.width: 1

    signal toggleSidebar()
    signal toggleFullscreen()
    signal toggleHelp()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        // Sidebar Toggle
        Button {
            text: qsTr("☰ Cameras")
            flat: true
            onClicked: root.toggleSidebar()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Toggle Camera List (Key: B)")
        }

        Rectangle {
            width: 1
            height: 24
            color: "#2a2a38"
        }

        // Layout presets
        Row {
            spacing: 4

            Button {
                text: "1×1"
                flat: !(backend.gridCols === 1 && backend.gridRows === 1 && backend.focusedChannel === -1)
                highlighted: (backend.gridCols === 1 && backend.gridRows === 1 && backend.focusedChannel === -1)
                onClicked: {
                    backend.exitFocus()
                    backend.setGridDimensions(1, 1)
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Single Camera View (Key: 1)")
            }

            Button {
                text: "2×2"
                flat: !(backend.gridCols === 2 && backend.gridRows === 2 && backend.focusedChannel === -1)
                highlighted: (backend.gridCols === 2 && backend.gridRows === 2 && backend.focusedChannel === -1)
                onClicked: {
                    backend.exitFocus()
                    backend.setGridDimensions(2, 2)
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("4 Camera Grid (Key: 2)")
            }

            Button {
                text: "3×3"
                flat: !(backend.gridCols === 3 && backend.gridRows === 3 && backend.focusedChannel === -1)
                highlighted: (backend.gridCols === 3 && backend.gridRows === 3 && backend.focusedChannel === -1)
                onClicked: {
                    backend.exitFocus()
                    backend.setGridDimensions(3, 3)
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("9 Camera Grid (Key: 3)")
            }

            Button {
                text: "4×4"
                flat: !(backend.gridCols === 4 && backend.gridRows === 4 && backend.focusedChannel === -1)
                highlighted: (backend.gridCols === 4 && backend.gridRows === 4 && backend.focusedChannel === -1)
                onClicked: {
                    backend.exitFocus()
                    backend.setGridDimensions(4, 4)
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("16 Camera Grid (Key: 4)")
            }

            Button {
                text: qsTr("All (5×6)")
                flat: !(backend.gridCols === 5 && backend.gridRows === 6 && backend.focusedChannel === -1)
                highlighted: (backend.gridCols === 5 && backend.gridRows === 6 && backend.focusedChannel === -1)
                onClicked: {
                    backend.exitFocus()
                    backend.setGridDimensions(5, 6)
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("All 30 Cameras Grid (Key: 5)")
            }
        }

        Item { Layout.fillWidth: true }

        // Pagination (only when multiple pages exist and not in focus mode)
        RowLayout {
            spacing: 4
            visible: backend.focusedChannel === -1 && backend.totalPages > 1

            Button {
                text: "◀"
                flat: true
                onClicked: backend.prevPage()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Previous Page (Left Arrow)")
            }

            Text {
                text: qsTr("Page %1 / %2").arg(backend.pageIndex + 1).arg(backend.totalPages)
                color: "white"
                font.pixelSize: 12
                font.bold: true
            }

            Button {
                text: "▶"
                flat: true
                onClicked: backend.nextPage()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Next Page (Right Arrow / Space)")
            }
        }

        Rectangle {
            width: 1
            height: 24
            color: "#2a2a38"
        }

        // Substream / Mainstream quality toggle
        Button {
            text: backend.substreamEnabled ? qsTr("Stream: Sub") : qsTr("Stream: Main")
            flat: true
            onClicked: backend.setSubstreamEnabled(!backend.substreamEnabled)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Toggle Substream (bandwidth friendly) or Mainstream (HD)")
        }

        // Patrol mode button
        Button {
            text: backend.patrolActive ? qsTr("⏹ Patrol (%1s)").arg(backend.patrolInterval) : qsTr("▶ Patrol")
            flat: !backend.patrolActive
            highlighted: backend.patrolActive
            onClicked: backend.togglePatrol()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Auto-cycle pages or cameras (Key: P)")
        }

        // Fullscreen button
        Button {
            text: qsTr("⛶ Fullscreen")
            flat: true
            onClicked: root.toggleFullscreen()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Toggle Fullscreen (Key: F / F11)")
        }

        // Help button
        Button {
            text: "?"
            flat: true
            onClicked: root.toggleHelp()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Keyboard Shortcuts (Key: ?)")
        }
    }
}
