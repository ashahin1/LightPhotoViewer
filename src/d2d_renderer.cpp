#include "pch.hpp"
#include "d2d_renderer.hpp"

D2DRenderer::D2DRenderer() {
}

D2DRenderer::~D2DRenderer() {
    DiscardDeviceResources();
}

void D2DRenderer::UpdateDpi(float dpi) {
    m_dpiScale = dpi / 90.0f; // baseline
    if (m_dpiScale < 0.5f) m_dpiScale = 1.0f;
    CreateDeviceIndependentResources();
    if (m_d2dContext && m_d2dTargetBitmap) {
        RecreateTargetBitmap();
    }
}

bool D2DRenderer::CreateDeviceIndependentResources() {
    D2D1_FACTORY_OPTIONS options = {};
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, reinterpret_cast<void**>(m_d2dFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(m_dwriteFactory.ReleaseAndGetAddressOf()));
    if (FAILED(hr)) return false;

    // DirectWrite Formats
    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f * m_dpiScale,
        L"en-US",
        &m_formatTab
    );
    if (m_formatTab) {
        m_formatTab->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        m_formatTab->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.0f * m_dpiScale,
        L"en-US",
        &m_formatHud
    );
    if (m_formatHud) {
        m_formatHud->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_formatHud->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f * m_dpiScale,
        L"en-US",
        &m_formatInfo
    );

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        24.0f * m_dpiScale,
        L"en-US",
        &m_formatTitle
    );
    if (m_formatTitle) {
        m_formatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f * m_dpiScale,
        L"en-US",
        &m_formatSubtitle
    );
    if (m_formatSubtitle) {
        m_formatSubtitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        13.0f * m_dpiScale,
        L"en-US",
        &m_formatHelpCat
    );

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.0f * m_dpiScale,
        L"en-US",
        &m_formatKeycap
    );
    if (m_formatKeycap) {
        m_formatKeycap->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_formatKeycap->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    m_dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        11.5f * m_dpiScale,
        L"en-US",
        &m_formatKeyDesc
    );
    if (m_formatKeyDesc) {
        m_formatKeyDesc->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    return true;
}

bool D2DRenderer::CreateDeviceResources() {
    if (m_d3dDevice) return true;

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL actualLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_d3dDevice,
        &actualLevel,
        &m_d3dContext
    );

    if (FAILED(hr)) {
        // Fallback to WARP software rasterizer if hardware unavailable
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            creationFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &m_d3dDevice,
            &actualLevel,
            &m_d3dContext
        );
        if (FAILED(hr)) return false;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr)) return false;

    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return false;

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) return false;

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    m_width = std::max(1L, rc.right - rc.left);
    m_height = std::max(1L, rc.bottom - rc.top);

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width = m_width;
    scDesc.Height = m_height;
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = dxgiFactory->CreateSwapChainForHwnd(
        m_d3dDevice.Get(),
        m_hWnd,
        &scDesc,
        nullptr,
        nullptr,
        &m_swapChain
    );
    if (FAILED(hr)) return false;

    RecreateTargetBitmap();

    // Create Brushes
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x121214, 1.0f), &m_brushBg);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x1e1e24, 0.92f), &m_brushChrome);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x06b6d4, 1.0f), &m_brushAccent); // Electric Cyan
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0xf8fafc, 1.0f), &m_brushText);   // White/Slate-50
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x94a3b8, 1.0f), &m_brushTextMuted); // Slate-400
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x334155, 0.8f), &m_brushBorder); // Slate-700

    return true;
}

void D2DRenderer::DiscardDeviceResources() {
    m_d2dTargetBitmap.Reset();
    m_brushBg.Reset();
    m_brushChrome.Reset();
    m_brushAccent.Reset();
    m_brushText.Reset();
    m_brushTextMuted.Reset();
    m_brushBorder.Reset();
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_swapChain.Reset();
    m_d3dContext.Reset();
    m_d3dDevice.Reset();
}

void D2DRenderer::RecreateTargetBitmap() {
    if (!m_swapChain || !m_d2dContext) return;

    m_d2dContext->SetTarget(nullptr);
    m_d2dTargetBitmap.Reset();

    ComPtr<IDXGISurface> dxgiBackBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
    if (FAILED(hr)) return;

    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f * m_dpiScale,
        96.0f * m_dpiScale
    );

    hr = m_d2dContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer.Get(), &bmpProps, &m_d2dTargetBitmap);
    if (SUCCEEDED(hr)) {
        m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
    }
}

bool D2DRenderer::Initialize(HWND hWnd) {
    m_hWnd = hWnd;

    UINT dpi = GetDpiForWindow(hWnd);
    m_dpiScale = (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;

    if (!CreateDeviceIndependentResources()) return false;
    return CreateDeviceResources();
}

void D2DRenderer::Resize(UINT width, UINT height) {
    if (width == 0 || height == 0) return;
    m_width = width;
    m_height = height;

    if (m_swapChain && m_d2dContext) {
        m_d2dContext->SetTarget(nullptr);
        m_d2dTargetBitmap.Reset();

        HRESULT hr = m_swapChain->ResizeBuffers(2, m_width, m_height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            DiscardDeviceResources();
            CreateDeviceResources();
        } else {
            RecreateTargetBitmap();
        }
    }
}

int D2DRenderer::HitTestTab(POINT pt, const TabManager& tabManager, bool& outCloseButton) {
    outCloseButton = false;
    if (!tabManager.ShouldShowTabs()) return -1;

    float tabH = GetTabStripHeight();
    if (pt.y < 0 || pt.y > tabH) return -1;

    size_t count = tabManager.GetTabCount();
    if (count == 0) return -1;

    float clientW = static_cast<float>(m_width);
    float maxTabW = 200.0f * m_dpiScale;
    float tabW = std::min(maxTabW, clientW / static_cast<float>(count));

    for (size_t i = 0; i < count; ++i) {
        float x1 = i * tabW;
        float x2 = x1 + tabW;

        if (pt.x >= x1 && pt.x < x2) {
            float closeSize = 16.0f * m_dpiScale;
            float closeX1 = x2 - closeSize - 8.0f * m_dpiScale;
            float closeX2 = x2 - 8.0f * m_dpiScale;
            float closeY1 = (tabH - closeSize) / 2.0f;
            float closeY2 = closeY1 + closeSize;

            if (pt.x >= closeX1 && pt.x <= closeX2 && pt.y >= closeY1 && pt.y <= closeY2) {
                outCloseButton = true;
            }
            return static_cast<int>(i);
        }
    }
    return -1;
}

HudButtonId D2DRenderer::HitTestHud(POINT pt) {
    for (const auto& btn : m_hudButtons) {
        if (pt.x >= btn.rect.left && pt.x <= btn.rect.right &&
            pt.y >= btn.rect.top && pt.y <= btn.rect.bottom) {
            return btn.id;
        }
    }
    return HudButtonId::None;
}

bool D2DRenderer::HitTestHelpClose(POINT pt) {
    return (pt.x >= m_helpCloseRect.left && pt.x <= m_helpCloseRect.right &&
            pt.y >= m_helpCloseRect.top && pt.y <= m_helpCloseRect.bottom);
}

bool D2DRenderer::HitTestHelpCard(POINT pt) {
    return (pt.x >= m_helpCardRect.left && pt.x <= m_helpCardRect.right &&
            pt.y >= m_helpCardRect.top && pt.y <= m_helpCardRect.bottom);
}

void D2DRenderer::Render(TabManager& tabManager, float hudAlpha, bool showExif, bool showHelp, POINT mousePt) {
    if (!CreateDeviceResources()) return;

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0x121214, 1.0f));

    TabItem* activeTab = tabManager.GetActiveTab();
    float tabStripHeight = tabManager.ShouldShowTabs() ? GetTabStripHeight() : 0.0f;
    float viewportHeight = static_cast<float>(m_height) - tabStripHeight;

    if (activeTab && activeTab->document && activeTab->document->isValid) {
        RenderImageCanvas(activeTab, tabStripHeight, viewportHeight);
    } else {
        RenderEmptyState();
    }

    if (tabManager.ShouldShowTabs()) {
        RenderTabs(tabManager, mousePt);
    }

    if (activeTab && activeTab->document && activeTab->document->isValid) {
        RenderHud(hudAlpha, mousePt);
        if (showExif) {
            RenderExifOverlay(activeTab);
        }
    }

    if (showHelp) {
        RenderHelpOverlay(mousePt);
    } else {
        m_helpCardRect = { 0, 0, 0, 0 };
        m_helpCloseRect = { 0, 0, 0, 0 };
    }

    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        DiscardDeviceResources();
    } else {
        m_swapChain->Present(1, 0);
    }
}

void D2DRenderer::RenderTabs(TabManager& tabManager, POINT mousePt) {
    size_t count = tabManager.GetTabCount();
    if (count == 0) return;

    float tabH = GetTabStripHeight();
    float clientW = static_cast<float>(m_width);
    float maxTabW = 200.0f * m_dpiScale;
    float tabW = std::min(maxTabW, clientW / static_cast<float>(count));

    // Background strip
    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 1.0f));
    m_d2dContext->FillRectangle(D2D1::RectF(0, 0, clientW, tabH), m_brushChrome.Get());

    // Bottom border line
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(0, tabH), D2D1::Point2F(clientW, tabH), m_brushBorder.Get(), 1.0f);

    int activeIdx = tabManager.GetActiveIndex();

    for (size_t i = 0; i < count; ++i) {
        const TabItem* tab = tabManager.GetTab(i);
        if (!tab) continue;

        float x1 = i * tabW;
        float x2 = x1 + tabW;
        D2D1_RECT_F tabRect = D2D1::RectF(x1, 0, x2, tabH);

        bool isActive = (static_cast<int>(i) == activeIdx);
        bool isHovered = (mousePt.x >= x1 && mousePt.x < x2 && mousePt.y >= 0 && mousePt.y <= tabH);

        if (isActive) {
            m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 1.0f));
            m_d2dContext->FillRectangle(tabRect, m_brushChrome.Get());

            // Electric Cyan active indicator bar at top of active tab
            m_brushAccent->SetColor(D2D1::ColorF(0x06b6d4, 1.0f));
            m_d2dContext->FillRectangle(D2D1::RectF(x1, 0, x2, 2.5f * m_dpiScale), m_brushAccent.Get());
        } else if (isHovered) {
            m_brushChrome->SetColor(D2D1::ColorF(0x202024, 0.8f));
            m_d2dContext->FillRectangle(tabRect, m_brushChrome.Get());
        }

        // Tab Separator
        if (!isActive && i + 1 != static_cast<size_t>(activeIdx)) {
            m_d2dContext->DrawLine(
                D2D1::Point2F(x2, 8.0f * m_dpiScale),
                D2D1::Point2F(x2, tabH - 8.0f * m_dpiScale),
                m_brushBorder.Get(),
                1.0f
            );
        }

        // Tab Title Text
        float textPadLeft = 12.0f * m_dpiScale;
        float textPadRight = 28.0f * m_dpiScale;
        D2D1_RECT_F textRect = D2D1::RectF(x1 + textPadLeft, 0, x2 - textPadRight, tabH);

        m_brushText->SetColor(isActive ? D2D1::ColorF(0xf8fafc) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(
            tab->title.c_str(),
            static_cast<UINT32>(tab->title.length()),
            m_formatTab.Get(),
            textRect,
            m_brushText.Get(),
            D2D1_DRAW_TEXT_OPTIONS_CLIP
        );

        // Close Button ('x')
        float closeSize = 16.0f * m_dpiScale;
        float closeX1 = x2 - closeSize - 8.0f * m_dpiScale;
        float closeX2 = x2 - 8.0f * m_dpiScale;
        float closeY1 = (tabH - closeSize) / 2.0f;
        float closeY2 = closeY1 + closeSize;
        D2D1_RECT_F closeRect = D2D1::RectF(closeX1, closeY1, closeX2, closeY2);

        bool closeHover = (mousePt.x >= closeX1 && mousePt.x <= closeX2 && mousePt.y >= closeY1 && mousePt.y <= closeY2);
        if (closeHover) {
            m_brushChrome->SetColor(D2D1::ColorF(0xef4444, 0.8f));
            m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(closeRect, 3.0f, 3.0f), m_brushChrome.Get());
            m_brushText->SetColor(D2D1::ColorF(0xffffff));
        } else {
            m_brushText->SetColor(D2D1::ColorF(0x64748b));
        }

        const wchar_t closeIcon[] = L"✕";
        m_d2dContext->DrawText(closeIcon, 1, m_formatHud.Get(), closeRect, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
}

void D2DRenderer::RenderImageCanvas(TabItem* tab, float viewportTop, float viewportHeight) {
    if (!tab || !tab->document) return;

    size_t frameCount = tab->document->GetFrameCount();
    if (frameCount == 0) return;

    // Cache Direct2D Bitmaps on demand
    if (tab->gpuBitmaps.size() < frameCount) {
        tab->gpuBitmaps.resize(frameCount);
    }

    size_t activeFrameIdx = tab->currentFrame % frameCount;
    if (!tab->gpuBitmaps[activeFrameIdx]) {
        const FrameData* fd = tab->document->GetFrame(activeFrameIdx);
        if (fd && fd->converter) {
            HRESULT hr = m_d2dContext->CreateBitmapFromWicBitmap(fd->converter.Get(), nullptr, &tab->gpuBitmaps[activeFrameIdx]);
            if (FAILED(hr)) return;
        }
    }

    ComPtr<ID2D1Bitmap1> bitmap = tab->gpuBitmaps[activeFrameIdx];
    if (!bitmap) return;

    D2D1_SIZE_F imgSize = bitmap->GetSize();
    float imgW = imgSize.width;
    float imgH = imgSize.height;

    // Viewport dimensions
    float vpW = static_cast<float>(m_width);
    float vpH = viewportHeight;
    float centerX = vpW / 2.0f;
    float centerY = viewportTop + vpH / 2.0f;

    // Account for 90 or 270 degree rotation swapping visual width/height
    bool isRotated90 = (tab->rotationDegrees == 90 || tab->rotationDegrees == 270);
    float visualW = isRotated90 ? imgH : imgW;
    float visualH = isRotated90 ? imgW : imgH;

    if (tab->isFitToWindow) {
        float pad = 24.0f * m_dpiScale;
        float availW = std::max(10.0f, vpW - pad);
        float availH = std::max(10.0f, vpH - pad);

        float fitZoom = std::min(availW / visualW, availH / visualH);
        tab->zoom = fitZoom;
        tab->panX = centerX;
        tab->panY = centerY;
    }

    // Clip rendering to the image viewport so it doesn't draw under the tab strip
    D2D1_RECT_F clipRect = D2D1::RectF(0, viewportTop, vpW, viewportTop + vpH);
    m_d2dContext->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Build transformation matrix
    D2D1::Matrix3x2F transform =
        D2D1::Matrix3x2F::Scale(tab->flipH ? -1.0f : 1.0f, tab->flipV ? -1.0f : 1.0f) *
        D2D1::Matrix3x2F::Rotation(static_cast<float>(tab->rotationDegrees)) *
        D2D1::Matrix3x2F::Scale(tab->zoom, tab->zoom) *
        D2D1::Matrix3x2F::Translation(tab->panX, tab->panY);

    m_d2dContext->SetTransform(transform);

    // Destination rect centered on origin (since translation moves it to pan position)
    D2D1_RECT_F destRect = D2D1::RectF(-imgW / 2.0f, -imgH / 2.0f, imgW / 2.0f, imgH / 2.0f);

    // Interpolation selection:
    // High quality cubic for downsampling to prevent aliasing
    // Nearest neighbor at high zoom (>400%) for pixel sharpness
    D2D1_INTERPOLATION_MODE interpMode;
    if (tab->zoom < 0.99f) {
        interpMode = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
    } else if (tab->zoom >= 4.0f) {
        interpMode = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
    } else {
        interpMode = D2D1_INTERPOLATION_MODE_LINEAR;
    }

    m_d2dContext->DrawBitmap(bitmap.Get(), destRect, 1.0f, interpMode);

    // Optional pixel grid when zoomed >= 800%
    if (tab->zoom >= 8.0f) {
        m_brushBorder->SetColor(D2D1::ColorF(0xffffff, 0.15f));
        for (float x = -imgW / 2.0f; x <= imgW / 2.0f; x += 1.0f) {
            m_d2dContext->DrawLine(D2D1::Point2F(x, -imgH / 2.0f), D2D1::Point2F(x, imgH / 2.0f), m_brushBorder.Get(), 0.5f / tab->zoom);
        }
        for (float y = -imgH / 2.0f; y <= imgH / 2.0f; y += 1.0f) {
            m_d2dContext->DrawLine(D2D1::Point2F(-imgW / 2.0f, y), D2D1::Point2F(imgW / 2.0f, y), m_brushBorder.Get(), 0.5f / tab->zoom);
        }
    }

    // Reset transform & pop clip
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
    m_d2dContext->PopAxisAlignedClip();
}

void D2DRenderer::RenderHud(float hudAlpha, POINT mousePt) {
    if (hudAlpha < 0.02f) {
        m_hudButtons.clear();
        return;
    }

    float vpW = static_cast<float>(m_width);
    float vpH = static_cast<float>(m_height);

    float btnW = 38.0f * m_dpiScale;
    float btnH = 36.0f * m_dpiScale;
    float gap = 4.0f * m_dpiScale;

    struct BtnSpec {
        HudButtonId id;
        const wchar_t* label;
        float customWidth = 0.0f;
    };

    BtnSpec specs[] = {
        { HudButtonId::Prev,      L"◀" },
        { HudButtonId::Next,      L"▶" },
        { HudButtonId::ZoomOut,   L"−" },
        { HudButtonId::Zoom100,   L"1:1", 44.0f * m_dpiScale },
        { HudButtonId::ZoomIn,    L"+" },
        { HudButtonId::ZoomFit,   L"Fit", 44.0f * m_dpiScale },
        { HudButtonId::RotateCCW, L"↶" },
        { HudButtonId::RotateCW,  L"↷" },
        { HudButtonId::Info,      L"ℹ" },
        { HudButtonId::Delete,    L"🗑" }
    };

    float totalBtnsW = 0.0f;
    for (const auto& s : specs) {
        float w = (s.customWidth > 0.0f) ? s.customWidth : btnW;
        totalBtnsW += w + gap;
    }
    totalBtnsW -= gap;

    float hudPadH = 10.0f * m_dpiScale;
    float hudPadV = 6.0f * m_dpiScale;
    float hudW = totalBtnsW + hudPadH * 2.0f;
    float hudH = btnH + hudPadV * 2.0f;

    float hudX = (vpW - hudW) / 2.0f;
    float hudY = vpH - hudH - 24.0f * m_dpiScale;

    m_hudRect = D2D1::RectF(hudX, hudY, hudX + hudW, hudY + hudH);

    // Draw Frosted Pill Background
    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.88f * hudAlpha));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_hudRect, hudH / 2.0f, hudH / 2.0f), m_brushChrome.Get());

    // Pill Subtle Border
    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.6f * hudAlpha));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(m_hudRect, hudH / 2.0f, hudH / 2.0f), m_brushBorder.Get(), 1.0f);

    m_hudButtons.clear();
    float currentX = hudX + hudPadH;
    float currentY = hudY + hudPadV;

    for (const auto& s : specs) {
        float w = (s.customWidth > 0.0f) ? s.customWidth : btnW;
        D2D1_RECT_F bRect = D2D1::RectF(currentX, currentY, currentX + w, currentY + btnH);

        bool isHovered = (mousePt.x >= bRect.left && mousePt.x <= bRect.right &&
                          mousePt.y >= bRect.top && mousePt.y <= bRect.bottom);

        HudButtonInfo info;
        info.id = s.id;
        info.label = s.label;
        info.rect = bRect;
        info.isHovered = isHovered;
        m_hudButtons.push_back(info);

        if (isHovered) {
            if (s.id == HudButtonId::Delete) {
                m_brushChrome->SetColor(D2D1::ColorF(0xef4444, 0.7f * hudAlpha));
            } else {
                m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 0.9f * hudAlpha));
            }
            m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(bRect, 6.0f, 6.0f), m_brushChrome.Get());
            m_brushText->SetColor(D2D1::ColorF(0xffffff, hudAlpha));
        } else {
            m_brushText->SetColor(D2D1::ColorF(0xd4d4d8, 0.9f * hudAlpha));
        }

        m_d2dContext->DrawText(
            s.label,
            static_cast<UINT32>(wcslen(s.label)),
            m_formatHud.Get(),
            bRect,
            m_brushText.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );

        currentX += w + gap;
    }
}

void D2DRenderer::RenderExifOverlay(TabItem* tab) {
    if (!tab || !tab->document) return;

    const ImageMetadata& meta = tab->document->metadata;

    float cardW = 340.0f * m_dpiScale;
    float cardH = 260.0f * m_dpiScale;
    float cardX = 24.0f * m_dpiScale;
    float cardY = (tab->navigator && tab->navigator->HasFiles() ? GetTabStripHeight() : 0.0f) + 24.0f * m_dpiScale;

    D2D1_RECT_F cardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

    // Frosted card background
    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.94f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(cardRect, 12.0f, 12.0f), m_brushChrome.Get());

    // Card border
    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(cardRect, 12.0f, 12.0f), m_brushBorder.Get(), 1.0f);

    float textX = cardX + 18.0f * m_dpiScale;
    float curY = cardY + 16.0f * m_dpiScale;
    float lineH = 24.0f * m_dpiScale;

    // Header: Image Details
    m_brushAccent->SetColor(D2D1::ColorF(0x06b6d4));
    const wchar_t header[] = L"Image Properties & EXIF";
    D2D1_RECT_F headerRect = D2D1::RectF(textX, curY, cardX + cardW - 18.0f * m_dpiScale, curY + lineH);
    m_d2dContext->DrawText(header, static_cast<UINT32>(wcslen(header)), m_formatHud.Get(), headerRect, m_brushAccent.Get());
    curY += lineH + 8.0f * m_dpiScale;

    auto DrawField = [&](const wchar_t* label, const std::wstring& val) {
        if (val.empty()) return;
        m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
        D2D1_RECT_F lRect = D2D1::RectF(textX, curY, textX + 90.0f * m_dpiScale, curY + lineH);
        m_d2dContext->DrawText(label, static_cast<UINT32>(wcslen(label)), m_formatInfo.Get(), lRect, m_brushTextMuted.Get());

        m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
        D2D1_RECT_F vRect = D2D1::RectF(textX + 95.0f * m_dpiScale, curY, cardX + cardW - 16.0f * m_dpiScale, curY + lineH);
        m_d2dContext->DrawText(val.c_str(), static_cast<UINT32>(val.length()), m_formatInfo.Get(), vRect, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        curY += lineH;
    };

    DrawField(L"File Name:", meta.fileName);
    DrawField(L"Dimensions:", std::to_wstring(meta.width) + L" × " + std::to_wstring(meta.height) + L" px");

    // File size
    std::wstring sizeStr;
    if (meta.fileSizeBytes > 1024 * 1024) {
        double mb = static_cast<double>(meta.fileSizeBytes) / (1024.0 * 1024.0);
        wchar_t buf[32];
        swprintf_s(buf, L"%.2f MB", mb);
        sizeStr = buf;
    } else {
        sizeStr = std::to_wstring(meta.fileSizeBytes / 1024) + L" KB";
    }
    DrawField(L"File Size:", sizeStr);
    DrawField(L"Format:", meta.formatName + (meta.isAnimated ? (L" (Animated, " + std::to_wstring(meta.frameCount) + L" frames)") : L""));

    if (!meta.cameraModel.empty()) DrawField(L"Camera:", meta.cameraModel);
    if (!meta.dateTaken.empty()) DrawField(L"Date Taken:", meta.dateTaken);
    if (!meta.exposure.empty() || !meta.iso.empty()) {
        std::wstring exp = meta.exposure + (meta.exposure.empty() || meta.iso.empty() ? L"" : L", ") + meta.iso;
        DrawField(L"Exposure:", exp);
    }

    if (tab->navigator && tab->navigator->HasFiles()) {
        std::wstring idxStr = std::to_wstring(tab->navigator->GetCurrentIndex() + 1) + L" of " + std::to_wstring(tab->navigator->GetTotalFiles());
        DrawField(L"Folder Index:", idxStr);
    }
}

void D2DRenderer::RenderEmptyState() {
    float vpW = static_cast<float>(m_width);
    float vpH = static_cast<float>(m_height);

    float centerY = vpH / 2.0f - 40.0f * m_dpiScale;

    // Title
    m_brushText->SetColor(D2D1::ColorF(0xf8fafc));
    const wchar_t title[] = L"LightPhotoViewer";
    D2D1_RECT_F tRect = D2D1::RectF(0, centerY, vpW, centerY + 36.0f * m_dpiScale);
    m_d2dContext->DrawText(title, static_cast<UINT32>(wcslen(title)), m_formatTitle.Get(), tRect, m_brushText.Get());

    // Subtitle
    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    const wchar_t sub[] = L"Drag & drop an image or folder here, or press Ctrl+O to open";
    D2D1_RECT_F sRect = D2D1::RectF(0, centerY + 46.0f * m_dpiScale, vpW, centerY + 74.0f * m_dpiScale);
    m_d2dContext->DrawText(sub, static_cast<UINT32>(wcslen(sub)), m_formatSubtitle.Get(), sRect, m_brushTextMuted.Get());

    // Shortcut Hints Card
    float cardW = 420.0f * m_dpiScale;
    float cardH = 90.0f * m_dpiScale;
    float cardX = (vpW - cardW) / 2.0f;
    float cardY = centerY + 94.0f * m_dpiScale;

    D2D1_RECT_F cRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);
    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.7f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(cRect, 8.0f, 8.0f), m_brushChrome.Get());
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(cRect, 8.0f, 8.0f), m_brushBorder.Get(), 1.0f);

    const wchar_t hints[] =
        L"← / → : Browse Folder      Mouse Wheel : Cursor-Centered Zoom\n"
        L"0 : Fit to Window          1 : 100% Actual Size          F : Fullscreen\n"
        L"R / L : Rotate CW / CCW    I : EXIF Info                 F1 : Shortcuts Help";

    D2D1_RECT_F hRect = D2D1::RectF(cardX + 16.0f * m_dpiScale, cardY + 12.0f * m_dpiScale, cardX + cardW - 16.0f * m_dpiScale, cardY + cardH - 12.0f * m_dpiScale);
    m_d2dContext->DrawText(hints, static_cast<UINT32>(wcslen(hints)), m_formatInfo.Get(), hRect, m_brushTextMuted.Get());
}

void D2DRenderer::RenderHelpOverlay(POINT mousePt) {
    float clientW = static_cast<float>(m_width);
    float clientH = static_cast<float>(m_height);

    // 1. Semi-transparent backdrop
    m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.72f));
    m_d2dContext->FillRectangle(D2D1::RectF(0, 0, clientW, clientH), m_brushBg.Get());

    // 2. Centered Modal Card
    float cardW = std::min(780.0f * m_dpiScale, clientW - 32.0f);
    float cardH = std::min(520.0f * m_dpiScale, clientH - 32.0f);
    float cardX = (clientW - cardW) / 2.0f;
    float cardY = (clientH - cardH) / 2.0f;

    m_helpCardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

    // Frosted Card Background
    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.96f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_helpCardRect, 12.0f, 12.0f), m_brushChrome.Get());

    // Card Border
    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.9f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(m_helpCardRect, 12.0f, 12.0f), m_brushBorder.Get(), 1.5f);

    // Header Title
    float headerPad = 20.0f * m_dpiScale;
    float titleY = cardY + 16.0f * m_dpiScale;

    m_brushText->SetColor(D2D1::ColorF(0xf8fafc));
    const wchar_t title[] = L"Keyboard & Mouse Shortcuts";
    D2D1_RECT_F titleRect = D2D1::RectF(cardX + headerPad, titleY, cardX + cardW - 60.0f * m_dpiScale, titleY + 24.0f * m_dpiScale);
    m_d2dContext->DrawText(title, static_cast<UINT32>(wcslen(title)), m_formatHud.Get(), titleRect, m_brushText.Get());

    // Subtitle
    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    const wchar_t sub[] = L"LightPhotoViewer Quick Reference Guide";
    D2D1_RECT_F subRect = D2D1::RectF(cardX + headerPad, titleY + 22.0f * m_dpiScale, cardX + cardW - 60.0f * m_dpiScale, titleY + 40.0f * m_dpiScale);
    m_d2dContext->DrawText(sub, static_cast<UINT32>(wcslen(sub)), m_formatKeyDesc.Get(), subRect, m_brushTextMuted.Get());

    // Close button ('✕')
    float closeSize = 24.0f * m_dpiScale;
    float closeX1 = cardX + cardW - headerPad - closeSize;
    float closeY1 = cardY + 16.0f * m_dpiScale;
    m_helpCloseRect = D2D1::RectF(closeX1, closeY1, closeX1 + closeSize, closeY1 + closeSize);

    bool isCloseHover = (mousePt.x >= m_helpCloseRect.left && mousePt.x <= m_helpCloseRect.right &&
                         mousePt.y >= m_helpCloseRect.top && mousePt.y <= m_helpCloseRect.bottom);

    if (isCloseHover) {
        m_brushChrome->SetColor(D2D1::ColorF(0xef4444, 0.85f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_helpCloseRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushText->SetColor(D2D1::ColorF(0xffffff));
    } else {
        m_brushText->SetColor(D2D1::ColorF(0x94a3b8));
    }
    const wchar_t closeIcon[] = L"✕";
    m_d2dContext->DrawText(closeIcon, 1, m_formatHud.Get(), m_helpCloseRect, m_brushText.Get());

    // Divider Line below header
    float divY = cardY + 60.0f * m_dpiScale;
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(cardX + headerPad, divY), D2D1::Point2F(cardX + cardW - headerPad, divY), m_brushBorder.Get(), 1.0f);

    // 3 Columns Layout
    struct ShortcutItem {
        const wchar_t* key;
        const wchar_t* desc;
    };
    struct ShortcutSection {
        const wchar_t* title;
        std::vector<ShortcutItem> items;
    };

    std::vector<ShortcutSection> col1 = {
        {
            L"NAVIGATION",
            {
                { L"→ / Space", L"Next image" },
                { L"← / Back",  L"Previous image" },
                { L"Home / End",L"First / Last file" },
                { L"Delete",    L"Send to Recycle Bin" },
            }
        },
        {
            L"FILE & TABS",
            {
                { L"Ctrl + O",     L"Open image..." },
                { L"Ctrl + W",     L"Close active tab" },
                { L"Ctrl + Tab",   L"Next tab" },
                { L"Ctrl+Shift+Tab", L"Previous tab" },
            }
        }
    };

    std::vector<ShortcutSection> col2 = {
        {
            L"ZOOM & VIEW",
            {
                { L"0",           L"Fit to window" },
                { L"1",           L"100% Actual size" },
                { L"+ / Wheel",   L"Zoom in (focal)" },
                { L"- / Wheel",   L"Zoom out (focal)" },
                { L"Left Drag",   L"Pan canvas" },
                { L"Double Click",L"Toggle Fit / 100%" },
                { L"Middle Click",L"Reset to Fit" },
                { L"F / Enter",   L"Toggle fullscreen" },
            }
        }
    };

    std::vector<ShortcutSection> col3 = {
        {
            L"TOOLS & ACTIONS",
            {
                { L"R",           L"Rotate 90° CW" },
                { L"L",           L"Rotate 90° CCW" },
                { L"H / V",       L"Flip Horiz / Vert" },
                { L"I",           L"Toggle EXIF info" },
                { L"Ctrl + C",    L"Copy bitmap" },
                { L"Ctrl+Shift+C",L"Copy file path" },
                { L"F1",          L"Toggle this help" },
                { L"Esc",         L"Close / Exit" },
            }
        }
    };

    std::vector<std::vector<ShortcutSection>> columns = { col1, col2, col3 };
    float colGap = 16.0f * m_dpiScale;
    float contentW = cardW - headerPad * 2.0f;
    float colW = (contentW - colGap * 2.0f) / 3.0f;
    float startY = divY + 14.0f * m_dpiScale;

    auto DrawSection = [&](const ShortcutSection& sec, float cx, float& cy) {
        m_brushAccent->SetColor(D2D1::ColorF(0x06b6d4)); // Cyan
        D2D1_RECT_F stRect = D2D1::RectF(cx, cy, cx + colW, cy + 18.0f * m_dpiScale);
        m_d2dContext->DrawText(sec.title, static_cast<UINT32>(wcslen(sec.title)), m_formatHelpCat.Get(), stRect, m_brushAccent.Get());
        cy += 24.0f * m_dpiScale;

        float rowH = 25.0f * m_dpiScale;
        for (const auto& item : sec.items) {
            float keycapW = std::min(92.0f * m_dpiScale, colW * 0.44f);
            D2D1_RECT_F kRect = D2D1::RectF(cx, cy, cx + keycapW, cy + rowH - 4.0f * m_dpiScale);

            m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 0.9f));
            m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(kRect, 4.0f, 4.0f), m_brushChrome.Get());
            m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.7f));
            m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(kRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);

            m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
            m_d2dContext->DrawText(item.key, static_cast<UINT32>(wcslen(item.key)), m_formatKeycap.Get(), kRect, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            D2D1_RECT_F dRect = D2D1::RectF(cx + keycapW + 8.0f * m_dpiScale, cy, cx + colW, cy + rowH - 4.0f * m_dpiScale);
            m_brushTextMuted->SetColor(D2D1::ColorF(0xcbd5e1));
            m_d2dContext->DrawText(item.desc, static_cast<UINT32>(wcslen(item.desc)), m_formatKeyDesc.Get(), dRect, m_brushTextMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            cy += rowH;
        }
        cy += 8.0f * m_dpiScale;
    };

    for (size_t c = 0; c < 3; ++c) {
        float cx = cardX + headerPad + c * (colW + colGap);
        float cy = startY;
        for (const auto& sec : columns[c]) {
            DrawSection(sec, cx, cy);
        }
    }

    // Footer
    float footerY = cardY + cardH - 32.0f * m_dpiScale;
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(cardX + headerPad, footerY), D2D1::Point2F(cardX + cardW - headerPad, footerY), m_brushBorder.Get(), 1.0f);

    const wchar_t footer[] = L"Press F1 or Esc or click outside to dismiss \x2022 Zero Runtime Dependencies";
    D2D1_RECT_F footRect = D2D1::RectF(cardX + headerPad, footerY + 6.0f * m_dpiScale, cardX + cardW - headerPad, footerY + 26.0f * m_dpiScale);
    m_brushTextMuted->SetColor(D2D1::ColorF(0x64748b));
    m_d2dContext->DrawText(footer, static_cast<UINT32>(wcslen(footer)), m_formatKeyDesc.Get(), footRect, m_brushTextMuted.Get());
}

