# Vigil

<p align="center">
  <strong>Fast, hardware-accelerated RTSP multi-camera live stream monitor built for Linux with C++, Qt 6, and Wayland.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-blue.svg" alt="C++17" />
  <img src="https://img.shields.io/badge/Framework-Qt%206.12%20(Qt%20Quick)-41CD52.svg" alt="Qt 6" />
  <img src="https://img.shields.io/badge/Display-Wayland%20Native-orange.svg" alt="Wayland" />
  <img src="https://img.shields.io/badge/Audio-PipeWire%20Native-purple.svg" alt="PipeWire" />
  <img src="https://img.shields.io/badge/Decode-H.265%20%2F%20HEVC%20(VAAPI)-red.svg" alt="VAAPI" />
  <img src="https://img.shields.io/badge/License-MIT-green.svg" alt="License: MIT" />
</p>

---

## Overview

**Vigil** is a high-performance Linux desktop client designed specifically for monitoring live RTSP CCTV camera feeds from on-premises Network Video Recorders (NVRs) such as Dahua, CP PLUS, and standard ONVIF-compliant equipment.

Engineered from the ground up for modern Wayland compositors (Hyprland, Sway, GNOME Wayland), Vigil delivers smooth 60 FPS UI rendering, low-latency live video streaming via hardware-accelerated FFmpeg decoding, PipeWire-integrated live audio, and flexible multi-camera layouts ranging from single-camera inspection to 30-channel overview grids.

---

## Key Highlights

- **Dynamic Multi-Camera Grids**: Effortlessly toggle between layout presets:
  - `1×1`: Single full-window inspection view.
  - `2×2`: 4 concurrent camera streams with paging.
  - `3×3`: 9 concurrent camera streams.
  - `4×4`: 16 concurrent camera streams.
  - `5×6`: All 30 channels displayed simultaneously.
- **Intelligent Dual-Stream Architecture**:
  - **Substream (`subtype=1`, 720p/D1)**: Used automatically in multi-camera grid views to conserve network bandwidth and GPU decoding context limits.
  - **Mainstream (`subtype=0`, 1080p Full HD)**: Automatically engaged when focusing on any camera for crystal-clear detail.
- **Historical Recording Playback**:
  - Full NVR recording catalog queries via native HTTP Digest API (`mediaFileFind.cgi`).
  - Interactive timeline scrubber with position tracking, seek slider, and jump controls (`±10s`, `±1m`).
  - Variable speed playback (`0.5×`, `1×`, `2×`, `4×`).
  - Visual clip drawer listing recorded segments with file sizes, timestamps, and one-click play.
  - Seamless toggle between Live monitoring and Historical Playback from toolbar or any camera tile.
- **Hardware-Accelerated Decoding**: Leverages Intel VAAPI, AMD VAAPI, or NVIDIA NVDEC through Qt 6's FFmpeg multimedia backend for minimal CPU overhead.
- **Embedded Live Audio with Smart Muting**:
  - Direct PipeWire output for G.711 A-law (`pcm_alaw`) audio streams.
  - **Smart Mute**: Multi-camera grid views mute audio by default to prevent overlapping noise; single-camera views (Focus and 1×1) auto-unmute.
  - One-click mute/unmute buttons on every tile header and control bar.
- **Instant Focus & Smart View Tracking**: Double-click any camera tile or click in the sidebar to enter full HD Focus mode. Switching back to grid view remembers your active camera and preserves its page position.
- **Zone & Group Filtering**: Categorize and filter cameras by physical area:
  - `Farm 1`, `Farm 2`, `Farm 3`
  - `Main Gate & Office`
  - `Feed Mill`
  - `House & Garage`
  - `Livestock & Brooder`
- **Instant Live Search**: Sub-millisecond filter in the camera sidebar by name, channel number, or camera IP address.
- **Patrol / Tour Mode**: Hands-free auto-rotation through camera pages or focused channels at a configurable interval.
- **Snapshot Capture Tool**: Grab high-resolution still frames directly to `~/cctv_snapshots/` via dedicated background FFmpeg capture.
- **MPV Pop-Out Integration**: Right-click any camera to launch an external, ultra-low-latency MPV window with customized real-time stream flags (`--profile=low-latency --untimed`).
- **Omarchy & Desktop Integration**: Reads desktop theme accent colors dynamically, supports rounded window corners, includes custom SVG application icons, and provides standard `.desktop` launcher integration.
- **Full CLI Interface**: Run headless inventory queries, capture snapshots, or spawn MPV streams directly from shell scripts or terminal.

---

## Technical Architecture

```
                       ┌─────────────────────────┐
                       │    Vigil QML Front-End  │
                       │ (Wayland / Qt Quick 6)  │
                       └────────────┬────────────┘
                                    │ Property Bindings & Signals
                       ┌────────────▼────────────┐
                       │   C++ Backend Engine    │
                       │   (Channel & Layouts)   │
                       └─────┬──────────────┬────┘
                             │              │
              ┌──────────────▼──────┐ ┌─────▼──────────────┐
              │ Qt 6 Multimedia     │ │ External Processes │
              │ (FFmpeg + VAAPI)    │ │ (FFmpeg, MPV)      │
              └───────┬─────────────┘ └────────────────────┘
                      │
        ┌─────────────┴─────────────┐
        │                           │
 ┌──────▼──────┐             ┌──────▼──────┐
 │ Video Output│             │ Audio Output│
 │ (RHI/OpenGL)│             │ (PipeWire)  │
 └─────────────┘             └─────────────┘
```

### Stream Resource Management
Modern integrated graphics (such as Intel UHD / Iris Xe) enforce hardware limits of 7–8 concurrent video decode sessions per process. To support monitoring up to 30 channels stably:
1. Grid tiles cleanly teardown idle `MediaPlayer` demuxers when transitioning between pages or focus states.
2. Grid views default to sub-streams (`subtype=1`), reducing memory bandwidth by ~70%.
3. Connection initiation uses staggered timers (`staggerIndex * 120ms`) to eliminate network traffic bursts and socket contention on the NVR.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `1` | 1×1 Single Camera Layout |
| `2` | 2×2 Grid Layout (4 cameras per page) |
| `3` | 3×3 Grid Layout (9 cameras per page) |
| `4` | 4×4 Grid Layout (16 cameras per page) |
| `5` | 5×6 Full Grid Layout (All 30 cameras) |
| `Left Arrow` / `◀` | Navigate to Previous Page |
| `Right Arrow` / `▶` | Navigate to Next Page |
| `Space` | Page forward / Exit Focus mode |
| `Double Click` | Toggle 1080p HD Focus mode on tile |
| `Right Click` | Open camera in external hardware-accelerated MPV window |
| `P` | Toggle Patrol / Auto-cycle Tour mode |
| `S` | Capture high-resolution snapshot to disk |
| `B` | Toggle left camera inventory sidebar |
| `F` / `F11` | Toggle Fullscreen mode |
| `?` | Display Keyboard Shortcuts quick help overlay |
| `Esc` / `Q` | Exit Focus mode / Quit application |

---

## Installation & Build Instructions

### Prerequisites (Arch Linux / Omarchy)

Install the required Qt 6 multimedia, build tools, and codecs:

```sh
sudo pacman -S --needed \
    base-devel \
    qt6-base \
    qt6-declarative \
    qt6-multimedia \
    qt6-multimedia-ffmpeg \
    ffmpeg \
    mpv \
    pipewire \
    libva-intel-driver \
    intel-media-driver
```

*(For AMD GPUs install `libva-mesa-driver`; for NVIDIA install `nvidia-utils`).*

### Building from Source

Clone the repository and build using the provided scripts:

```sh
git clone https://github.com/aditya1944/vigil.git
cd vigil

# 1. Compile the C++ application
bin/build

# 2. Run the automated unit test suite
bin/test

# 3. (Optional) Build Arch package and install system-wide
bin/install
```

---

## Usage

### 1. Launching the GUI
```sh
# Run directly from build tree:
build/cctv

# Or run system binary if installed:
cctv
```

### 2. Headless CLI Commands
Vigil includes built-in command-line utilities for shell integration, automated surveillance scripts, or cron jobs:

```sh
# Print full inventory of configured cameras, channel numbers, IPs, and zones:
cctv list

# Launch GUI directly in playback mode for Channel 5:
cctv playback 5

# Capture a high-resolution snapshot from Channel 5 without opening the GUI:
cctv snapshot 5

# Launch live Channel 10 directly in a standalone, low-latency MPV window:
cctv mpv 10

# Play recorded historical footage in standalone MPV:
cctv playback-mpv 5 2026-11-17 14:00:00 15:00:00

# Display CLI help:
cctv --help
```

---

## Configuration

Vigil stores its persistent configuration at:
```text
~/.config/cctv/config.json
```

If the file does not exist, Vigil creates it automatically with defaults for the 30 on-premises NVR channels.

### Example `config.json`

```json
{
  "nvrHost": "192.168.1.245",
  "nvrPort": 554,
  "username": "admin",
  "password": "YourSecurePassword",
  "snapshotDir": "/home/user/cctv_snapshots",
  "gridCols": 2,
  "gridRows": 2,
  "patrolInterval": 8,
  "cameras": [
    {
      "channel": 1,
      "name": "Farm Main Gate",
      "ip": "192.168.1.101",
      "group": "Main Gate & Office"
    },
    {
      "channel": 5,
      "name": "Farm-1 platform left",
      "ip": "192.168.1.116",
      "group": "Farm 1"
    },
    {
      "channel": 10,
      "name": "Farm-2 inside right",
      "ip": "192.168.1.106",
      "group": "Farm 2"
    }
  ]
}
```

### Configuration Options Reference

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `nvrHost` | string | `"192.168.1.245"` | IP address or hostname of the NVR |
| `nvrPort` | integer | `554` | RTSP port (standard: 554) |
| `username` | string | `"admin"` | Authentication username for RTSP digest auth |
| `password` | string | `""` | Authentication password for RTSP |
| `snapshotDir` | string | `"~/cctv_snapshots"` | Directory where snapshots are saved |
| `gridCols` | integer | `2` | Initial grid columns (1–6) |
| `gridRows` | integer | `2` | Initial grid rows (1–6) |
| `patrolInterval`| integer | `8` | Interval in seconds between patrol page cycles |
| `cameras` | array | `[...]` | List of camera definitions with channel number, name, IP, and zone |

---

## RTSP URL Convention

Vigil constructs Dahua/CP PLUS standard RTSP URLs using credentials and parameters:

- **Main Stream (Full HD 1080p)**:
  ```text
  rtsp://{username}:{password}@{nvrHost}:{nvrPort}/cam/realmonitor?channel={channel}&subtype=0
  ```
- **Sub Stream (720p / D1)**:
  ```text
  rtsp://{username}:{password}@{nvrHost}:{nvrPort}/cam/realmonitor?channel={channel}&subtype=1
  ```

---

## Project Structure

```text
vigil/
├── bin/
│   ├── build                # qmake6 and parallel make compilation script
│   ├── install              # makepkg package creation and system install
│   └── test                 # Offscreen QtTest automated test runner
├── pkgbuild/
│   ├── PKGBUILD             # Arch Linux / pacman package definition
│   ├── cctv.desktop         # FreeDesktop desktop entry
│   └── cctv.svg             # Vector application icon
├── src/
│   ├── backend.h / .cpp     # Core C++ engine: cameras, RTSP URLs, paging, patrol
│   ├── theme.h / .cpp       # Omarchy live accent color detection & fallback
│   ├── translator.h / .cpp  # Internationalization & translation support
│   ├── main.cpp             # CLI dispatcher & QML application entry point
│   ├── resources.qrc        # Qt resource bundle
│   ├── Main.qml             # Root window, split view, layout switching
│   ├── CameraTile.qml       # Camera player, VideoOutput, AudioOutput, live controls
│   ├── Sidebar.qml          # Camera list, zone filtering, real-time search
│   ├── Toolbar.qml          # Grid presets, patrol toggle, quality toggle, help
│   ├── HelpOverlay.qml      # Keyboard shortcut quick reference modal
│   └── i18n/
│       └── en.json          # English translation strings
├── tests/
│   ├── cctv_tests.pro       # Test suite project configuration
│   └── cctv_tests.cpp       # 11 automated unit tests
├── cctv.pro                 # Main qmake project specification
├── LICENSE                  # MIT License (Aditya Kumar)
└── README.md                # Project documentation
```

---

## Automated Testing

Vigil includes a comprehensive headless unit test suite verifying core functionality without requiring an active X11/Wayland display server:

```sh
bin/test
```

**Test Coverage**:
- Camera database initialization (all 30 channels)
- Zone filtering and search query matching
- Grid dimension calculation and page index arithmetic
- Single camera focus switching and state restoration
- Patrol cycle timer logic
- Theme color detection and fallbacks
- Translation dictionary fallbacks

---

## License

This project is licensed under the [MIT License](LICENSE) — see the [LICENSE](LICENSE) file for details.

Copyright (c) 2026 Aditya Kumar.
