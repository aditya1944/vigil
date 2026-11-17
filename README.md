# vigil

Fast, hardware-accelerated RTSP multi-camera live stream monitor built for Linux with C++, Qt 6 and Wayland.

A high-performance, native Wayland/Linux desktop client for viewing live CCTV camera feeds. Built specifically for Omarchy (Hyprland) with C++ and Qt Quick, featuring hardware-accelerated H.265/HEVC decoding, flexible multi-camera grid layouts, instant focus mode, patrol cycling, live audio streaming, and snapshot capture.

Preconfigured for all 30 channels on the on-premises Dahua/CP PLUS NVR (`192.168.1.245`).

---

## Features

- **Live Multi-Camera Grids**: 1×1, 2×2 (4 cameras), 3×3 (9 cameras), 4×4 (16 cameras), and 5×6 (all 30 cameras) with smooth pagination.
- **Dual-Stream Optimization**:
  - **Substream (`subtype=1`)**: High frame rate, low CPU/RAM consumption for multi-camera grids.
  - **Mainstream (`subtype=0`)**: Full HD 1080p resolution automatically activated when focusing on any single camera.
- **Embedded Live Audio**: Wire-connected PipeWire live audio streaming (`pcm_alaw` G.711 A-law) with smart auto-mute in multi-grid and single-click unmute controls.
- **Instant Focus Mode**: Double-click any camera tile to inspect it in full 1080p HD. Press `Esc` or double-click to return to the grid.
- **Zone & Group Filtering**: Categorize and filter cameras by zone:
  - Farm 1
  - Farm 2
  - Farm 3
  - Main Gate & Office
  - Feed Mill
  - House & Garage
  - Livestock & Brooder
- **Live Search**: Instant real-time filtering in the sidebar by camera name, channel number, or IP.
- **Patrol / Tour Mode**: Automatically cycles through pages or cameras every few seconds.
- **Snapshot Tool**: Capture high-resolution timestamped still images directly to `~/cctv_snapshots/`.
- **Standalone MPV Pop-Out**: Right-click any camera to launch a dedicated, low-latency, hardware-accelerated MPV window.
- **Omarchy Native Integration**: Live theme accent color sync, Hyprland window rounding, dark Material design, and full Wayland support.
- **CLI Commands**: Headless commands for listing cameras, taking snapshots, or streaming directly via MPV.

---

## Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `1` | 1×1 View |
| `2` | 2×2 Grid Layout (4 cameras per page) |
| `3` | 3×3 Grid Layout (9 cameras per page) |
| `4` | 4×4 Grid Layout (16 cameras per page) |
| `5` | 5×6 Grid Layout (All 30 cameras) |
| `Left Arrow` / `◀` | Previous Page |
| `Right Arrow` / `▶` | Next Page |
| `Space` | Next Page / Exit Focus |
| `Double Click` | Focus / Maximize camera in 1080p HD |
| `Right Click` | Open camera in external MPV window |
| `P` | Toggle Patrol / Auto-cycle mode |
| `S` | Take snapshot of focused camera |
| `B` | Toggle camera list sidebar |
| `F` / `F11` | Toggle Fullscreen |
| `?` | Show Keyboard Shortcuts overlay |
| `Esc` / `Q` | Exit Focus / Quit |

---

## Installation & Running

### Launch the GUI
```sh
# Run directly from the build directory
~/Work/cctv/build/cctv

# Or install to your system (appears in Super + Space launcher)
~/Work/cctv/bin/install
```

### CLI Commands
```sh
# List all configured cameras with names, IPs, and zones
cctv list

# Capture a high-res snapshot from Channel 1
cctv snapshot 1

# Launch Channel 25 in an external low-latency MPV window
cctv mpv 25
```

---

## Configuration

Configuration is automatically stored and loaded from:
```text
~/.config/cctv/config.json
```

You can customize:
- `nvrHost`: NVR IP address (default: `192.168.1.245`)
- `nvrPort`: RTSP port (default: `554`)
- `username`: NVR username (default: `admin`)
- `password`: NVR password
- `snapshotDir`: Directory where snapshots are saved (default: `~/cctv_snapshots`)
- `gridCols` / `gridRows`: Default grid layout
- `patrolInterval`: Patrol rotation speed in seconds (default: `8`)
- `cameras`: Custom channel mappings, names, and zones

---

## Building from Source

```sh
cd ~/Work/cctv
bin/build        # Compiles binary into build/cctv
bin/test         # Runs automated Qt unit tests
bin/install      # Builds package and installs to /usr/bin/cctv
```
