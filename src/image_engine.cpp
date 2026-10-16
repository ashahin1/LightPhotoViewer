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
        IID_PPV_ARGS(&m_mainWicFactory)
    );

    if (SUCCEEDED(hr)) {
        m_initialized = true;
        return true;
    }
    return false;
}

IWICImagingFactory* ImageEngine::GetThreadWicFactory() {
    thread_local ComPtr<IWICImagingFactory> t_factory;
    if (!t_factory) {
        CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&t_factory)
        );
    }
    return t_factory.Get();
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

WICBitmapTransformOptions ImageEngine::GetExifTransform(IWICMetadataQueryReader* reader) {
    if (!reader) return WICBitmapTransformRotate0;

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

void ImageEngine::ParseExifMetadata(IWICMetadataQueryReader* reader, ImageMetadata& meta) {
    if (!reader) return;

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

bool ImageEngine::LoadSvgMetadata(const std::wstring& filePath, ImageMetadata& meta) {
    meta.isSvg = true;
    meta.formatName = L"SVG (Vector)";
    meta.width = 800;
    meta.height = 600;

    FILE* f = nullptr;
    if (_wfopen_s(&f, filePath.c_str(), L"rb") == 0 && f) {
        char buffer[4096] = {};
        size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, f);
        fclose(f);

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string content(buffer);

            // Look for viewBox="minX minY width height"
            size_t vbPos = content.find("viewBox=");
            if (vbPos != std::string::npos) {
                char quote = content[vbPos + 8];
                if (quote == '"' || quote == '\'') {
                    size_t endQuote = content.find(quote, vbPos + 9);
                    if (endQuote != std::string::npos) {
                        std::string vbStr = content.substr(vbPos + 9, endQuote - (vbPos + 9));
                        float minX = 0, minY = 0, vbW = 0, vbH = 0;
                        if (sscanf_s(vbStr.c_str(), "%f %f %f %f", &minX, &minY, &vbW, &vbH) == 4 ||
                            sscanf_s(vbStr.c_str(), "%f,%f,%f,%f", &minX, &minY, &vbW, &vbH) == 4) {
                            if (vbW > 0 && vbH > 0) {
                                meta.width = static_cast<UINT>(vbW + 0.5f);
                                meta.height = static_cast<UINT>(vbH + 0.5f);
                                return true;
                            }
                        }
                    }
                }
            }

            // Fallback: look for width="..." and height="..."
            size_t wPos = content.find("width=");
            size_t hPos = content.find("height=");
            if (wPos != std::string::npos && hPos != std::string::npos) {
                float wVal = 0, hVal = 0;
                if (sscanf_s(content.c_str() + wPos + 7, "%f", &wVal) == 1 &&
                    sscanf_s(content.c_str() + hPos + 8, "%f", &hVal) == 1) {
                    if (wVal > 0 && hVal > 0) {
                        meta.width = static_cast<UINT>(wVal + 0.5f);
                        meta.height = static_cast<UINT>(hVal + 0.5f);
                        return true;
                    }
                }
            }
        }
    }
    return true;
}

std::shared_ptr<ImageDocument> ImageEngine::LoadFromFile(const std::wstring& filePath) {
    IWICImagingFactory* factory = GetThreadWicFactory();
    if (!factory) return nullptr;

    auto doc = std::make_shared<ImageDocument>();
    doc->metadata.filePath = filePath;

    try {
        std::filesystem::path p(filePath);
        doc->metadata.fileName = p.filename().wstring();
        if (std::filesystem::exists(p)) {
            doc->metadata.fileSizeBytes = std::filesystem::file_size(p);
        }

        std::wstring ext = p.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext == L".svg") {
            if (LoadSvgMetadata(filePath, doc->metadata)) {
                doc->isValid = true;
                return doc;
            }
            return nullptr;
        }
    } catch (...) {
        doc->metadata.fileName = filePath;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        filePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
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

    bool isGif = IsEqualGUID(containerGuid, GUID_ContainerFormatGif) && (frameCount > 1);

    if (isGif) {
        // High-fidelity GIF89a Multi-Frame Compositor
        UINT canvasW = 0, canvasH = 0;

        // Query logical screen size
        ComPtr<IWICMetadataQueryReader> decoderMeta;
        if (SUCCEEDED(decoder->GetMetadataQueryReader(&decoderMeta))) {
            PROPVARIANT prop;
            PropVariantInit(&prop);
            if (SUCCEEDED(decoderMeta->GetMetadataByName(L"/logscrdesc/Width", &prop)) && prop.vt == VT_UI2 && prop.uiVal > 0) {
                canvasW = prop.uiVal;
            }
            PropVariantClear(&prop);
            if (SUCCEEDED(decoderMeta->GetMetadataByName(L"/logscrdesc/Height", &prop)) && prop.vt == VT_UI2 && prop.uiVal > 0) {
                canvasH = prop.uiVal;
            }
            PropVariantClear(&prop);
        }

        // Fallback to frame 0 size if logical screen descriptor missing
        if (canvasW == 0 || canvasH == 0) {
            ComPtr<IWICBitmapFrameDecode> f0;
            if (SUCCEEDED(decoder->GetFrame(0, &f0))) {
                f0->GetSize(&canvasW, &canvasH);
            }
        }
        if (canvasW == 0 || canvasH == 0) {
            canvasW = 100; canvasH = 100;
        }

        doc->metadata.width = canvasW;
        doc->metadata.height = canvasH;

        std::vector<uint32_t> canvas(canvasW * canvasH, 0);
        std::vector<uint32_t> prevCanvas = canvas;

        for (UINT i = 0; i < frameCount; ++i) {
            ComPtr<IWICBitmapFrameDecode> frameDecode;
            if (FAILED(decoder->GetFrame(i, &frameDecode))) continue;

            UINT subW = 0, subH = 0;
            frameDecode->GetSize(&subW, &subH);
            if (subW == 0 || subH == 0) continue;

            UINT frameLeft = 0;
            UINT frameTop = 0;
            UINT delayMs = 100;
            unsigned char disposal = 0;

            ComPtr<IWICMetadataQueryReader> frameMeta;
            if (SUCCEEDED(frameDecode->GetMetadataQueryReader(&frameMeta))) {
                PROPVARIANT prop;
                PropVariantInit(&prop);
                if (SUCCEEDED(frameMeta->GetMetadataByName(L"/imgdesc/Left", &prop)) && prop.vt == VT_UI2) {
                    frameLeft = prop.uiVal;
                }
                PropVariantClear(&prop);
                if (SUCCEEDED(frameMeta->GetMetadataByName(L"/imgdesc/Top", &prop)) && prop.vt == VT_UI2) {
                    frameTop = prop.uiVal;
                }
                PropVariantClear(&prop);
                if (SUCCEEDED(frameMeta->GetMetadataByName(L"/grctlext/Delay", &prop)) && prop.vt == VT_UI2) {
                    delayMs = prop.uiVal * 10;
                    if (delayMs < 20) delayMs = 100;
                }
                PropVariantClear(&prop);
                if (SUCCEEDED(frameMeta->GetMetadataByName(L"/grctlext/Disposal", &prop)) && (prop.vt == VT_UI1 || prop.vt == VT_UI2)) {
                    disposal = static_cast<unsigned char>(prop.vt == VT_UI1 ? prop.bVal : prop.uiVal);
                }
                PropVariantClear(&prop);
            }

            // Save canvas if disposal mode requires restoring to previous
            if (disposal == 3) {
                prevCanvas = canvas;
            }

            ComPtr<IWICFormatConverter> converter;
            if (SUCCEEDED(factory->CreateFormatConverter(&converter))) {
                if (SUCCEEDED(converter->Initialize(
                    frameDecode.Get(),
                    GUID_WICPixelFormat32bppPBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom))) {

                    std::vector<uint32_t> subPixels(subW * subH);
                    WICRect rcSub = { 0, 0, static_cast<INT>(subW), static_cast<INT>(subH) };
                    if (SUCCEEDED(converter->CopyPixels(&rcSub, subW * 4, static_cast<UINT>(subPixels.size() * 4), reinterpret_cast<BYTE*>(subPixels.data())))) {
                        // Blend subPixels onto canvas
                        for (UINT y = 0; y < subH; ++y) {
                            UINT cy = frameTop + y;
                            if (cy >= canvasH) break;
                            for (UINT x = 0; x < subW; ++x) {
                                UINT cx = frameLeft + x;
                                if (cx >= canvasW) break;

                                uint32_t srcP = subPixels[y * subW + x];
                                uint32_t srcA = (srcP >> 24) & 0xFF;
                                if (srcA == 255) {
                                    canvas[cy * canvasW + cx] = srcP;
                                } else if (srcA > 0) {
                                    uint32_t dstP = canvas[cy * canvasW + cx];
                                    uint32_t invA = 255 - srcA;

                                    uint32_t srcB = srcP & 0xFF;
                                    uint32_t srcG = (srcP >> 8) & 0xFF;
                                    uint32_t srcR = (srcP >> 16) & 0xFF;

                                    uint32_t dstB = dstP & 0xFF;
                                    uint32_t dstG = (dstP >> 8) & 0xFF;
                                    uint32_t dstR = (dstP >> 16) & 0xFF;
                                    uint32_t dstA = (dstP >> 24) & 0xFF;

                                    uint32_t outB = srcB + ((dstB * invA) / 255);
                                    uint32_t outG = srcG + ((dstG * invA) / 255);
                                    uint32_t outR = srcR + ((dstR * invA) / 255);
                                    uint32_t outA = srcA + ((dstA * invA) / 255);

                                    canvas[cy * canvasW + cx] = (outA << 24) | (outR << 16) | (outG << 8) | outB;
                                }
                            }
                        }

                        // Create in-memory snapshot bitmap
                        ComPtr<IWICBitmap> frameBmp;
                        if (SUCCEEDED(factory->CreateBitmapFromMemory(
                            canvasW,
                            canvasH,
                            GUID_WICPixelFormat32bppPBGRA,
                            canvasW * 4,
                            static_cast<UINT>(canvas.size() * 4),
                            reinterpret_cast<BYTE*>(canvas.data()),
                            &frameBmp))) {

                            FrameData fd;
                            fd.width = canvasW;
                            fd.height = canvasH;
                            fd.delayMs = delayMs;
                            fd.bitmap = frameBmp;
                            doc->frames.push_back(std::move(fd));
                        }

                        // Apply disposal for subsequent frames
                        if (disposal == 2) { // Restore to background
                            for (UINT y = 0; y < subH; ++y) {
                                UINT cy = frameTop + y;
                                if (cy >= canvasH) break;
                                for (UINT x = 0; x < subW; ++x) {
                                    UINT cx = frameLeft + x;
                                    if (cx >= canvasW) break;
                                    canvas[cy * canvasW + cx] = 0;
                                }
                            }
                        } else if (disposal == 3) { // Restore to previous
                            canvas = prevCanvas;
                        }
                    }
                }
            }
        }
    } else {
        // Standard Image Decoding (JPEG, PNG, WebP, BMP, TIFF, ICO)
        for (UINT i = 0; i < frameCount; ++i) {
            ComPtr<IWICBitmapFrameDecode> frameDecode;
            if (FAILED(decoder->GetFrame(i, &frameDecode))) continue;

            UINT w = 0, h = 0;
            frameDecode->GetSize(&w, &h);
            if (w == 0 || h == 0) continue;

            ComPtr<IWICMetadataQueryReader> metaReader;
            frameDecode->GetMetadataQueryReader(&metaReader);

            if (i == 0) {
                doc->metadata.width = w;
                doc->metadata.height = h;
                if (metaReader) {
                    ParseExifMetadata(metaReader.Get(), doc->metadata);
                }
            }

            WICBitmapTransformOptions transform = GetExifTransform(metaReader.Get());
            ComPtr<IWICBitmapSource> sourceBitmap;

            if (transform != WICBitmapTransformRotate0) {
                ComPtr<IWICBitmapFlipRotator> rotator;
                if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rotator))) {
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
            hr = factory->CreateFormatConverter(&converter);
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
                // EAGER MATERIALIZATION: Decodes pixels into system memory immediately
                // so the file handle on disk is closed immediately on return.
                ComPtr<IWICBitmap> inMemoryBmp;
                hr = factory->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &inMemoryBmp);
                if (SUCCEEDED(hr) && inMemoryBmp) {
                    UINT outW = 0, outH = 0;
                    inMemoryBmp->GetSize(&outW, &outH);

                    FrameData fd;
                    fd.width = outW;
                    fd.height = outH;
                    fd.delayMs = 100;
                    fd.bitmap = inMemoryBmp;
                    doc->frames.push_back(std::move(fd));
                }
            }
        }
    }

    if (!doc->frames.empty()) {
        doc->isValid = true;
        return doc;
    }
    return nullptr;
}

bool ImageEngine::CopyToClipboard(HWND hWnd, const FrameData& frame) {
    if (!frame.bitmap) return false;

    UINT width = frame.width;
    UINT height = frame.height;
    if (width == 0 || height == 0) return false;

    UINT stride = width * 4;
    UINT bufferSize = stride * height;
    std::vector<BYTE> pixelBuffer(bufferSize);

    WICRect rc = { 0, 0, static_cast<INT>(width), static_cast<INT>(height) };
    HRESULT hr = frame.bitmap->CopyPixels(&rc, stride, bufferSize, pixelBuffer.data());
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
