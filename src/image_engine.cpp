#include "pch.hpp"
#include "image_engine.hpp"
#include "supported_formats.hpp"

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

bool ImageEngine::IsRawExtension(const std::wstring& ext) {
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (!lower.empty() && lower[0] == L'.') {
        lower = lower.substr(1);
    }
    static const std::unordered_set<std::wstring> s_rawExts = [] {
        std::unordered_set<std::wstring> set;
        for (const wchar_t* e : SupportedFormats::kRaw) {
            set.insert(e + 1); // strip leading '.'
        }
        return set;
    }();
    return s_rawExts.find(lower) != s_rawExts.end();
}

static std::wstring FormatFromContainer(const GUID& guid, const std::wstring& ext = L"") {
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
    if (IsEqualGUID(guid, GUID_ContainerFormatAdng)) return L"DNG";
    if (IsEqualGUID(guid, GUID_ContainerFormatRaw) || (!ext.empty() && ImageEngine::IsRawExtension(ext))) {
        if (!ext.empty()) {
            std::wstring cleanExt = ext;
            if (!cleanExt.empty() && cleanExt[0] == L'.') cleanExt = cleanExt.substr(1);
            std::transform(cleanExt.begin(), cleanExt.end(), cleanExt.begin(), ::towupper);
            return L"RAW (" + cleanExt + L")";
        }
        return L"RAW";
    }
    return L"Image";
}

static double ReadExifRational(const PROPVARIANT& pv) {
    if (pv.vt == VT_R8) return pv.dblVal;
    if (pv.vt == VT_R4) return static_cast<double>(pv.fltVal);
    if (pv.vt == VT_UI8) {
        ULONG num = pv.uhVal.LowPart;
        ULONG den = pv.uhVal.HighPart;
        return (den != 0) ? (static_cast<double>(num) / static_cast<double>(den)) : 0.0;
    }
    if (pv.vt == VT_I8) {
        LONG num = pv.hVal.LowPart;
        LONG den = pv.hVal.HighPart;
        return (den != 0) ? (static_cast<double>(num) / static_cast<double>(den)) : 0.0;
    }
    return 0.0;
}

WICBitmapTransformOptions ImageEngine::GetExifTransform(IWICMetadataQueryReader* reader) {
    if (!reader) return WICBitmapTransformRotate0;

    PROPVARIANT value;
    PropVariantInit(&value);

    // Try standard query paths for orientation
    // /app1/ifd/{ushort=274}, /ifd/{ushort=274}, or /ifd/exif/{ushort=274}
    HRESULT hr = reader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &value);
    if (FAILED(hr)) {
        hr = reader->GetMetadataByName(L"/ifd/{ushort=274}", &value);
    }
    if (FAILED(hr)) {
        hr = reader->GetMetadataByName(L"/ifd/exif/{ushort=274}", &value);
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

    auto QueryStr = [&](const wchar_t* p1, const wchar_t* p2, std::wstring& out) {
        PROPVARIANT val;
        PropVariantInit(&val);
        HRESULT hr = reader->GetMetadataByName(p1, &val);
        if (FAILED(hr) && p2) {
            hr = reader->GetMetadataByName(p2, &val);
        }
        if (SUCCEEDED(hr)) {
            if (val.vt == VT_LPWSTR && val.pwszVal) {
                out = val.pwszVal;
            } else if (val.vt == VT_LPSTR && val.pszVal) {
                int len = MultiByteToWideChar(CP_ACP, 0, val.pszVal, -1, nullptr, 0);
                if (len > 1) {
                    out.resize(len);
                    MultiByteToWideChar(CP_ACP, 0, val.pszVal, -1, out.data(), len);
                    out.pop_back(); // Remove null terminator from std::wstring
                }
            }
        }
        PropVariantClear(&val);
    };

    auto Trim = [](std::wstring& s) {
        while (!s.empty() && (s.back() == L' ' || s.back() == L'\t' || s.back() == L'\r' || s.back() == L'\n' || s.back() == L'\0')) {
            s.pop_back();
        }
        size_t start = 0;
        while (start < s.size() && (s[start] == L' ' || s[start] == L'\t' || s[start] == L'\r' || s[start] == L'\n')) {
            start++;
        }
        if (start > 0) s = s.substr(start);
    };

    // Camera Make and Model
    std::wstring make, model;
    QueryStr(L"/app1/ifd/{ushort=271}", L"/ifd/{ushort=271}", make);
    QueryStr(L"/app1/ifd/{ushort=272}", L"/ifd/{ushort=272}", model);
    Trim(make);
    Trim(model);

    if (!model.empty()) {
        if (!make.empty()) {
            std::wstring lowerMake = make, lowerModel = model;
            std::transform(lowerMake.begin(), lowerMake.end(), lowerMake.begin(), ::towlower);
            std::transform(lowerModel.begin(), lowerModel.end(), lowerModel.begin(), ::towlower);
            if (lowerModel.rfind(lowerMake, 0) == 0) {
                meta.cameraModel = model;
            } else {
                meta.cameraModel = make + L" " + model;
            }
        } else {
            meta.cameraModel = model;
        }
    } else if (!make.empty()) {
        meta.cameraModel = make;
    }

    // Date Taken
    QueryStr(L"/app1/ifd/{ushort=306}", L"/ifd/{ushort=306}", meta.dateTaken);
    Trim(meta.dateTaken);

    // ISO Speed
    auto QueryUInt = [&](const wchar_t* p1, const wchar_t* p2, UINT& outVal) -> bool {
        PROPVARIANT val;
        PropVariantInit(&val);
        HRESULT hr = reader->GetMetadataByName(p1, &val);
        if (FAILED(hr) && p2) {
            hr = reader->GetMetadataByName(p2, &val);
        }
        bool found = false;
        if (SUCCEEDED(hr)) {
            if (val.vt == VT_UI2) { outVal = val.uiVal; found = true; }
            else if (val.vt == VT_UI4) { outVal = val.ulVal; found = true; }
        }
        PropVariantClear(&val);
        return found;
    };

    UINT isoVal = 0;
    if (QueryUInt(L"/app1/ifd/exif/{ushort=34855}", L"/ifd/exif/{ushort=34855}", isoVal) && isoVal > 0) {
        meta.iso = L"ISO " + std::to_wstring(isoVal);
    }

    // Exposure Time, Aperture, and Focal Length
    auto QueryRational = [&](const wchar_t* p1, const wchar_t* p2) -> double {
        PROPVARIANT val;
        PropVariantInit(&val);
        HRESULT hr = reader->GetMetadataByName(p1, &val);
        if (FAILED(hr) && p2) {
            hr = reader->GetMetadataByName(p2, &val);
        }
        double res = 0.0;
        if (SUCCEEDED(hr)) {
            res = ReadExifRational(val);
        }
        PropVariantClear(&val);
        return res;
    };

    double expTime = QueryRational(L"/app1/ifd/exif/{ushort=33434}", L"/ifd/exif/{ushort=33434}");
    double fNum    = QueryRational(L"/app1/ifd/exif/{ushort=33437}", L"/ifd/exif/{ushort=33437}");
    double focal   = QueryRational(L"/app1/ifd/exif/{ushort=37386}", L"/ifd/exif/{ushort=37386}");

    std::wstring shutterStr;
    if (expTime > 0.0) {
        if (expTime < 1.0) {
            int denom = static_cast<int>(1.0 / expTime + 0.5);
            shutterStr = L"1/" + std::to_wstring(denom) + L"s";
        } else {
            wchar_t buf[16];
            if (expTime == static_cast<int>(expTime)) {
                swprintf_s(buf, L"%ds", static_cast<int>(expTime));
            } else {
                swprintf_s(buf, L"%.1fs", expTime);
            }
            shutterStr = buf;
        }
    }

    std::wstring fStr;
    if (fNum > 0.0) {
        wchar_t buf[16];
        swprintf_s(buf, L"f/%.1f", fNum);
        fStr = buf;
    }

    std::wstring focalStr;
    if (focal > 0.0) {
        wchar_t buf[16];
        if (focal == static_cast<int>(focal)) {
            swprintf_s(buf, L"%dmm", static_cast<int>(focal));
        } else {
            swprintf_s(buf, L"%.1fmm", focal);
        }
        focalStr = buf;
    }

    std::wstring exposureCombined;
    if (!shutterStr.empty()) exposureCombined += shutterStr;
    if (!fStr.empty()) {
        if (!exposureCombined.empty()) exposureCombined += L", ";
        exposureCombined += fStr;
    }
    if (!focalStr.empty()) {
        if (!exposureCombined.empty()) exposureCombined += L", ";
        exposureCombined += focalStr;
    }

    if (!exposureCombined.empty()) {
        meta.exposure = exposureCombined;
    }
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

bool ImageEngine::LoadRawEmbeddedPreview(IWICImagingFactory* factory, const std::wstring& filePath, ImageDocument& doc) {
    if (!factory) return false;

    FILE* f = nullptr;
    if (_wfopen_s(&f, filePath.c_str(), L"rb") != 0 || !f) {
        return false;
    }

    _fseeki64(f, 0, SEEK_END);
    int64_t fileSize = _ftelli64(f);
    if (fileSize < 4096) {
        fclose(f);
        return false;
    }
    _fseeki64(f, 0, SEEK_SET);

    size_t sz = static_cast<size_t>(fileSize);
    std::vector<uint8_t> buf(sz);
    size_t bytesRead = fread(buf.data(), 1, sz, f);
    fclose(f);
    if (bytesRead < 4096) return false;

    UINT bestW = 0, bestH = 0;
    size_t bestOffset = 0;

    // Scan for embedded JPEG streams (FF D8 FF ...)
    for (size_t i = 0; i + 16 < bytesRead; ++i) {
        if (buf[i] == 0xFF && buf[i + 1] == 0xD8 && buf[i + 2] == 0xFF) {
            uint8_t m = buf[i + 3];
            if (m == 0xDB || m == 0xE0 || m == 0xE1 || m == 0xE2 || m == 0xC0 || m == 0xC2 || m == 0xC4 || m == 0xEE) {
                ComPtr<IStream> stm;
                stm.Attach(SHCreateMemStream(buf.data() + i, static_cast<UINT>(bytesRead - i)));
                if (stm) {
                    ComPtr<IWICBitmapDecoder> dec;
                    if (SUCCEEDED(CoCreateInstance(CLSID_WICJpegDecoder, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dec)))) {
                        if (SUCCEEDED(dec->Initialize(stm.Get(), WICDecodeMetadataCacheOnDemand))) {
                            ComPtr<IWICBitmapFrameDecode> f0;
                            if (SUCCEEDED(dec->GetFrame(0, &f0))) {
                                UINT w = 0, h = 0;
                                if (SUCCEEDED(f0->GetSize(&w, &h)) && w >= 160 && h >= 120) {
                                    if (static_cast<uint64_t>(w) * h > static_cast<uint64_t>(bestW) * bestH) {
                                        bestW = w;
                                        bestH = h;
                                        bestOffset = i;
                                    }
                                    i += 4096;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (bestW == 0 || bestH == 0) {
        return false;
    }

    // Decode the best embedded JPEG stream
    ComPtr<IStream> bestStream;
    bestStream.Attach(SHCreateMemStream(buf.data() + bestOffset, static_cast<UINT>(bytesRead - bestOffset)));
    if (!bestStream) return false;

    ComPtr<IWICBitmapDecoder> bestDec;
    if (FAILED(CoCreateInstance(CLSID_WICJpegDecoder, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&bestDec)))) {
        return false;
    }
    if (FAILED(bestDec->Initialize(bestStream.Get(), WICDecodeMetadataCacheOnLoad))) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> f0;
    if (FAILED(bestDec->GetFrame(0, &f0))) {
        return false;
    }

    ComPtr<IWICMetadataQueryReader> metaReader;
    f0->GetMetadataQueryReader(&metaReader);
    if (metaReader) {
        ParseExifMetadata(metaReader.Get(), doc.metadata);
    }

    WICBitmapTransformOptions transform = GetExifTransform(metaReader.Get());
    ComPtr<IWICBitmapSource> sourceBitmap;

    if (transform != WICBitmapTransformRotate0) {
        ComPtr<IWICBitmapFlipRotator> rotator;
        if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rotator))) {
            if (SUCCEEDED(rotator->Initialize(f0.Get(), transform))) {
                sourceBitmap = rotator;
            }
        }
    }
    if (!sourceBitmap) {
        sourceBitmap = f0;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return false;
    if (FAILED(converter->Initialize(
        sourceBitmap.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom))) {
        return false;
    }

    ComPtr<IWICBitmap> inMemoryBmp;
    if (FAILED(factory->CreateBitmapFromSource(converter.Get(), WICBitmapCacheOnLoad, &inMemoryBmp)) || !inMemoryBmp) {
        return false;
    }

    UINT outW = 0, outH = 0;
    inMemoryBmp->GetSize(&outW, &outH);
    doc.metadata.width = outW;
    doc.metadata.height = outH;

    std::filesystem::path p(filePath);
    std::wstring ext = p.extension().wstring();
    doc.metadata.formatName = FormatFromContainer(GUID_ContainerFormatRaw, ext);
    doc.metadata.frameCount = 1;
    doc.metadata.isAnimated = false;

    FrameData fd;
    fd.width = outW;
    fd.height = outH;
    fd.delayMs = 100;
    fd.bitmap = inMemoryBmp;
    doc.frames.clear();
    doc.frames.push_back(std::move(fd));
    doc.isValid = true;
    return true;
}

std::shared_ptr<ImageDocument> ImageEngine::LoadFromFile(const std::wstring& filePath) {
    IWICImagingFactory* factory = GetThreadWicFactory();
    if (!factory) return nullptr;

    auto doc = std::make_shared<ImageDocument>();
    doc->metadata.filePath = filePath;

    std::wstring ext;
    try {
        std::filesystem::path p(filePath);
        doc->metadata.fileName = p.filename().wstring();
        if (std::filesystem::exists(p)) {
            doc->metadata.fileSizeBytes = std::filesystem::file_size(p);
        }

        ext = p.extension().wstring();
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

    bool isRawExt = IsRawExtension(ext);

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = factory->CreateDecoderFromFilename(
        filePath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );

    if (FAILED(hr) || !decoder) {
        if (isRawExt) {
            if (LoadRawEmbeddedPreview(factory, filePath, *doc)) {
                return doc;
            }
        }
        return nullptr;
    }

    GUID containerGuid = GUID_NULL;
    decoder->GetContainerFormat(&containerGuid);

    // If DNG decoder only returned a low-res thumbnail, try explicit CLSID_WICRAWDecoder
    if (IsEqualGUID(containerGuid, GUID_ContainerFormatAdng)) {
        ComPtr<IWICBitmapFrameDecode> testF0;
        UINT tw = 0, th = 0;
        if (SUCCEEDED(decoder->GetFrame(0, &testF0)) && SUCCEEDED(testF0->GetSize(&tw, &th))) {
            if (std::max(tw, th) < 1024) {
                ComPtr<IWICBitmapDecoder> rawDec;
                if (SUCCEEDED(CoCreateInstance(CLSID_WICRAWDecoder, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawDec)))) {
                    ComPtr<IWICStream> stream;
                    if (SUCCEEDED(factory->CreateStream(&stream)) &&
                        SUCCEEDED(stream->InitializeFromFilename(filePath.c_str(), GENERIC_READ))) {
                        if (SUCCEEDED(rawDec->Initialize(stream.Get(), WICDecodeMetadataCacheOnLoad))) {
                            decoder = rawDec;
                            decoder->GetContainerFormat(&containerGuid);
                        }
                    }
                }
            }
        }
    }

    doc->metadata.formatName = FormatFromContainer(containerGuid, ext);

    bool isRaw = IsEqualGUID(containerGuid, GUID_ContainerFormatRaw) || IsEqualGUID(containerGuid, GUID_ContainerFormatAdng) || isRawExt;

    UINT frameCount = 1;
    if (!isRaw) {
        if (FAILED(decoder->GetFrameCount(&frameCount)) || frameCount == 0) {
            return nullptr;
        }
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
    } else if (isRaw) {
        ComPtr<IWICBitmapFrameDecode> frame0;
        decoder->GetFrame(0, &frame0);

        ComPtr<IWICMetadataQueryReader> metaReader;
        if (frame0) {
            frame0->GetMetadataQueryReader(&metaReader);
            if (metaReader) {
                ParseExifMetadata(metaReader.Get(), doc->metadata);
            }
        }

        WICBitmapTransformOptions transform = GetExifTransform(metaReader.Get());
        ComPtr<IWICBitmapSource> sourceBitmap;

        // Check if decoder provides a high-resolution preview
        ComPtr<IWICBitmapSource> previewSource;
        if (SUCCEEDED(decoder->GetPreview(&previewSource)) && previewSource) {
            UINT pw = 0, ph = 0;
            if (SUCCEEDED(previewSource->GetSize(&pw, &ph)) && std::max(pw, ph) >= 1280) {
                sourceBitmap = previewSource;
            }
        }

        if (!sourceBitmap && frame0) {
            sourceBitmap = frame0;
        }

        bool isPreview = (previewSource && sourceBitmap.Get() == previewSource.Get());
        bool decoded = false;
        if (sourceBitmap) {
            ComPtr<IWICFormatConverter> converter;
            if (SUCCEEDED(factory->CreateFormatConverter(&converter))) {
                if (SUCCEEDED(converter->Initialize(
                    sourceBitmap.Get(),
                    GUID_WICPixelFormat32bppPBGRA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0.0,
                    WICBitmapPaletteTypeCustom))) {

                    ComPtr<IWICBitmapSource> finalSource = converter;
                    ComPtr<IWICBitmapFlipRotator> rotator;
                    if (transform != WICBitmapTransformRotate0 && !isPreview) {
                        if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rotator))) {
                            if (SUCCEEDED(rotator->Initialize(converter.Get(), transform))) {
                                finalSource = rotator;
                            }
                        }
                    }

                    ComPtr<IWICBitmap> inMemoryBmp;
                    if (SUCCEEDED(factory->CreateBitmapFromSource(finalSource.Get(), WICBitmapCacheOnLoad, &inMemoryBmp)) && inMemoryBmp) {
                        UINT outW = 0, outH = 0;
                        inMemoryBmp->GetSize(&outW, &outH);

                        doc->metadata.width = outW;
                        doc->metadata.height = outH;

                        FrameData fd;
                        fd.width = outW;
                        fd.height = outH;
                        fd.delayMs = 100;
                        fd.bitmap = inMemoryBmp;
                        doc->frames.push_back(std::move(fd));
                        decoded = true;
                    }
                }
            }
        }

        if (!decoded) {
            if (LoadRawEmbeddedPreview(factory, filePath, *doc)) {
                return doc;
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

const std::vector<ExportFormatInfo>& ImageEngine::GetExportFormats() {
    static const std::vector<ExportFormatInfo> s_formats = {
        { ExportFormat::Jpeg, L"JPEG Image (.jpg)",     L"JPEG", L".jpg",  L"JPEG Image (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0", GUID_ContainerFormatJpeg, true,  false },
        { ExportFormat::Png,  L"PNG Image (.png)",      L"PNG",  L".png",  L"PNG Image (*.png)\0*.png\0",                GUID_ContainerFormatPng,  false, true  },
        { ExportFormat::Bmp,  L"BMP Image (.bmp)",      L"BMP",  L".bmp",  L"BMP Image (*.bmp)\0*.bmp\0",                GUID_ContainerFormatBmp,  false, true  },
        { ExportFormat::Tiff, L"TIFF Image (.tiff)",    L"TIFF", L".tiff", L"TIFF Image (*.tiff;*.tif)\0*.tiff;*.tif\0", GUID_ContainerFormatTiff, false, true  },
        { ExportFormat::Gif,  L"GIF Image (.gif)",      L"GIF",  L".gif",  L"GIF Image (*.gif)\0*.gif\0",                GUID_ContainerFormatGif,  false, false },
        { ExportFormat::Jxr,  L"JPEG XR Image (.jxr)",  L"JXR",  L".jxr",  L"JPEG XR Image (*.jxr;*.wdp)\0*.jxr;*.wdp\0", GUID_ContainerFormatWmp, true,  true  }
    };
    return s_formats;
}

bool ImageEngine::EncodeBitmapToMemory(
    IWICBitmapSource* rawSource,
    const std::vector<uint32_t>* preRasterizedPixels,
    UINT srcW,
    UINT srcH,
    const ExportEncodeParams& params,
    std::vector<BYTE>& outBytes
) {
    outBytes.clear();
    if (srcW == 0 || srcH == 0) return false;
    if (!rawSource && (!preRasterizedPixels || preRasterizedPixels->size() < static_cast<size_t>(srcW) * srcH)) {
        return false;
    }

    IWICImagingFactory* factory = GetThreadWicFactory();
    if (!factory) return false;

    // 1. Obtain un-premultiplied 32bppBGRA source pixels
    std::vector<uint32_t> srcPixels(static_cast<size_t>(srcW) * srcH);
    if (preRasterizedPixels && preRasterizedPixels->size() >= srcPixels.size()) {
        memcpy(srcPixels.data(), preRasterizedPixels->data(), srcPixels.size() * sizeof(uint32_t));
    } else if (rawSource) {
        ComPtr<IWICFormatConverter> srcConv;
        if (FAILED(factory->CreateFormatConverter(&srcConv))) return false;
        if (FAILED(srcConv->Initialize(
                rawSource,
                GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0f,
                WICBitmapPaletteTypeCustom))) {
            return false;
        }
        WICRect rc = { 0, 0, static_cast<INT>(srcW), static_cast<INT>(srcH) };
        if (FAILED(srcConv->CopyPixels(
                &rc,
                srcW * 4,
                static_cast<UINT>(srcPixels.size() * sizeof(uint32_t)),
                reinterpret_cast<BYTE*>(srcPixels.data())))) {
            return false;
        }
    }

    // 2. Apply orientation transforms (rotation + horizontal/vertical flip)
    int rot = ((params.rotationDegrees % 360) + 360) % 360;
    bool isRot90 = (rot == 90 || rot == 270);
    UINT orientedW = isRot90 ? srcH : srcW;
    UINT orientedH = isRot90 ? srcW : srcH;

    std::vector<uint32_t> orientedPixels(static_cast<size_t>(orientedW) * orientedH);
    if (rot == 0 && !params.flipH && !params.flipV) {
        orientedPixels = std::move(srcPixels);
    } else {
        for (UINT y = 0; y < orientedH; ++y) {
            for (UINT x = 0; x < orientedW; ++x) {
                UINT ox = params.flipH ? (orientedW - 1 - x) : x;
                UINT oy = params.flipV ? (orientedH - 1 - y) : y;

                UINT sx = ox;
                UINT sy = oy;
                switch (rot) {
                    case 90:  sx = oy; sy = srcH - 1 - ox; break;
                    case 180: sx = srcW - 1 - ox; sy = srcH - 1 - oy; break;
                    case 270: sx = srcW - 1 - oy; sy = ox; break;
                    default:  break;
                }
                orientedPixels[static_cast<size_t>(y) * orientedW + x] =
                    srcPixels[static_cast<size_t>(sy) * srcW + sx];
            }
        }
    }

    // 3. Apply resolution scaling if target dimensions differ from oriented dimensions
    UINT targetW = (params.targetWidth > 0) ? std::clamp(params.targetWidth, 1U, 8192U) : orientedW;
    UINT targetH = (params.targetHeight > 0) ? std::clamp(params.targetHeight, 1U, 8192U) : orientedH;

    std::vector<uint32_t> finalPixels;
    if (targetW == orientedW && targetH == orientedH) {
        finalPixels = std::move(orientedPixels);
    } else {
        ComPtr<IWICBitmap> orientedBmp;
        if (FAILED(factory->CreateBitmapFromMemory(
                orientedW,
                orientedH,
                GUID_WICPixelFormat32bppBGRA,
                orientedW * 4,
                static_cast<UINT>(orientedPixels.size() * sizeof(uint32_t)),
                reinterpret_cast<BYTE*>(orientedPixels.data()),
                &orientedBmp))) {
            return false;
        }

        ComPtr<IWICBitmapScaler> scaler;
        if (FAILED(factory->CreateBitmapScaler(&scaler))) return false;
        if (FAILED(scaler->Initialize(orientedBmp.Get(), targetW, targetH, WICBitmapInterpolationModeHighQualityCubic))) {
            return false;
        }

        finalPixels.resize(static_cast<size_t>(targetW) * targetH);
        WICRect rcScaled = { 0, 0, static_cast<INT>(targetW), static_cast<INT>(targetH) };
        if (FAILED(scaler->CopyPixels(
                &rcScaled,
                targetW * 4,
                static_cast<UINT>(finalPixels.size() * sizeof(uint32_t)),
                reinterpret_cast<BYTE*>(finalPixels.data())))) {
            return false;
        }
    }

    // 4. Apply Alpha Matte Flattening if needed
    const auto& formats = GetExportFormats();
    int fmtIdx = std::clamp(static_cast<int>(params.format), 0, static_cast<int>(formats.size() - 1));
    const ExportFormatInfo& fmtInfo = formats[fmtIdx];

    ExportMatte effectiveMatte = params.matte;
    if (!fmtInfo.supportsAlpha && effectiveMatte == ExportMatte::KeepAlpha) {
        effectiveMatte = ExportMatte::White;
    }

    if (effectiveMatte != ExportMatte::KeepAlpha) {
        const uint32_t matteVal = (effectiveMatte == ExportMatte::White) ? 255U : 0U;
        BYTE* px = reinterpret_cast<BYTE*>(finalPixels.data());
        size_t totalPx = finalPixels.size();
        for (size_t i = 0; i < totalPx; ++i) {
            uint32_t b = px[i * 4 + 0];
            uint32_t g = px[i * 4 + 1];
            uint32_t r = px[i * 4 + 2];
            uint32_t a = px[i * 4 + 3];
            if (a < 255U) {
                uint32_t invA = 255U - a;
                px[i * 4 + 0] = static_cast<BYTE>((b * a + matteVal * invA + 127U) / 255U);
                px[i * 4 + 1] = static_cast<BYTE>((g * a + matteVal * invA + 127U) / 255U);
                px[i * 4 + 2] = static_cast<BYTE>((r * a + matteVal * invA + 127U) / 255U);
                px[i * 4 + 3] = 255;
            }
        }
    }

    ComPtr<IWICBitmap> finalWicBmp;
    if (FAILED(factory->CreateBitmapFromMemory(
            targetW,
            targetH,
            GUID_WICPixelFormat32bppBGRA,
            targetW * 4,
            static_cast<UINT>(finalPixels.size() * sizeof(uint32_t)),
            reinterpret_cast<BYTE*>(finalPixels.data()),
            &finalWicBmp))) {
        return false;
    }

    // 5. Create growable in-memory IStream
    ComPtr<IStream> memStream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream)) || !memStream) {
        return false;
    }

    // 6. Initialize WIC Encoder & Frame
    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(fmtInfo.containerGuid, nullptr, &encoder)) || !encoder) {
        return false;
    }
    if (FAILED(encoder->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) {
        return false;
    }

    ComPtr<IWICBitmapFrameEncode> frameEncode;
    ComPtr<IPropertyBag2> propBag;
    if (FAILED(encoder->CreateNewFrame(&frameEncode, &propBag)) || !frameEncode) {
        return false;
    }

    // 7. Configure encoder properties
    if (propBag) {
        if (fmtInfo.supportsQuality) {
            PROPBAG2 option = {};
            option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT varVal;
            VariantInit(&varVal);
            varVal.vt = VT_R4;
            varVal.fltVal = static_cast<float>(std::clamp(params.quality, 1, 100)) / 100.0f;
            propBag->Write(1, &option, &varVal);
        } else if (params.format == ExportFormat::Tiff) {
            PROPBAG2 option = {};
            option.pstrName = const_cast<LPOLESTR>(L"TiffCompressionMethod");
            VARIANT varVal;
            VariantInit(&varVal);
            varVal.vt = VT_UI1;
            varVal.bVal = static_cast<BYTE>(WICTiffCompressionLZW);
            propBag->Write(1, &option, &varVal);
        }
    }

    if (FAILED(frameEncode->Initialize(propBag.Get()))) return false;
    if (FAILED(frameEncode->SetSize(targetW, targetH))) return false;

    // 8. Configure pixel format & palette and write frame
    if (params.format == ExportFormat::Gif) {
        WICPixelFormatGUID pf = GUID_WICPixelFormat8bppIndexed;
        if (FAILED(frameEncode->SetPixelFormat(&pf))) return false;

        ComPtr<IWICPalette> palette;
        if (FAILED(factory->CreatePalette(&palette))) return false;
        if (FAILED(palette->InitializeFromBitmap(finalWicBmp.Get(), 256, FALSE))) return false;
        frameEncode->SetPalette(palette.Get());

        ComPtr<IWICFormatConverter> conv;
        if (FAILED(factory->CreateFormatConverter(&conv))) return false;
        if (FAILED(conv->Initialize(
                finalWicBmp.Get(),
                pf,
                WICBitmapDitherTypeErrorDiffusion,
                palette.Get(),
                0.0f,
                WICBitmapPaletteTypeCustom))) {
            return false;
        }
        if (FAILED(frameEncode->WriteSource(conv.Get(), nullptr))) return false;
    } else {
        WICPixelFormatGUID pf = (effectiveMatte == ExportMatte::KeepAlpha && fmtInfo.supportsAlpha)
            ? GUID_WICPixelFormat32bppBGRA
            : GUID_WICPixelFormat24bppBGR;
        if (FAILED(frameEncode->SetPixelFormat(&pf))) return false;

        ComPtr<IWICFormatConverter> conv;
        if (FAILED(factory->CreateFormatConverter(&conv))) return false;
        if (FAILED(conv->Initialize(
                finalWicBmp.Get(),
                pf,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0f,
                WICBitmapPaletteTypeCustom))) {
            return false;
        }
        if (FAILED(frameEncode->WriteSource(conv.Get(), nullptr))) return false;
    }

    // 9. Commit & extract encoded bytes
    if (FAILED(frameEncode->Commit())) return false;
    if (FAILED(encoder->Commit())) return false;

    STATSTG stat = {};
    if (FAILED(memStream->Stat(&stat, STATFLAG_NONAME)) || stat.cbSize.QuadPart == 0) {
        return false;
    }

    HGLOBAL hGlobal = nullptr;
    if (FAILED(GetHGlobalFromStream(memStream.Get(), &hGlobal)) || !hGlobal) {
        return false;
    }

    const void* pData = GlobalLock(hGlobal);
    if (!pData) return false;

    outBytes.resize(static_cast<size_t>(stat.cbSize.QuadPart));
    memcpy(outBytes.data(), pData, outBytes.size());
    GlobalUnlock(hGlobal);

    return true;
}

bool ImageEngine::WriteBytesToFileAtomic(const std::wstring& destPath, const std::vector<BYTE>& bytes) {
    if (destPath.empty() || bytes.empty()) return false;
    std::wstring tmpPath = destPath + L".lpv_tmp";

    HANDLE hFile = CreateFileW(
        tmpPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    if (!ok || written != static_cast<DWORD>(bytes.size())) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    if (!MoveFileExW(tmpPath.c_str(), destPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }
    return true;
}
