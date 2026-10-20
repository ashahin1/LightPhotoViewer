<p align="center">
  <img src="resources/app_icon_preview.png" width="128" height="128" alt="LightPhotoViewer Icon" />
</p>

# LightPhotoViewer

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg)](https://www.microsoft.com/windows)
[![Release](https://img.shields.io/github/v/release/ashahin1/LightPhotoViewer?color=blue)](https://github.com/ashahin1/LightPhotoViewer/releases)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-426%20KB-brightgreen.svg)]()
[![Private RAM](https://img.shields.io/badge/Private%20RAM-35--50%20MB-success.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An ultra-fast, minimalist, hardware-accelerated image viewer for Windows 10 & 11.
Built with pure C++20, Direct2D 1.1, Direct3D 11, and WIC with **zero runtime DLL dependencies**.

[⬇️ **Download Latest Release (LightPhotoViewer.exe)**](https://github.com/ashahin1/LightPhotoViewer/releases/latest)

---

## ⚡ Performance Benchmarks

Measured on Windows 11 (64-bit) with cold execution and hardware performance counters:

| Metric | LightPhotoViewer | Windows Photos | ImageGlass | IrfanView |
| :--- | :---: | :---: | :---: | :---: |
| **Standalone Binary Size** | **426 KB** | ~150 MB | ~65 MB | ~3 MB |
| **External Runtime DLLs** | **0** (Native OS only) | 40+ | 30+ | 0–10 |
| **Cold Startup Latency** | **~220 ms** | ~1,800 ms | ~900 ms | ~120 ms |
| **Private RAM (4K Image)** | **~35–50 MB** | ~180 MB | ~110 MB | ~40 MB |
| **Admin Required** | **No** | Store managed | Optional | Optional |

---

## 🚀 Key Highlights

* **Instant Cold Launch**: Opens in ~220 ms with zero XAML, WinUI, or Electron runtime bloat.
* **Microscopic Footprint**: Single standalone **403 KB** executable with zero external DLL dependencies (`/MT`).
* **Direct2D Hardware-Accelerated Rendering**:
  * Silky-smooth 60/120/144 Hz rendering using DXGI flip-model swap chains.
  * High-quality bicubic downsampling with bilinear zoom interpolation.
  * Automatic pixel-grid inspection overlay when zooming $\ge$ 800%.
* **1:1 Physical Pixel High-DPI Accuracy**:
  * Direct2D target surface locked to `96.0 DPI` with `PerMonitorV2` High-DPI awareness.
  * 1 image pixel maps to exactly 1 physical screen pixel at 100% actual size without blur or double-scaling.
* **Native Vector SVG Rendering**:
  * Hardware-accelerated vector rendering via Direct2D `ID2D1DeviceContext5::DrawSvgDocument`.
  * Infinite vector scaling up to 5,000% zoom with razor-sharp edges.
* **High-Fidelity Animated GIF89a Compositor**:
  * Full multi-frame logical screen compositor supporting frame sub-rect offsets (`Left`, `Top`, `Width`, `Height`).
  * Honors disposal methods 0–3 (Restore to Background, Restore to Previous) and premultiplied alpha blending.
* **Zero-Lock In-Memory Materialization**:
  * Eager decoding into memory (`WICBitmapCacheOnLoad`) releases OS disk file handles immediately upon load.
  * Move, rename, or delete images externally without encountering Windows file-lock errors.
* **Smart Auto-Hiding Multi-Tabs**:
  * When **1 image** is open: Tab bar automatically hides (0 px height) for an unobstructed view.
  * When **2+ images** are open: Clean tab strip appears with title badges, close buttons (`×`), and a `+` new tab button.
  * Middle-click any tab or press `Ctrl+W` to close.
  * Reopen closed tabs with `Ctrl+Shift+T`.
* **Single-Instance Tab Forwarding**:
  * Double-clicking files in Windows Explorer forwards them seamlessly into tabs of the running window via `WM_COPYDATA`.
* **Dynamic Floating HUD Toolbar**:
  * Smooth auto-fade toolbar that hides when reading images and awakens on mouse movement.
  * Dual-purpose live zoom badge and state-aware **`Fit` $\leftrightarrow$ `1:1`** toggle button with active highlight.
* **Canvas Transparency Backdrops**:
  * Press **`B`** to cycle between **Dark (`#121214`)** $\rightarrow$ **Checkerboard Grid** $\rightarrow$ **Light (`#F8FAFC`)**.
* **Slideshow Mode**:
  * Press **`F5`** or **`S`** to toggle automated 3-second slideshow with a live on-screen status badge.
* **Recent Files (MRU List)**:
  * Persistent recent files list on empty state with keyboard shortcuts `1`–`6` and direct click opening.
* **Interactive `F1` Shortcut Help Overlay**:
  * Press **`F1`** anytime to display an in-canvas categorized Direct2D shortcuts dialog.
* **In-Canvas Print Preview & Native Spooling (`Ctrl+P`)**:
  * Live hardware-accelerated WYSIWYG virtual paper sheet preview directly on the Direct2D canvas.
  * Instant interactive controls for Printer, Paper Size presets (`A4`, `Letter`, `Legal`, `A3`, `A5`, `4×6 in`, `5×7 in`, `8×10 in`), Orientation (`Portrait` / `Landscape`), Scale modes (`Fit`, `Fill (Crop)`, `1:1 Actual Size`), Margins (`Normal 0.5"`, `Narrow 0.25"`, `Borderless`), and Copies (`1`–`99`).
  * Direct high-resolution Win32 spooler printing with transparent backgrounds cleanly composited over pure white and vector SVGs rasterized via Direct2D offscreen GPU buffers.
* **EXIF & Metadata Inspection**:
  * Press **`I`** to toggle image metadata (dimensions, format, file size, camera, exposure, ISO).
* **Zero-Admin Shell Associations**:
  * Run `LightPhotoViewer.exe /register` to associate supported image formats in your user profile without admin rights.

---

## ⌨️ Keyboard & Mouse Shortcuts

### Navigation & Viewing
| Shortcut | Action |
| :--- | :--- |
| `→` / `Space` | Next image in folder |
| `←` / `Backspace` | Previous image in folder |
| `Home` / `End` | First / Last image in folder |
| `0` / Middle Click | Toggle Fit to Window $\leftrightarrow$ 100% Actual Size |
| `1` / Double Click | 100% Actual Size (1:1 physical pixel map) |
| `+` / `Ctrl + Wheel Up` | Zoom In (cursor-centered) |
| `-` / `Ctrl + Wheel Down` | Zoom Out (cursor-centered) |
| `Left Mouse Drag` | Pan canvas |
| `B` | Cycle canvas backdrop (Dark / Checkerboard / Light) |
| `F5` / `S` | Toggle Slideshow mode (3s auto-advance) |
| `F` / `F11` / `Enter` | Toggle Fullscreen |

### Transformations & Information
| Shortcut | Action |
| :--- | :--- |
| `R` | Rotate 90° Clockwise |
| `L` | Rotate 90° Counter-Clockwise |
| `H` | Flip Horizontal |
| `V` | Flip Vertical |
| `I` | Toggle EXIF & metadata HUD card |
| `Delete` | Safely send active image to Windows Recycle Bin |
| `Ctrl + C` | Copy image bitmap to system clipboard |
| `Ctrl + Shift + C` | Copy image file path to clipboard |
| `F1` | Toggle Keyboard Shortcuts Help Overlay |

### Tabs & Application
| Shortcut | Action |
| :--- | :--- |
| `Ctrl + O` | Open image file dialog |
| `Ctrl + P` | Open Print Preview / Execute print job |
| `Ctrl + W` | Close active tab |
| `Ctrl + Shift + T` | Reopen recently closed tab |
| `Ctrl + Tab` | Next tab |
| `Ctrl + Shift + Tab` | Previous tab |
| `1` – `6` | Open recent file from empty state |
| `Esc` | Close help overlay, exit slideshow, or exit fullscreen |

---

## 💻 Command Line Usage

```cmd
:: Open a blank window / empty state
LightPhotoViewer.exe

:: Open a specific image
LightPhotoViewer.exe "C:\path\to\photo.jpg"

:: Open multiple images into tabs
LightPhotoViewer.exe photo1.png photo2.webp photo3.svg

:: Register as default image viewer in user profile (no admin rights required)
LightPhotoViewer.exe /register

:: Remove file associations cleanly
LightPhotoViewer.exe /unregister
```

---

## 🛠️ Building from Source

### Prerequisites
* Windows 10 (Build 1809+) or Windows 11 (64-bit)
* MSVC compiler supporting C++20 (Visual Studio 2022, Build Tools, or portable MSVC)

### Build Commands

```cmd
:: Standard incremental build
build.bat

:: Clean rebuild from scratch
build.bat /rebuild

:: Clean build artifacts
build.bat /clean
```

The build script compiles with:
- C++20 (`/std:c++20`)
- Static C Runtime (`/MT`)
- Link-Time Code Generation (`/GL`, `/LTCG`)
- Size and speed optimizations (`/O2`, `/Gy`, `/Gw`, `/OPT:REF`, `/OPT:ICF`)
- Embedded High-DPI manifest (`PerMonitorV2`)
- Precompiled headers (`pch.hpp`) for sub-second rebuilds.

---

## 📄 License

MIT License. Free and open source for personal and commercial use. See [LICENSE](LICENSE) for details.
