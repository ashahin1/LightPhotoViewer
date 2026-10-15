<h1 align="center">
  LightPhotoViewer
</h1>

<p align="center">
  <b>An ultra-fast, minimalist, hardware-accelerated image viewer for Windows.</b><br>
  <sub>311 KB · 230 ms launch · 32–48 MB RAM · Zero runtime DLLs · C++20 · Direct2D/Direct3D 11</sub>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/binary-311_KB-brightgreen?style=flat-square" alt="Size">
  <img src="https://img.shields.io/badge/RAM-32--48_MB-7B1FA2?style=flat-square" alt="RAM">
  <img src="https://img.shields.io/badge/C%2B%2B20-MSVC_14.44-E65100?style=flat-square" alt="C++20">
  <img src="https://img.shields.io/badge/platform-Windows_10%2F11-0078D6?style=flat-square" alt="Windows">
  <img src="https://img.shields.io/badge/DLLs-zero-4CAF50?style=flat-square" alt="Dependencies">
  <img src="https://img.shields.io/badge/license-MIT-9E9E9E?style=flat-square" alt="License">
</p>

---

## Quick Start

```cmd
:: Run — zero installation needed
LightPhotoViewer.exe

:: Open an image directly
LightPhotoViewer.exe "C:\path\to\image.jpg"

:: Register as default image viewer (per-user, no admin required)
LightPhotoViewer.exe /register /silent

:: Unregister associations cleanly
LightPhotoViewer.exe /unregister /silent
```

---

## Features

| Feature | Description |
|---|---|
| ⚡ **Instant Cold Launch** | Starts instantly &mdash; 5x–10x faster than the default Windows Photos app |
| 🪶 **Microscopic Footprint** | **311 KB** single `.exe`, **zero external DLLs**, fully static CRT (`/MT`) |
| 🎮 **Direct2D GPU Rendering** | 60/120/144 Hz hardware-accelerated canvas with high-quality bicubic downsampling, smooth pan/zoom, and pixel-grid rendering at high zoom |
| 🗂️ **Smart Multi-Tabs** | Auto-hiding tab bar: invisible with 1 image open, appears when 2+ images are opened; middle-click close and `Ctrl+W` |
| 🚀 **Single Instance** | Opening images from Windows Explorer forwards directly into new tabs of the active window via `WM_COPYDATA` |
| 🎞️ **Animated GIF Playback** | Native multi-frame playback loop with frame-specific delay timing |
| 🖼️ **Wide Format Support** | JPEG, PNG, WebP, animated GIF, BMP, ICO, TIFF, AVIF, HEIC, and SVG |
| 🎯 **Cursor-Centered Zoom** | Mouse wheel zoom preserves the focal point under the cursor |
| 🧭 **0 ms Perceptual Folder Browsing** | Speculative background worker pre-decodes adjacent images into an LRU cache |
| 🔄 **Transformations** | Rotate CW/CCW (`R`/`L`), Flip Horizontal/Vertical (`H`/`V`), and Fit to Window (`0`) / 100% (`1`) |
| ℹ️ **EXIF & Properties HUD** | Press `I` to inspect camera model, date taken, exposure time, ISO, dimensions, file size, and folder index |
| 🗑️ **Recycle Bin Integration** | Press `Delete` to safely move the active image to the Windows Recycle Bin with undo support |
| 📋 **Direct Clipboard Copy** | Press `Ctrl+C` to copy the bitmap to clipboard, or `Ctrl+Shift+C` to copy the file path |
| 🔗 **Zero-Admin Association** | `LightPhotoViewer.exe /register` sets up Windows "Open with" without administrator elevation |

---

## Benchmarks

Measured on Windows 11 x64 with cold execution and hardware performance counters.

| Metric | LightPhotoViewer | Windows Photos | ImageGlass | IrfanView |
| :--- | :---: | :---: | :---: | :---: |
| **Binary Size** | **311 KB** | ~150 MB | ~65 MB | ~3 MB |
| **External DLLs** | **0** | 40+ | 30+ | 0–10 |
| **Cold Startup** | **~230 ms** | ~1,800 ms | ~900 ms | ~120 ms |
| **Private RAM (4K Image)** | **~35 MB** | ~180 MB | ~110 MB | ~40 MB |
| **Admin Required** | **No** | Store managed | Optional | Optional |

---

## Keyboard & Mouse Shortcuts

### Navigation & Viewing
| Shortcut | Action |
| :--- | :--- |
| `→` / `Space` | Next image in folder |
| `←` / `Backspace` | Previous image in folder |
| `Home` / `End` | First / Last image in folder |
| `0` / Middle Click | Fit to Window |
| `1` / Double Click | 100% Actual Size |
| `+` / `Mouse Wheel Up` | Zoom In (focal-centered) |
| `-` / `Mouse Wheel Down`| Zoom Out (focal-centered) |
| `Left Mouse Drag` | Pan canvas |
| `F` / `F11` / `Enter` | Toggle Fullscreen |

### Editing & Information
| Shortcut | Action |
| :--- | :--- |
| `R` | Rotate 90° Clockwise |
| `L` | Rotate 90° Counter-Clockwise |
| `H` | Flip Horizontal |
| `V` | Flip Vertical |
| `I` | Toggle EXIF & Properties HUD card |
| `Delete` | Move active image to Windows Recycle Bin |
| `Ctrl + C` | Copy image bitmap to system clipboard |
| `Ctrl + Shift + C` | Copy image file path to clipboard |

### Tabs & Application
| Shortcut | Action |
| :--- | :--- |
| `Ctrl + O` | Open image file dialog |
| `Ctrl + W` | Close active tab |
| `Ctrl + Tab` | Next tab |
| `Ctrl + Shift + Tab` | Previous tab |
| `Middle Click Tab` | Close tab |
| `Esc` | Exit fullscreen or dismiss EXIF card |

---

## Build from Source

Requirements: Windows 10/11 x64 and the portable MSVC toolchain located in `..\LightPDF\tools\msvc`.

```cmd
:: Standard release build
build.bat

:: Clean build
build.bat /clean

:: Rebuild from scratch
build.bat /rebuild
```

---

## License

MIT License. Free and open source for personal and commercial use.
