<p align="center">
  <img src="resources/app_icon_preview.png" width="128" height="128" alt="LightPhotoViewer Icon" />
</p>

# LightPhotoViewer

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6.svg)](https://www.microsoft.com/windows)
[![Release](https://img.shields.io/github/v/release/ashahin1/LightPhotoViewer?color=blue)](https://github.com/ashahin1/LightPhotoViewer/releases)
[![Binary Size](https://img.shields.io/badge/Binary%20Size-477%20KB-brightgreen.svg)]()
[![Private RAM](https://img.shields.io/badge/Private%20RAM-35--55%20MB-success.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An ultra-fast, minimalist, hardware-accelerated image viewer for Windows 10 & 11.  
Built with pure C++20, Direct2D 1.1, Direct3D 11, and WIC with **zero runtime DLL dependencies**.

[⬇️ **Download Latest Release (LightPhotoViewer.exe)**](https://github.com/ashahin1/LightPhotoViewer/releases/latest)

---

## ⚡ Performance Benchmarks

Measured on Windows 11 (64-bit) with cold execution and hardware performance counters:

| Metric | LightPhotoViewer | Windows Photos | ImageGlass | IrfanView |
| :--- | :---: | :---: | :---: | :---: |
| **Standalone Binary Size** | **477 KB** | ~150 MB | ~65 MB | ~3 MB |
| **External Runtime DLLs** | **0** (Native OS only) | 40+ | 30+ | 0–10 |
| **Cold Startup Latency** | **~220 ms** | ~1,800 ms | ~900 ms | ~120 ms |
| **Private RAM (4K Image)** | **~35–55 MB** | ~180 MB | ~110 MB | ~40 MB |
| **Admin Required** | **No** | Store managed | Optional | Optional |

---

## ✨ Core Features

### 🎨 Hardware-Accelerated Rendering Engine
* **Direct2D GPU Acceleration**: Silky-smooth 60/120/144 Hz rendering using DXGI flip-model swap chains with high-quality bicubic downsampling and bilinear zoom interpolation.
* **1:1 Physical Pixel High-DPI Accuracy**: Surface locked to `96.0 DPI` with `PerMonitorV2` High-DPI awareness—1 image pixel maps to exactly 1 physical screen pixel at 100% actual size without blur or scaling artifacts.
* **Automatic Pixel-Grid Inspection**: Automatically overlays an inspection grid when zooming $\ge 800\%$ for precision pixel-art and texture editing.
* **Native Vector SVG Rendering**: Hardware-accelerated vector rendering via Direct2D `ID2D1DeviceContext5::DrawSvgDocument` with infinite vector scaling up to 5,000% zoom.
* **High-Fidelity Animated GIF89a Compositor**: Multi-frame logical screen compositor supporting sub-rect offsets (`Left`, `Top`, `Width`, `Height`), disposal methods 0–3, and premultiplied alpha blending.
* **Canvas Transparency Backdrops**: Press **`B`** to cycle between **Dark (`#121214`)** $\rightarrow$ **Checkerboard Grid** $\rightarrow$ **Light (`#F8FAFC`)**.

### 📷 Camera RAW & EXIF Metadata
* **26+ Camera RAW Formats**: Blazing-fast viewing for professional RAW photos from Canon, Nikon, Sony, Adobe DNG, Fujifilm, Olympus, Panasonic, Pentax, Hasselblad, Leica, Sigma, Samsung, Kodak, Minolta, Leaf, Mamiya, and Epson.
* **2-Tier Intelligent Decoding Pipeline**:
  * **Tier 1 (Instant WIC Preview)**: Leverages camera-embedded full-resolution preview streams (`14–100 ms`, up to 24 MP) with orientation transformations, eliminating 1–2 second Bayer demosaicing lags during folder browsing. Automatically switches to full sensor demosaicing via `CLSID_WICRAWDecoder` on Adobe `.dng` files with sub-1024px thumbnails.
  * **Tier 2 (Zero-Dependency Embedded Recovery)**: Built-in binary stream extractor scanning for baseline/progressive JPEG frames decoded via `CLSID_WICJpegDecoder`, guaranteeing instant RAW viewing even without Windows Store RAW codec packages.
* **Zero-Lock In-Memory Materialization**: Eager decoding into memory (`WICBitmapCacheOnLoad`) releases OS disk file handles immediately upon load—move, rename, or delete images externally without encountering Windows file-lock errors.
* **EXIF & Camera Metadata HUD**: Press **`I`** to toggle the EXIF card displaying Camera Make & Model (with automatic brand deduplication), Date Taken, Shutter Speed (`1/Xs`), Aperture (`f/X.X`), Focal Length (`Xmm`), ISO speed, Dimensions, and File Size.

### 💾 In-Canvas Export & Format Conversion (`Ctrl+S`)
* **6 Native Target Encoders**: Convert any open image—including all 26+ Camera RAW formats, vector SVG, WebP, AVIF, HEIC, animated GIF frames, PNG, JPEG, BMP, TIFF, and ICO—into **JPEG (`.jpg`)**, **PNG (`.png`)**, **BMP (`.bmp`)**, **TIFF (`.tiff`)**, **GIF (`.gif`)**, or **JPEG XR (`.jxr`)**.
* **Live Exact File-Size Telemetry**: Encodes output in memory (`CreateStreamOnHGlobal`) and displays the exact output byte size and compression delta (`Est. Size: 101.4 KB (−68%)`) before writing to disk.
* **Interactive Canvas Controls**:
  * Smooth lossy quality slider (`1–100%`) with `−` / `+` steppers and click-to-type numeric entry.
  * Resolution scale presets (`100%`, `75%`, `50%`, `25%`, `Custom`) and clickable `W` / `H` inputs with aspect-ratio lock (`🔒`).
  * Alpha matte flattening selector (`Keep Alpha`, `White #FFF`, `Black #000`) with live canvas simulation.
  * View transform baking toggle (`Bake Rot/Flip` vs `Original`).
  * Animated GIF frame stepper (`◀ Frame X of Y ▶` / `[` and `]`) for exporting any individual animation frame.
* **Atomic Disk Writes**: Encoded bytes write to a temporary `.lpv_tmp` file first and swap via `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`, ensuring interrupted saves never corrupt existing files.

### 🖨️ In-Canvas Print Preview & Spooling (`Ctrl+P`)
* **WYSIWYG Virtual Paper Sheet**: Hardware-accelerated paper preview directly on the canvas with true aspect ratio representation.
* **Interactive Print Controls**: Dropdown popovers for local/network printers and standard paper sizes (`A4`, `Letter`, `Legal`, `A3`, `A5`, `4×6 in`, `5×7 in`, `8×10 in`).
* **Sizing & Margins**: Switch between `Fit to Page`, `Fill Page (Crop)`, and `1:1 Actual Size` with `Normal`, `Narrow`, or `Borderless` margins.
* **Animated GIF Frame Freeze**: Stepper and `[` / `]` keyboard shortcuts let you select and freeze any frame of an animated image for still printing.
* **Direct High-Resolution Spooling**: Native Win32 GDI print spooler with transparent backgrounds cleanly composited over white and vector SVGs rasterized through Direct2D offscreen GPU buffers.

### 🗂️ Minimalist Window & Tab Workflow
* **Smart Auto-Hiding Multi-Tabs**: When 1 image is open, the tab strip automatically hides for a clean view; when 2+ images are open, an interactive tab bar appears. Close tabs with `Ctrl+W` or middle-click, and reopen closed tabs with `Ctrl+Shift+T`.
* **Single-Instance Tab Forwarding**: Double-clicking files in Windows Explorer forwards them into tabs of the running window via `WM_COPYDATA`.
* **Dynamic Floating HUD Toolbar**: Auto-fading toolbar awakens on mouse movement with a dual-purpose zoom badge and state-aware **`Fit` $\leftrightarrow$ `1:1`** toggle button.
* **Automated Slideshow Mode**: Press **`F5`** or **`S`** to cycle images automatically every 3 seconds.
* **Recent Files (MRU List)**: Displays persistent recent files on empty state with keyboard shortcuts `1`–`6` and direct click opening.
* **System-Aware Theme Synchronization**: Native Windows 11/10 dark mode integration via `uxtheme.dll` dynamically matches DWM title bars and context menus with system preferences.

---

## 🖼️ Supported Formats

LightPhotoViewer decodes images through the Windows Imaging Component (WIC) and Direct2D, keeping the executable completely standalone without third-party codec dependencies.

### Input Formats (Viewing & Converting)

#### Standard Formats
| Format | Extensions | Notes |
| :--- | :--- | :--- |
| **JPEG** | `.jpg`, `.jpeg`, `.jpe`, `.jfif` | EXIF metadata and orientation supported |
| **PNG** | `.png` | Full 32-bit alpha transparency supported |
| **GIF** | `.gif` | Animated GIF89a with multi-frame compositor |
| **WebP** | `.webp` | Native decoding on modern Windows 10/11 |
| **BMP** | `.bmp`, `.dib` | Standard Windows Bitmap |
| **Icon** | `.ico` | Windows Icon resource |
| **TIFF** | `.tiff`, `.tif` | Tagged Image File Format |
| **SVG** | `.svg` | Native Direct2D vector rendering up to 5,000% zoom |
| **AVIF** | `.avif` | Requires the free *AV1 Video Extension* from the Microsoft Store |
| **HEIC / HEIF** | `.heic`, `.heif` | Requires the *HEIF Image Extensions* from the Microsoft Store |

#### Camera RAW Formats (26+ Manufacturers)
| Manufacturer | Extensions |
| :--- | :--- |
| **Canon** | `.cr2`, `.cr3`, `.crw` |
| **Nikon** | `.nef`, `.nrw` |
| **Sony** | `.arw`, `.srf`, `.sr2` |
| **Adobe (Universal)** | `.dng` |
| **Fujifilm** | `.raf` |
| **Olympus / OM System** | `.orf`, `.ori` |
| **Panasonic** | `.rw2` |
| **Pentax / Ricoh** | `.pef`, `.ptx` |
| **Hasselblad** | `.3fr`, `.fff` |
| **Leica** | `.rwl`, `.raw` |
| **Sigma** | `.x3f` |
| **Samsung** | `.srw` |
| **Kodak** | `.kdc`, `.dcr` |
| **Minolta** | `.mrw` |
| **Leaf** | `.mos` |
| **Mamiya** | `.mef` |
| **Epson** | `.erf` |

> [!NOTE]
> RAW files are displayed using high-resolution previews embedded by the camera ISP firmware for near-instant opening. When previews are unavailable or sub-resolution, LightPhotoViewer arbitrates full sensor demosaicing via installed WIC decoders or falls back to internal JPEG stream recovery.

### Output Formats (In-Canvas Export & Conversion)
| Target Format | Extension | Capabilities |
| :--- | :--- | :--- |
| **JPEG** | `.jpg` | Adjustable quality (`1–100%`), automatic alpha matting (`White` / `Black`) |
| **PNG** | `.png` | Lossless compression, full 32-bit alpha channel preservation |
| **BMP** | `.bmp` | 24-bit RGB or 32-bit BGRA uncompressed bitmap |
| **TIFF** | `.tiff` | Lossless LZW compression, multi-channel support |
| **GIF** | `.gif` | 256-color custom adaptive palette with error-diffusion dithering |
| **JPEG XR** | `.jxr` | High dynamic range, adjustable quality (`1–100%`), alpha channel support |

---

## 📦 Installation & Windows Integration

### Portable Usage
LightPhotoViewer is 100% portable:
1. Download `LightPhotoViewer.exe` (or the `.zip` package) from the [Latest Release](https://github.com/ashahin1/LightPhotoViewer/releases/latest).
2. Place the executable anywhere and run it directly. No installer, runtime packages, or administrative rights are required.

### Shell Association & Default Apps (`/register`)
To integrate LightPhotoViewer with Windows Explorer and make it available in the **"Open with"** list and **Windows Settings > Apps > Default apps**:

```cmd
:: Register file associations for current user (no admin rights required)
LightPhotoViewer.exe /register

:: Silently register without displaying confirmation dialog
LightPhotoViewer.exe /register /silent

:: Remove all registered keys and associations cleanly
LightPhotoViewer.exe /unregister

:: Silently unregister without confirmation
LightPhotoViewer.exe /unregister /silent
```

> [!TIP]
> Registration operates strictly per-user under `HKEY_CURRENT_USER`. It registers the ProgID (`LightPhotoViewer.Image`), `OpenWithProgids`, `Applications\LightPhotoViewer.exe\SupportedTypes`, and Windows `Capabilities` / `RegisteredApplications`. Windows security does not allow applications to silently seize default associations; after registering, select LightPhotoViewer in **Settings > Apps > Default apps** or right-click any image and choose **Open with > Choose another app > Always**.

### System Requirements
* **Operating System**: Windows 10 (Build 1809+) or Windows 11 (64-bit).
* **Hardware**: DirectX 11.1 compatible GPU or integrated graphics.

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

### Export, Print & Application
| Shortcut | Action |
| :--- | :--- |
| `Ctrl + O` | Open image file dialog (multi-select supported) |
| `Ctrl + S` | Open Export / Convert Image modal (or save when modal is open) |
| `Ctrl + P` | Open Print Preview / Execute print job |
| `[` / `]` | Previous / Next animation frame (in Export Modal & Print Preview) |
| `Ctrl + W` | Close active tab |
| `Ctrl + Shift + T` | Reopen recently closed tab |
| `Ctrl + Tab` | Next tab |
| `Ctrl + Shift + Tab` | Previous tab |
| `1` – `6` | Open recent file from empty state |
| `Esc` | Close modal / overlay, exit slideshow, or exit fullscreen |

---

## 💻 Command Line Usage

```cmd
:: Open a blank window / empty state
LightPhotoViewer.exe

:: Open a specific image
LightPhotoViewer.exe "C:\Photos\Landscape.nef"

:: Open multiple images into tabs
LightPhotoViewer.exe photo1.png photo2.webp photo3.svg

:: Register file associations in user profile
LightPhotoViewer.exe /register

:: Silently register file associations
LightPhotoViewer.exe /register /silent

:: Remove all registered file associations
LightPhotoViewer.exe /unregister
```

---

## 🛠️ Building from Source

### Prerequisites
* Windows 10 (Build 1809+) or Windows 11 (64-bit)
* MSVC compiler supporting C++20 (Visual Studio 2022, Build Tools, or portable MSVC)
* Windows SDK 10.0.19041+

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
- C++20 standard (`/std:c++20`, `/permissive-`)
- Static C Runtime (`/MT`) for zero DLL dependencies
- Size-optimized code generation (`/O1`, `/GL`, `/LTCG`, `/Gy`, `/Gw`, `/OPT:REF`, `/OPT:ICF`)
- Embedded High-DPI manifest (`PerMonitorV2`, `longPathAware`)
- Precompiled headers (`pch.hpp`) for sub-second incremental builds.

---

## 📄 License

Distributed under the MIT License. Free and open source for personal and commercial use. See [LICENSE](LICENSE) for details.
