#include "pch.hpp"
#include "image_engine.hpp"

ImageEngine& ImageEngine::Instance() {
    static ImageEngine s_instance;
    return s_instance;
}

bool ImageEngine::Initialize() {
    if (m_initialized) return true;

    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_wicFactory)
    );

    if (SUCCEEDED(hr)) {
        m_initialized = true;
        return true;
    }
    return false;
}

static std::wstring FormatFromContainer(const GUID& guid) {
    if (IsEqualGUID(guid, GUID_ContainerFormatBmp)) return L"BMP";
    if (IsEqualGUID(guid, GUID_ContainerFormatPng)) return L"PNG";
    if (IsEqualGUID(guid, GUID_ContainerFormatIco)) return L"ICO";
    if (IsEqualGUID(guid, GUID_ContainerFormatJpeg)) return L"JPEG";
    if (IsEqualGUID(guid, GUID_ContainerFormatTiff)) return L"TIFF";
    if (IsEqualGUID(guid, GUID_ContainerFormatGif)) return L"GIF";
    if (IsEqualGUID(guid, GUID_ContainerFormatWmp)) return L"HD Photo";
    if (IsEqualGUID(guid, GUID_ContainerFormatDds)) return L"DDS";
    if (IsEqualGUID(guid, GUID_ContainerFormatWebp)) return L"WebP";
    if (IsEqualGUID(guid, GUID_ContainerFormatHeif)) return L"HEIF";
    return L"Image";
}

WICBitmapTransformOptions ImageEngine::GetExifTransform(IWICBitmapFrameDecode* frame) {
    if (!frame) return WICBitmapTransformRotate0;

    ComPtr<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(&reader))) {
        return WICBitmapTransformRotate0;
    }

    PROPVARIANT value;
    PropVariantInit(&value);

    // Try standard query paths for orientation
    // /app1/ifd/{ushort=274} or /ifd/{ushort=274}
    HRESULT hr = reader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &value);
    if (FAILED(hr)) {
        hr = reader->GetMetadataByName(L"/ifd/{ushort=274}", &value);
    }

    unsigned short orientation = 1;
    if (SUCCEEDED(hr) && value.vt == VT_UI2) {
        orientation = value.uiVal;
    }
    PropVariantClear(&value);

    switch (orientation) {
        case 2: return WICBitmapTransformFlipHorizontal;
        case 3: return WICBitmapTransformRotate180;
        case 4: return WICBitmapTransformFlipVertical;
        case 5: return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
        case 6: return WICBitmapTransformRotate90;
        case 7: return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
        case 8: return WICBitmapTransformRotate270;
        default: return WICBitmapTransformRotate0;
    }
}

void ImageEngine::ParseExifMetadata(IWICBitmapFrameDecode* frame, ImageMetadata& meta) {
    if (!frame) return;

    ComPtr<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(&reader))) return;

    PROPVARIANT val;

    // Camera Model
    PropVariantInit(&val);
    if (SUCCEEDED(reader->GetMetadataByName(L"/app1/ifd/{ushort=272}", &val)) && (val.vt == VT_LPSTR || val.vt == VT_LPWSTR)) {
        if (val.vt == VT_LPWSTR && val.pwszVal) meta.cameraModel = val.pwszVal;
        else if (val.vt == VT_LPSTR && val.pszVal) {
            int len = MultiByteToWideChar(CP_ACP, 0, val.pszVal, -1, nullptr, 0);
            if (len > 0) {
                meta.cameraModel.resize(len - 1);
                MultiByteToWideChar(CP_ACP, 0, val.pszVal, -1, &meta.cameraModel[0], len);
            }
        }
    }
    PropVariantClear(&val);

    // Date Taken
    PropVariantInit(&val);
    if (SUCCEEDED(reader->GetMetadataByName(L"/app1/ifd/{ushort=306}", &val)) && (val.vt == VT_LPSTR || val.vt == VT_LPWSTR)) {
        if (val.vt == VT_LPWSTR && val.pwszVal) meta.dateTaken = val.pwszVal;
        else if (val.vt == VT_LPSTR && val.pszVal) {
            int len = MultiByteToWideChar(CP_ACP, 0, val.pszVal, -1, nullptr, 0);
            if (len > 0) {
                meta.dateTaken.resize(len - 1);
                MultiByteToWideChar(CP_ACP, 0, val.pszVal, -1, &meta.dateTaken[0], len);
            }
        }
    }
    PropVariantClear(&val);

    // ISO Speed
    PropVariantInit(&val);
    if (SUCCEEDED(reader->GetMetadataByName(L"/app1/ifd/exif/{ushort=34855}", &val)) && val.vt == VT_UI2) {
        meta.iso = L"ISO " + std::to_wstring(val.uiVal);
    }
    PropVariantClear(&val);

    // Exposure Time
    PropVariantInit(&val);
    if (SUCCEEDED(reader->GetMetadataByName(L"/app1/ifd/exif/{ushort=33434}", &val))) {
        if (val.vt == VT_R8 && val.dblVal > 0.0) {
            if (val.dblVal < 1.0) {
                int denom = static_cast<int>(1.0 / val.dblVal + 0.5);
                meta.exposure = L"1/" + std::to_wstring(denom) + L"s";
            } else {
                meta.exposure = std::to_wstring(static_cast<int>(val.dblVal)) + L"s";
            }
        }
    }
    PropVariantClear(&val);
}

std::shared_ptr<ImageDocument> ImageEngine::LoadFromFile(const std::wstring& filePath) {
    if (!Initialize()) return nullptr;

    auto doc = std::make_shared<ImageDocument>();
    doc->metadata.filePath = filePath;

    // File name and size
    try {
        std::filesystem::path p(filePath);
        doc->metadata.fileName = p.filename().wstring();
        if (std::filesystem::exists(p)) {
            doc->metadata.fileSizeBytes = std::filesystem::file_size(p);
        }
    } catch (...) {
        doc->metadata.fileName = filePath;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = m_wicFactory->CreateDecoderFromFilename(
        filePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder
    );

    if (FAILED(hr) || !decoder) {
        return nullptr;
    }

    GUID containerGuid = GUID_NULL;
    decoder->GetContainerFormat(&containerGuid);
    doc->metadata.formatName = FormatFromContainer(containerGuid);

    UINT frameCount = 0;
    if (FAILED(decoder->GetFrameCount(&frameCount)) || frameCount == 0) {
        return nullptr;
    }

    doc->metadata.frameCount = frameCount;
    doc->metadata.isAnimated = (frameCount > 1);

    // Check GIF global loop count
    if (doc->metadata.isAnimated) {
        ComPtr<IWICMetadataQueryReader> globalReader;
        if (SUCCEEDED(decoder->GetMetadataQueryReader(&globalReader))) {
            PROPVARIANT prop;
            PropVariantInit(&prop);
            if (SUCCEEDED(globalReader->GetMetadataByName(L"/appext/Application", &prop))) {
                // Application identifier
            }
            PropVariantClear(&prop);
        }
    }

    for (UINT i = 0; i < frameCount; ++i) {
        ComPtr<IWICBitmapFrameDecode> frameDecode;
        if (FAILED(decoder->GetFrame(i, &frameDecode))) continue;

        UINT w = 0, h = 0;
        frameDecode->GetSize(&w, &h);
        if (w == 0 || h == 0) continue;

        if (i == 0) {
            doc->metadata.width = w;
            doc->metadata.height = h;
            ParseExifMetadata(frameDecode.Get(), doc->metadata);
        }

        UINT delayMs = 100; // default 100ms
        if (doc->metadata.isAnimated) {
            ComPtr<IWICMetadataQueryReader> metaReader;
            if (SUCCEEDED(frameDecode->GetMetadataQueryReader(&metaReader))) {
                PROPVARIANT propVal;
                PropVariantInit(&propVal);
                if (SUCCEEDED(metaReader->GetMetadataByName(L"/grctlext/Delay", &propVal)) && propVal.vt == VT_UI2) {
                    delayMs = propVal.uiVal * 10;
                    if (delayMs < 20) delayMs = 100;
                }
                PropVariantClear(&propVal);
            }
        }

        // Apply EXIF rotation if needed (mainly for JPEG)
        WICBitmapTransformOptions transform = GetExifTransform(frameDecode.Get());
        ComPtr<IWICBitmapSource> sourceBitmap;

        if (transform != WICBitmapTransformRotate0) {
            ComPtr<IWICBitmapFlipRotator> rotator;
            if (SUCCEEDED(m_wicFactory->CreateBitmapFlipRotator(&rotator))) {
                if (SUCCEEDED(rotator->Initialize(frameDecode.Get(), transform))) {
                    sourceBitmap = rotator;
                    if (i == 0 && (transform == WICBitmapTransformRotate90 || transform == WICBitmapTransformRotate270)) {
                        doc->metadata.width = h;
                        doc->metadata.height = w;
                    }
                }
            }
        }

        if (!sourceBitmap) {
            sourceBitmap = frameDecode;
        }

        ComPtr<IWICFormatConverter> converter;
        hr = m_wicFactory->CreateFormatConverter(&converter);
        if (FAILED(hr)) continue;

        hr = converter->Initialize(
            sourceBitmap.Get(),
            GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom
        );

        if (SUCCEEDED(hr)) {
            UINT outW = 0, outH = 0;
            converter->GetSize(&outW, &outH);

            FrameData fd;
            fd.width = outW;
            fd.height = outH;
            fd.delayMs = delayMs;
            fd.converter = converter;
            doc->frames.push_back(std::move(fd));
        }
    }

    if (!doc->frames.empty()) {
        doc->isValid = true;
        return doc;
    }
    return nullptr;
}

bool ImageEngine::CopyToClipboard(HWND hWnd, const FrameData& frame) {
    if (!frame.converter) return false;

    UINT width = frame.width;
    UINT height = frame.height;
    if (width == 0 || height == 0) return false;

    UINT stride = width * 4;
    UINT bufferSize = stride * height;
    std::vector<BYTE> pixelBuffer(bufferSize);

    WICRect rc = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    HRESULT hr = frame.converter->CopyPixels(&rc, stride, bufferSize, pixelBuffer.data());
    if (FAILED(hr)) return false;

    // Allocate global memory for standard CF_DIB (bottom-up BGRA)
    size_t dibSize = sizeof(BITMAPINFOHEADER) + bufferSize;
    HGLOBAL hMem = GlobalAlloc(GHND | GMEM_MOVEABLE, dibSize);
    if (!hMem) return false;

    BYTE* pMem = static_cast<BYTE*>(GlobalLock(hMem));
    if (!pMem) {
        GlobalFree(hMem);
        return false;
    }

    BITMAPINFOHEADER* bih = reinterpret_cast<BITMAPINFOHEADER*>(pMem);
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = static_cast<LONG>(width);
    bih->biHeight = static_cast<LONG>(height); // positive = bottom-up
    bih->biPlanes = 1;
    bih->biBitCount = 32;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = bufferSize;

    BYTE* pPixels = pMem + sizeof(BITMAPINFOHEADER);
    // Flip rows to bottom-up for standard DIB format
    for (UINT y = 0; y < height; ++y) {
        const BYTE* srcRow = pixelBuffer.data() + (height - 1 - y) * stride;
        BYTE* dstRow = pPixels + y * stride;
        memcpy(dstRow, srcRow, stride);
    }

    GlobalUnlock(hMem);

    if (!OpenClipboard(hWnd)) {
        GlobalFree(hMem);
        return false;
    }

    EmptyClipboard();
    SetClipboardData(CF_DIB, hMem);
    CloseClipboard();
    return true;
}
