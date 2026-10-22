#pragma once

#include <string>
#include <vector>
#include <algorithm>

// Single source of truth for every file extension LightPhotoViewer can open.
// Used by folder navigation, the Open File dialog, and shell (/register) associations.
namespace SupportedFormats {

// Category-specific extension lists (all lowercase with leading dot)
inline constexpr const wchar_t* kJpeg[]     = { L".jpg", L".jpeg", L".jpe", L".jfif" };
inline constexpr const wchar_t* kPng[]      = { L".png" };
inline constexpr const wchar_t* kWebp[]     = { L".webp" };
inline constexpr const wchar_t* kGif[]      = { L".gif" };
inline constexpr const wchar_t* kBmp[]      = { L".bmp", L".dib" };
inline constexpr const wchar_t* kTiff[]     = { L".tiff", L".tif" };
inline constexpr const wchar_t* kSvg[]      = { L".svg" };
inline constexpr const wchar_t* kIco[]      = { L".ico" };
inline constexpr const wchar_t* kAvifHeif[] = { L".avif", L".heic", L".heif" };

inline constexpr const wchar_t* kStandard[] = {
    L".jpg", L".jpeg", L".jpe", L".jfif", L".png", L".gif", L".webp",
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

// Builds a semicolon-separated wildcard pattern such as "*.jpg;*.png"
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

// Builds a filter label with pattern such as "JPEG Images (*.jpg;*.jpeg;*.jpe;*.jfif)"
template <size_t N>
inline std::wstring BuildFilterLabel(const wchar_t* prefix, const wchar_t* const (&exts)[N]) {
    std::wstring out = prefix;
    out += L" (";
    out += BuildPattern(exts);
    out += L")";
    return out;
}

// Returns a flat vector of all supported extensions (e.g. for shell registration)
inline std::vector<std::wstring> GetAllExtensions() {
    std::vector<std::wstring> exts;
    exts.reserve(sizeof(kStandard) / sizeof(kStandard[0]) + sizeof(kRaw) / sizeof(kRaw[0]));
    for (const wchar_t* e : kStandard) exts.emplace_back(e);
    for (const wchar_t* e : kRaw) exts.emplace_back(e);
    return exts;
}

// Unified fast check if an extension is supported (accepts with/without leading dot, case-insensitive)
inline bool IsSupported(const std::wstring& ext) {
    if (ext.empty()) return false;
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (lower[0] != L'.') lower.insert(0, 1, L'.');

    for (const wchar_t* e : kStandard) {
        if (lower == e) return true;
    }
    for (const wchar_t* e : kRaw) {
        if (lower == e) return true;
    }
    return false;
}

} // namespace SupportedFormats
