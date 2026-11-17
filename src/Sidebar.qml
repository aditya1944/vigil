import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    width: 280
    color: "#111116"
    border.color: "#1e1e26"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // App Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: "#10b981"
            }

            Text {
                text: qsTr("CCTV Monitor")
                color: "white"
                font.bold: true
                font.pixelSize: 16
                Layout.fillWidth: true
            }

            Rectangle {
                color: "#1f2937"
                radius: 10
                height: 22
                width: 48

                Text {
                    anchors.centerIn: parent
                    text: qsTr("%1 cams").arg(backend.totalCameraCount)
                    color: "#9ca3af"
                    font.pixelSize: 11
                }
            }

            Button {
                implicitWidth: 26
                implicitHeight: 22
                flat: true
                text: backend.fetchingCameraNames ? "⏳" : "🔄"
                enabled: !backend.fetchingCameraNames
                onClicked: backend.fetchCameraNamesFromNvr()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Sync camera names from NVR")
            }
        }

        // Search Input
        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search camera or IP...")
            font.pixelSize: 13
            text: backend.searchQuery
            onTextChanged: backend.setSearchQuery(text)

            background: Rectangle {
                color: "#1a1a24"
                radius: Math.min(4, theme.radius)
                border.color: searchField.activeFocus ? theme.accent : "#2d2d3d"
            }
        }

        // Group selector ComboBox
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: qsTr("Zone:")
                color: "#9ca3af"
                font.pixelSize: 12
            }

            ComboBox {
                id: groupCombo
                Layout.fillWidth: true
                model: backend.groups
                currentIndex: backend.groups.indexOf(backend.currentGroup)
                onActivated: (index) => {
                    backend.setCurrentGroup(model[index])
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#22222e"
        }

        // Camera List
        ListView {
            id: cameraList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: backend.cameras

            delegate: Rectangle {
                id: itemRoot
                width: cameraList.width
                radius: Math.min(4, theme.radius)
                color: {
                    if (backend.focusedChannel === modelData.channel) return "#22223a"
                    if (itemMouse.containsMouse) return "#1a1a24"
                    return "transparent"
                }

                // Filter visibility based on current group & search
                visible: {
                    var inGroup = (backend.currentGroup === "All Cameras" || modelData.group === backend.currentGroup)
                    var q = backend.searchQuery.trim().toLowerCase()
                    if (!q) return inGroup
                    var matchName = modelData.name.toLowerCase().indexOf(q) !== -1
                    var matchIp = modelData.ip.indexOf(q) !== -1
                    var matchCh = ("" + modelData.channel) === q
                    return inGroup && (matchName || matchIp || matchCh)
                }
                height: visible ? 48 : 0

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 8

                    // Channel Badge
                    Rectangle {
                        width: 24
                        height: 22
                        radius: 3
                        color: backend.focusedChannel === modelData.channel ? theme.accent : "#2d3748"

                        Text {
                            anchors.centerIn: parent
                            text: modelData.channel < 10 ? "0" + modelData.channel : "" + modelData.channel
                            color: backend.focusedChannel === modelData.channel ? theme.accentForeground : "white"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    // Name and Info
                    Column {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: modelData.name
                            color: "white"
                            font.pixelSize: 12
                            font.bold: backend.focusedChannel === modelData.channel
                            elide: Text.ElideRight
                            width: itemRoot.width - 90
                        }

                        Text {
                            text: modelData.ip + " • " + modelData.group
                            color: "#6b7280"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            width: itemRoot.width - 90
                        }
                    }

                    // Hover Snapshot Button
                    Button {
                        width: 24
                        height: 24
                        padding: 0
                        text: "📸"
                        flat: true
                        visible: itemMouse.containsMouse
                        onClicked: backend.takeSnapshot(modelData.channel)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Take Snapshot")
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        backend.focusCamera(modelData.channel)
                    }
                    onDoubleClicked: {
                        backend.openExternalMpv(modelData.channel)
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }

        // Bottom connection summary
        Rectangle {
            Layout.fillWidth: true
            height: 36
            color: "#161620"
            radius: Math.min(4, theme.radius)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6

                Text {
                    text: qsTr("NVR: %1:%2").arg(backend.nvrHost).arg(backend.nvrPort)
                    color: "#9ca3af"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Button {
                    implicitWidth: 24
                    implicitHeight: 24
                    flat: true
                    text: "⚙"
                    onClicked: nvrDialog.open()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Configure NVR connection")
                }

                Text {
                    text: backend.substreamEnabled ? "Sub (Speed)" : "Main (HD)"
                    color: backend.substreamEnabled ? "#38bdf8" : "#fbbf24"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }

    Dialog {
        id: nvrDialog
        title: qsTr("NVR Connection Settings")
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 320
        standardButtons: Dialog.Save | Dialog.Cancel

        ColumnLayout {
            spacing: 8
            width: parent.width

            Text { text: qsTr("NVR Host / IP:"); color: "#9ca3af"; font.pixelSize: 12 }
            TextField {
                id: hostInput
                Layout.fillWidth: true
                text: backend.nvrHost
                placeholderText: "e.g. 192.168.1.245"
            }

            Text { text: qsTr("RTSP Port:"); color: "#9ca3af"; font.pixelSize: 12 }
            TextField {
                id: portInput
                Layout.fillWidth: true
                text: backend.nvrPort.toString()
                placeholderText: "554"
            }

            Text { text: qsTr("Username:"); color: "#9ca3af"; font.pixelSize: 12 }
            TextField {
                id: userInput
                Layout.fillWidth: true
                text: backend.username
                placeholderText: "admin"
            }

            Text { text: qsTr("Password:"); color: "#9ca3af"; font.pixelSize: 12 }
            TextField {
                id: passInput
                Layout.fillWidth: true
                text: backend.password
                echoMode: TextInput.Password
            }
        }

        onAccepted: {
            backend.setNvrConnection(hostInput.text, parseInt(portInput.text) || 554, userInput.text, passInput.text)
        }
    }
}
