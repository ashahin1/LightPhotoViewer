#pragma once

#include "pch.hpp"

struct FrameData {
    UINT width = 0;
    UINT height = 0;
    UINT delayMs = 100;
    ComPtr<IWICBitmap> bitmap; // Eagerly decoded in-memory PBGRA bitmap (zero file locking)
};

struct ImageMetadata {
    std::wstring filePath;
    std::wstring fileName;
    uint64_t fileSizeBytes = 0;
    UINT width = 0;
    UINT height = 0;
    std::wstring formatName = L"Unknown";
    std::wstring cameraModel;
    std::wstring dateTaken;
    std::wstring exposure;
    std::wstring iso;
    bool isAnimated = false;
    bool isSvg = false;
    size_t frameCount = 1;
    UINT loopCount = 0; // 0 = loop forever
};

class ImageDocument {
public:
    ImageMetadata metadata;
    std::vector<FrameData> frames;
    bool isValid = false;

    UINT GetWidth() const { return metadata.width; }
    UINT GetHeight() const { return metadata.height; }
    size_t GetFrameCount() const { return frames.size(); }
    const FrameData* GetFrame(size_t index) const {
        if (index < frames.size()) return &frames[index];
        return frames.empty() ? nullptr : &frames[0];
    }
};

enum class ExportFormat : int {
    Jpeg = 0,   // .jpg  (GUID_ContainerFormatJpeg)
    Png  = 1,   // .png  (GUID_ContainerFormatPng)
    Bmp  = 2,   // .bmp  (GUID_ContainerFormatBmp)
    Tiff = 3,   // .tiff (GUID_ContainerFormatTiff)
    Gif  = 4,   // .gif  (GUID_ContainerFormatGif)
    Jxr  = 5    // .jxr  (GUID_ContainerFormatWmp)
};

enum class ExportMatte : int {
    KeepAlpha = 0, // Preserves alpha on PNG, TIFF, 32-bit BMP, JXR (falls back to White on JPEG)
    White     = 1, // Flatten alpha over #FFFFFF
    Black     = 2  // Flatten alpha over #000000
};

struct ExportFormatInfo {
    ExportFormat format;
    const wchar_t* displayName;
    const wchar_t* shortName;
    const wchar_t* defaultExt;
    const wchar_t* filterSpec;
    GUID containerGuid;
    bool supportsQuality;
    bool supportsAlpha;
};

struct ExportEncodeParams {
    ExportFormat format = ExportFormat::Jpeg;
    int quality = 92;               // 1..100
    UINT targetWidth = 0;           // 1..8192
    UINT targetHeight = 0;          // 1..8192
    int rotationDegrees = 0;        // 0, 90, 180, 270
    bool flipH = false;
    bool flipV = false;
    ExportMatte matte = ExportMatte::White;
};

class ImageEngine {
public:
    static ImageEngine& Instance();

    bool Initialize();
    IWICImagingFactory* GetThreadWicFactory();

    std::shared_ptr<ImageDocument> LoadFromFile(const std::wstring& filePath);
    bool CopyToClipboard(HWND hWnd, const FrameData& frame);
    static bool IsRawExtension(const std::wstring& ext);

    static const std::vector<ExportFormatInfo>& GetExportFormats();
    bool EncodeBitmapToMemory(
        IWICBitmapSource* rawSource,
        const std::vector<uint32_t>* preRasterizedPixels,
        UINT srcW,
        UINT srcH,
        const ExportEncodeParams& params,
        std::vector<BYTE>& outBytes
    );
    static bool WriteBytesToFileAtomic(const std::wstring& destPath, const std::vector<BYTE>& bytes);

private:
    ImageEngine() = default;
    ~ImageEngine() = default;

    ComPtr<IWICImagingFactory> m_mainWicFactory;
    bool m_initialized = false;

    void ParseExifMetadata(IWICMetadataQueryReader* reader, ImageMetadata& meta);
    WICBitmapTransformOptions GetExifTransform(IWICMetadataQueryReader* reader);
    bool LoadSvgMetadata(const std::wstring& filePath, ImageMetadata& meta);
    bool LoadRawEmbeddedPreview(IWICImagingFactory* factory, const std::wstring& filePath, ImageDocument& doc);
};
