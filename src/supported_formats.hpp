#pragma once

#include <string>

// Single source of truth for every file extension LightPhotoViewer can open.
// Used by folder navigation, the Open File dialog, and shell (/register) associations.
namespace SupportedFormats {

inline constexpr const wchar_t* kStandard[] = {
    L".jpg", L".jpeg", L".jpe", L".png", L".gif", L".webp",
    L".bmp", L".dib", L".ico", L".tiff", L".tif", L".avif",
    L".svg", L".heic", L".heif"
};

inline constexpr const wchar_t* kRaw[] = {
    L".cr2", L".cr3", L".crw",
    L".nef", L".nrw",
    L".arw", L".srf", L".sr2",
    L".dng",
    L".raf",
    L".orf", L".ori",
    L".rw2",
    L".pef", L".ptx",
    L".3fr", L".fff",
    L".rwl",
    L".x3f",
    L".srw",
    L".raw",
    L".kdc", L".dcr",
    L".mrw",
    L".mos",
    L".erf",
    L".mef"
};

// Builds a semicolon separated wildcard pattern such as "*.jpg;*.png" (for OPENFILENAME filters).
template <size_t N>
inline std::wstring BuildPattern(const wchar_t* const (&exts)[N]) {
    std::wstring out;
    for (size_t i = 0; i < N; ++i) {
        if (!out.empty()) out += L';';
        out += L'*';
        out += exts[i];
    }
    return out;
}

} // namespace SupportedFormats
