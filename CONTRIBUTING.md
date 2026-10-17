# Contributing to LightPhotoViewer

Thank you for your interest in contributing to **LightPhotoViewer**!

## Core Principles & Design Constraints

Before submitting contributions, please keep in mind the fundamental design goals of the project:

1. **Extreme Performance**: Sub-250ms cold launch, instantaneous tab switching, and low memory profile.
2. **Zero External Runtime Dependencies**: Standalone single `.exe` utilizing only native Windows 10/11 Win32, Direct2D 1.1, Direct3D 11, and WIC APIs. No external DLLs.
3. **Strict Size Budget**: Total executable size must strictly remain under **500 KB**.
4. **Clean Code & Toolchain**: Pure C++20 (`/std:c++20`), static runtime library (`/MT`), warning-free compilation (`/W4`).

---

## Development Setup

### Prerequisites
- Windows 10 (Build 1809+) or Windows 11 (64-bit).
- MSVC compiler supporting C++20 (Visual Studio 2022, VS Build Tools, or portable MSVC).

### Building Locally

```cmd
:: Build incrementally
build.bat

:: Clean rebuild
build.bat /rebuild

:: Clean artifacts
build.bat /clean
```

---

## Testing & Benchmarks

Always run the automated verification scripts before submitting a pull request:

```powershell
# Run the image format test suite
powershell -ExecutionPolicy Bypass -File tools\test_all.ps1

# Run single-instance forwarding test
powershell -ExecutionPolicy Bypass -File tools\test_forwarding.ps1

# Run shell registration test
powershell -ExecutionPolicy Bypass -File tools\test_registration.ps1

# Run performance & binary footprint benchmark
powershell -ExecutionPolicy Bypass -File tools\benchmark.ps1
```

---

## Coding Guidelines

- Format all C++ code using the root `.clang-format` rules.
- Maintain `PerMonitorV2` High-DPI scaling accuracy.
- Ensure all resources (`IStream`, `IWICBitmap`, `ID2D1Bitmap`, `HANDLE`) are safely managed and released immediately when closed to prevent file locking.
- Do not introduce runtime DLL dependencies or heavy third-party frameworks.
