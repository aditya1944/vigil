import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.75)
    z: 100

    signal close()

    MouseArea {
        anchors.fill: parent
        onClicked: root.close()
    }

    Rectangle {
        width: 480
        height: 480
        anchors.centerIn: parent
        color: "#161620"
        border.color: theme.accent
        border.width: 1
        radius: Math.min(8, theme.radius)

        MouseArea {
            // Prevent clicks inside from closing
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("Keyboard Shortcuts & Help")
                    color: "white"
                    font.bold: true
                    font.pixelSize: 18
                    Layout.fillWidth: true
                }

                Button {
                    text: "✕"
                    flat: true
                    onClicked: root.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#2a2a38"
            }

            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 10
                Layout.fillWidth: true

                // Left: key, Right: description
                Repeater {
                    model: [
                        { key: "1..5", desc: qsTr("1×1 up to All Cameras Grid") },
                        { key: "Space", desc: qsTr("Play/Pause or Next Page") },
                        { key: "◀ / ▶", desc: qsTr("Prev/Next Page or Scrub ±5s") },
                        { key: "Mouse Wheel", desc: qsTr("Scrub video (Shift: 30s, Ctrl: 1m)") },
                        { key: "Timeline Drag", desc: qsTr("Click or drag to seek any time") },
                        { key: "Ctrl + Wheel / + - 0", desc: qsTr("Zoom timeline ruler (0: Fit 24h)") },
                        { key: "Double Click", desc: qsTr("Maximize / Fullscreen camera") },
                        { key: "P", desc: qsTr("Toggle Patrol / Auto-Cycle") },
                        { key: "S", desc: qsTr("Snapshot active / focused camera") },
                        { key: "B", desc: qsTr("Toggle Camera List Sidebar") },
                        { key: "F / F11", desc: qsTr("Toggle Fullscreen") },
                        { key: "Right Click", desc: qsTr("Open in external MPV") },
                        { key: "Esc", desc: qsTr("Back to Live / Exit Focus") }
                    ]

                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            width: 80
                            height: 24
                            color: "#222232"
                            radius: 4
                            border.color: "#333348"

                            Text {
                                anchors.centerIn: parent
                                text: modelData.key
                                color: theme.accent
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        Text {
                            text: modelData.desc
                            color: "#d1d5db"
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Text {
                text: qsTr("Config stored at: ~/.config/cctv/config.json")
                color: "#6b7280"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
