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

class ImageEngine {
public:
    static ImageEngine& Instance();

    bool Initialize();
    IWICImagingFactory* GetThreadWicFactory();

    std::shared_ptr<ImageDocument> LoadFromFile(const std::wstring& filePath);
    bool CopyToClipboard(HWND hWnd, const FrameData& frame);

private:
    ImageEngine() = default;
    ~ImageEngine() = default;

    ComPtr<IWICImagingFactory> m_mainWicFactory;
    bool m_initialized = false;

    void ParseExifMetadata(IWICMetadataQueryReader* reader, ImageMetadata& meta);
    WICBitmapTransformOptions GetExifTransform(IWICMetadataQueryReader* reader);
    bool LoadSvgMetadata(const std::wstring& filePath, ImageMetadata& meta);
};
