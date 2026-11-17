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
- **Historical Recording Playback & 24-Hour Timeline Ruler**:
  - **Auto-Fitted Screen Scaling**: The 24-hour ruler automatically fits to the screen width by default so all hours (`00:00` to `23:59`) are fully visible without spilling or horizontal scrolling.
  - **High-Contrast Track Differentiation & Legend**:
    - `🟩 Recorded Footage` (`#10b981` Emerald Green): Displays exact segments queried from NVR (`mediaFileFind.cgi`) with top highlights, durations, and sizes.
    - `⬛ No Recording` (`#1c1c28` Slate Charcoal): Explicitly indicates unrecorded gaps across the 24-hour track.
    - Clicking on an unrecorded gap automatically seeks or jumps to the next available recording.
  - **Zoom & Pan Controls**: Zoom in up to 8× with `Zoom +`, `Ctrl + Wheel`, or `+` / `-` keys; snap back to full screen width anytime with `Fit 24h` or key `0`.
  - **Playhead, Live Hover Guide & Real-Time Orange Needle**:
    - Red playhead needle tracks active playback position with `🎯 Center` auto-centering.
    - Cyan hover needle and timestamp flag preview any point before clicking.
    - **Live Orange Needle (`NOW`)**: Pinpoints the exact current wall-clock time and continuously moves forward as time progresses, marking the real-time boundary.
  - **Recorded-Days-Only Selector**:
    - The day selector displays and allows selecting **only** dates for which recordings actually exist on the NVR (even if present for just 1 second).
    - Automatically scans up to 60 days of historical footage in the background without UI lag.
    - Step buttons (`◀` / `▶`) navigate strictly between available recorded dates.
    - If today has no recordings for a camera, it automatically selects the latest date with available footage.
  - **Mouse-Wheel Scrub-to-Seek**: Scroll the mouse wheel directly over the video player to scrub forward/backward (`±5s`, `±30s` with Shift, `±1m` with Ctrl) with an animated On-Screen Display (OSD) time badge.
  - **Streamlined Playback Navigation**: Clean flickable top navigation with camera picker, available day dropdown, quick "Today" shortcut, NVR clips refresh, standalone MPV launcher, and 1-click recording saver.
  - **Lossless Clip Saving with Date & Time Range**: Save any playback interval or drawer clip directly to `~/cctv_recordings` with formatted filenames containing channel, camera name, date, and time range (e.g., `recording_ch01_Farm_Main_Gate_2026-11-17_14-00-00_to_15-00-00.mp4`).
  - **Scrollable Recordings Drawer**: Lists recorded clips with exact start/end times, durations, file sizes in MB, and one-click play, MPV playback, or disk save (`💾`).
  - **Fine-Grained Stream Progress Slider & Speed Controls**: Variable speed playback (`0.5×`, `1×`, `2×`, `4×`), snapshot, and audio toggle.
  - **Seamless Live / Playback Toggle**: Instant mode switching from the top toolbar or via the `⏪ Playback` button on any camera tile.
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

## Keyboard Shortcuts & Navigation

| Shortcut | Context | Action |
| :--- | :--- | :--- |
| `1` | Live | 1×1 Single Camera Layout |
| `2` | Live | 2×2 Grid Layout (4 cameras per page) |
| `3` | Live | 3×3 Grid Layout (9 cameras per page) |
| `4` | Live | 4×4 Grid Layout (16 cameras per page) |
| `5` | Live | 5×6 Full Grid Layout (All 30 cameras) |
| `Left Arrow` / `◀` | Live / Playback | Previous Page (Live) / Scrub `-5s` (Playback) |
| `Right Arrow` / `▶` | Live / Playback | Next Page (Live) / Scrub `+5s` (Playback) |
| `Space` | Live / Playback | Next Page or Exit Focus (Live) / Play-Pause Toggle (Playback) |
| `Mouse Wheel` | Playback Video | Scrub footage (`±5s`; `Shift` for `±30s`, `Ctrl` for `±1m`) |
| `Timeline Drag` | Playback Ruler | Click or drag to jump to any hour/minute of the day |
| `Ctrl + Wheel` / `+ -` | Playback Ruler | Zoom 24-hour timeline ruler in / out (up to 8×) |
| `0` | Playback Ruler | Reset zoom and Fit all 24 hours to screen width |
| `Double Click` | Live / Playback | Focus camera (Live) / Toggle Fullscreen (Playback) |
| `Right Click` | Live / Playback | Open camera/clip in external hardware-accelerated MPV window |
| `P` | Live | Toggle Patrol / Auto-cycle Tour mode |
| `S` | Live / Playback | Capture high-resolution snapshot to disk |
| `B` | Live | Toggle left camera inventory sidebar |
| `F` / `F11` | Global | Toggle Fullscreen mode |
| `?` | Global | Display Keyboard Shortcuts quick help overlay |
| `Esc` / `Q` | Global | Return to Live / Exit Focus mode / Quit |

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
