#include "pch.hpp"
#include "d2d_renderer.hpp"

D2DRenderer::D2DRenderer() {
}

D2DRenderer::~D2DRenderer() {
    DiscardDeviceResources();
}

void D2DRenderer::UpdateDpi(float dpi) {
    m_dpiScale = (dpi > 0.0f) ? (dpi / 96.0f) : 1.0f;
    CreateDeviceIndependentResources();
    if (m_d2dContext && m_d2dTargetBitmap) {
        RecreateTargetBitmap();
    }
}

void D2DRenderer::CycleBackgroundMode() {
    switch (m_bgMode) {
        case CanvasBackground::Dark:         m_bgMode = CanvasBackground::Checkerboard; break;
        case CanvasBackground::Checkerboard: m_bgMode = CanvasBackground::Light; break;
        case CanvasBackground::Light:        m_bgMode = CanvasBackground::Dark; break;
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
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x06b6d4, 1.0f), &m_brushAccent);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0xf8fafc, 1.0f), &m_brushText);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x94a3b8, 1.0f), &m_brushTextMuted);
    m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0x334155, 0.8f), &m_brushBorder);

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

    // Direct2D target surface locked to 96.0f DPI so 1 unit = 1 physical screen pixel
    // This provides exact 1:1 pixel rendering at 100% zoom and pixel-perfect hit testing on all DPI scales
    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f
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
    float availW = std::max(10.0f, clientW - 40.0f * m_dpiScale);
    float tabW = std::min(maxTabW, availW / static_cast<float>(count));

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

bool D2DRenderer::HitTestTabAdd(POINT pt) {
    return (pt.x >= m_tabAddRect.left && pt.x <= m_tabAddRect.right &&
            pt.y >= m_tabAddRect.top && pt.y <= m_tabAddRect.bottom);
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

int D2DRenderer::HitTestRecentFile(POINT pt, std::wstring& outPath) {
    for (const auto& hit : m_recentHits) {
        if (pt.x >= hit.rect.left && pt.x <= hit.rect.right &&
            pt.y >= hit.rect.top && pt.y <= hit.rect.bottom) {
            outPath = hit.path;
            return hit.index;
        }
    }
    return -1;
}

void D2DRenderer::DrawCheckerboard(D2D1_RECT_F rect) {
    float size = 16.0f * m_dpiScale;
    m_brushChrome->SetColor(D2D1::ColorF(0x222226, 1.0f));
    m_d2dContext->FillRectangle(rect, m_brushChrome.Get());

    m_brushBorder->SetColor(D2D1::ColorF(0x18181c, 1.0f));
    for (float y = rect.top; y < rect.bottom; y += size) {
        int row = static_cast<int>((y - rect.top) / size);
        for (float x = rect.left; x < rect.right; x += size) {
            int col = static_cast<int>((x - rect.left) / size);
            if ((row + col) % 2 == 1) {
                D2D1_RECT_F sq = D2D1::RectF(x, y, std::min(x + size, rect.right), std::min(y + size, rect.bottom));
                m_d2dContext->FillRectangle(sq, m_brushBorder.Get());
            }
        }
    }
}

void D2DRenderer::Render(TabManager& tabManager, float hudAlpha, bool showExif, bool showHelp, bool isSlideshow, const PrintSettings& printSettings, const ExportSettings& exportSettings, POINT mousePt) {
    if (!CreateDeviceResources()) return;

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0x121214, 1.0f));

    TabItem* activeTab = tabManager.GetActiveTab();
    float tabStripHeight = tabManager.ShouldShowTabs() ? GetTabStripHeight() : 0.0f;
    float viewportHeight = static_cast<float>(m_height) - tabStripHeight;

    if (activeTab && activeTab->document && activeTab->document->isValid) {
        m_recentHits.clear();
        RenderImageCanvas(activeTab, tabStripHeight, viewportHeight);
    } else {
        RenderEmptyState(tabManager, mousePt);
    }

    if (tabManager.ShouldShowTabs()) {
        RenderTabs(tabManager, mousePt);
    }

    if (activeTab && activeTab->document && activeTab->document->isValid) {
        RenderHud(activeTab, hudAlpha, mousePt);
        if (showExif) {
            RenderExifOverlay(activeTab);
        }
    }

    if (isSlideshow) {
        RenderSlideshowBadge();
    }

    if (showHelp) {
        RenderHelpOverlay(mousePt);
    } else {
        m_helpCardRect = { 0, 0, 0, 0 };
        m_helpCloseRect = { 0, 0, 0, 0 };
    }

    if (printSettings.isActive) {
        RenderPrintPreview(activeTab, printSettings, mousePt);
    } else {
        m_printHits.clear();
        m_printCardRect = { 0, 0, 0, 0 };
        m_printCloseRect = { 0, 0, 0, 0 };
    }

    if (exportSettings.isActive) {
        RenderExportModal(activeTab, exportSettings, mousePt);
    } else {
        m_exportHits.clear();
        m_exportCardRect = { 0, 0, 0, 0 };
        m_exportCloseRect = { 0, 0, 0, 0 };
        m_exportSliderRect = { 0, 0, 0, 0 };
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
    float availW = std::max(10.0f, clientW - 40.0f * m_dpiScale);
    float tabW = std::min(maxTabW, availW / static_cast<float>(count));

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

            // Electric Cyan active indicator bar
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

    // '+' New Tab Button
    float addBtnSize = 24.0f * m_dpiScale;
    float addX = count * tabW + 6.0f * m_dpiScale;
    float addY = (tabH - addBtnSize) / 2.0f;
    m_tabAddRect = D2D1::RectF(addX, addY, addX + addBtnSize, addY + addBtnSize);

    bool addHover = (mousePt.x >= m_tabAddRect.left && mousePt.x <= m_tabAddRect.right &&
                     mousePt.y >= m_tabAddRect.top && mousePt.y <= m_tabAddRect.bottom);

    if (addHover) {
        m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 1.0f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_tabAddRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushText->SetColor(D2D1::ColorF(0xffffff));
    } else {
        m_brushText->SetColor(D2D1::ColorF(0x94a3b8));
    }
    const wchar_t plusIcon[] = L"+";
    m_d2dContext->DrawText(plusIcon, 1, m_formatHud.Get(), m_tabAddRect, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

void D2DRenderer::RenderImageCanvas(TabItem* tab, float viewportTop, float viewportHeight) {
    if (!tab || !tab->document) return;

    float imgW = static_cast<float>(tab->document->GetWidth());
    float imgH = static_cast<float>(tab->document->GetHeight());
    if (imgW <= 0 || imgH <= 0) return;

    // Viewport dimensions
    float vpW = static_cast<float>(m_width);
    float vpH = viewportHeight;
    float centerX = vpW / 2.0f;
    float centerY = viewportTop + vpH / 2.0f;

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

    D2D1_RECT_F clipRect = D2D1::RectF(0, viewportTop, vpW, viewportTop + vpH);
    m_d2dContext->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    D2D1::Matrix3x2F transform =
        D2D1::Matrix3x2F::Scale(tab->flipH ? -1.0f : 1.0f, tab->flipV ? -1.0f : 1.0f) *
        D2D1::Matrix3x2F::Rotation(static_cast<float>(tab->rotationDegrees)) *
        D2D1::Matrix3x2F::Scale(tab->zoom, tab->zoom) *
        D2D1::Matrix3x2F::Translation(tab->panX, tab->panY);

    m_d2dContext->SetTransform(transform);
    D2D1_RECT_F destRect = D2D1::RectF(-imgW / 2.0f, -imgH / 2.0f, imgW / 2.0f, imgH / 2.0f);

    // Render Canvas Transparency Backdrop (Dark / Checkerboard / Light)
    if (m_bgMode == CanvasBackground::Light) {
        m_brushChrome->SetColor(D2D1::ColorF(0xF8FAFC, 1.0f));
        m_d2dContext->FillRectangle(destRect, m_brushChrome.Get());
    } else if (m_bgMode == CanvasBackground::Checkerboard) {
        DrawCheckerboard(destRect);
    }

    if (tab->document->metadata.isSvg) {
        // Native SVG Vector Rendering via ID2D1DeviceContext5
        ComPtr<ID2D1DeviceContext5> ctx5;
        if (SUCCEEDED(m_d2dContext.As(&ctx5))) {
            if (!tab->svgDocument) {
                ComPtr<IStream> stream;
                if (SUCCEEDED(SHCreateStreamOnFileEx(
                    tab->document->metadata.filePath.c_str(),
                    STGM_READ | STGM_SHARE_DENY_NONE,
                    FILE_ATTRIBUTE_NORMAL,
                    FALSE,
                    nullptr,
                    &stream))) {
                    ctx5->CreateSvgDocument(stream.Get(), D2D1::SizeF(imgW, imgH), &tab->svgDocument);
                }
            }
            if (tab->svgDocument) {
                D2D1::Matrix3x2F svgTransform = D2D1::Matrix3x2F::Translation(-imgW / 2.0f, -imgH / 2.0f) * transform;
                m_d2dContext->SetTransform(svgTransform);
                ctx5->DrawSvgDocument(tab->svgDocument.Get());
                m_d2dContext->SetTransform(transform);
            }
        }
    } else {
        // Bitmap Rendering (Direct2D Texture)
        size_t frameCount = tab->document->GetFrameCount();
        if (frameCount > 0) {
            if (tab->gpuBitmaps.size() < frameCount) {
                tab->gpuBitmaps.resize(frameCount);
            }

            size_t activeFrameIdx = tab->currentFrame % frameCount;
            if (!tab->gpuBitmaps[activeFrameIdx]) {
                const FrameData* fd = tab->document->GetFrame(activeFrameIdx);
                if (fd && fd->bitmap) {
                    m_d2dContext->CreateBitmapFromWicBitmap(fd->bitmap.Get(), nullptr, &tab->gpuBitmaps[activeFrameIdx]);
                }
            }

            ComPtr<ID2D1Bitmap1> bitmap = tab->gpuBitmaps[activeFrameIdx];
            if (bitmap) {
                D2D1_INTERPOLATION_MODE interpMode;
                if (tab->zoom < 0.99f) {
                    interpMode = D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
                } else if (tab->zoom >= 4.0f) {
                    interpMode = D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
                } else {
                    interpMode = D2D1_INTERPOLATION_MODE_LINEAR;
                }

                m_d2dContext->DrawBitmap(bitmap.Get(), destRect, 1.0f, interpMode);

                if (tab->zoom >= 8.0f) {
                    m_brushBorder->SetColor(D2D1::ColorF(0xffffff, 0.15f));
                    for (float x = -imgW / 2.0f; x <= imgW / 2.0f; x += 1.0f) {
                        m_d2dContext->DrawLine(D2D1::Point2F(x, -imgH / 2.0f), D2D1::Point2F(x, imgH / 2.0f), m_brushBorder.Get(), 0.5f / tab->zoom);
                    }
                    for (float y = -imgH / 2.0f; y <= imgH / 2.0f; y += 1.0f) {
                        m_d2dContext->DrawLine(D2D1::Point2F(-imgW / 2.0f, y), D2D1::Point2F(imgW / 2.0f, y), m_brushBorder.Get(), 0.5f / tab->zoom);
                    }
                }
            }
        }
    }

    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
    m_d2dContext->PopAxisAlignedClip();
}

void D2DRenderer::RenderHud(TabItem* activeTab, float hudAlpha, POINT mousePt) {
    if (hudAlpha < 0.02f) {
        m_hudButtons.clear();
        return;
    }

    float vpW = static_cast<float>(m_width);
    float vpH = static_cast<float>(m_height);

    float btnW = 38.0f * m_dpiScale;
    float btnH = 36.0f * m_dpiScale;
    float gap = 4.0f * m_dpiScale;

    std::wstring zoomText = L"1:1";
    std::wstring fitText = L"Fit";
    if (activeTab) {
        int zPercent = static_cast<int>(activeTab->zoom * 100.0f + 0.5f);
        zoomText = std::to_wstring(zPercent) + L"%";
        fitText = activeTab->isFitToWindow ? L"1:1" : L"Fit";
    }

    struct BtnSpec {
        HudButtonId id;
        std::wstring label;
        float customWidth = 0.0f;
    };

    BtnSpec specs[] = {
        { HudButtonId::Prev,      L"◀" },
        { HudButtonId::Next,      L"▶" },
        { HudButtonId::ZoomOut,   L"−" },
        { HudButtonId::Zoom100,   zoomText, 48.0f * m_dpiScale },
        { HudButtonId::ZoomIn,    L"+" },
        { HudButtonId::ZoomFit,   fitText, 44.0f * m_dpiScale },
        { HudButtonId::RotateCCW, L"↶" },
        { HudButtonId::RotateCW,  L"↷" },
        { HudButtonId::Info,      L"ℹ" },
        { HudButtonId::Export,    L"💾" },
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

    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.88f * hudAlpha));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_hudRect, hudH / 2.0f, hudH / 2.0f), m_brushChrome.Get());

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
            if (s.id == HudButtonId::ZoomFit && activeTab && activeTab->isFitToWindow) {
                m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 0.5f * hudAlpha));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(bRect, 6.0f, 6.0f), m_brushChrome.Get());
                m_brushText->SetColor(D2D1::ColorF(0x38bdf8, 0.95f * hudAlpha));
            } else {
                m_brushText->SetColor(D2D1::ColorF(0xd4d4d8, 0.9f * hudAlpha));
            }
        }

        m_d2dContext->DrawText(
            s.label.c_str(),
            static_cast<UINT32>(s.label.length()),
            m_formatHud.Get(),
            bRect,
            m_brushText.Get(),
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );

        currentX += w + gap;
    }
}

void D2DRenderer::RenderSlideshowBadge() {
    float vpW = static_cast<float>(m_width);
    float badgeW = 220.0f * m_dpiScale;
    float badgeH = 32.0f * m_dpiScale;
    float badgeX = vpW - badgeW - 24.0f * m_dpiScale;
    float badgeY = 24.0f * m_dpiScale;

    D2D1_RECT_F bRect = D2D1::RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH);
    m_brushChrome->SetColor(D2D1::ColorF(0x06b6d4, 0.85f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(bRect, 6.0f, 6.0f), m_brushChrome.Get());

    m_brushText->SetColor(D2D1::ColorF(0xffffff));
    const wchar_t text[] = L"▶ Slideshow (3s) [Space/Esc]";
    m_d2dContext->DrawText(text, static_cast<UINT32>(wcslen(text)), m_formatKeycap.Get(), bRect, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

void D2DRenderer::RenderExifOverlay(TabItem* tab) {
    if (!tab || !tab->document) return;

    const ImageMetadata& meta = tab->document->metadata;

    float cardW = 340.0f * m_dpiScale;
    float cardH = 260.0f * m_dpiScale;
    float cardX = 24.0f * m_dpiScale;
    float cardY = (tab->navigator && tab->navigator->HasFiles() ? GetTabStripHeight() : 0.0f) + 24.0f * m_dpiScale;

    D2D1_RECT_F cardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.94f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(cardRect, 12.0f, 12.0f), m_brushChrome.Get());

    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(cardRect, 12.0f, 12.0f), m_brushBorder.Get(), 1.0f);

    float textX = cardX + 18.0f * m_dpiScale;
    float curY = cardY + 16.0f * m_dpiScale;
    float lineH = 24.0f * m_dpiScale;

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

void D2DRenderer::RenderHelpOverlay(POINT mousePt) {
    float clientW = static_cast<float>(m_width);
    float clientH = static_cast<float>(m_height);

    m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.72f));
    m_d2dContext->FillRectangle(D2D1::RectF(0, 0, clientW, clientH), m_brushBg.Get());

    float cardW = std::min(780.0f * m_dpiScale, clientW - 32.0f);
    float cardH = std::min(530.0f * m_dpiScale, clientH - 32.0f);
    float cardX = (clientW - cardW) / 2.0f;
    float cardY = (clientH - cardH) / 2.0f;

    m_helpCardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.96f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_helpCardRect, 12.0f, 12.0f), m_brushChrome.Get());

    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.9f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(m_helpCardRect, 12.0f, 12.0f), m_brushBorder.Get(), 1.5f);

    float headerPad = 20.0f * m_dpiScale;
    float titleY = cardY + 16.0f * m_dpiScale;

    m_brushText->SetColor(D2D1::ColorF(0xf8fafc));
    const wchar_t title[] = L"Keyboard & Mouse Shortcuts";
    D2D1_RECT_F titleRect = D2D1::RectF(cardX + headerPad, titleY, cardX + cardW - 60.0f * m_dpiScale, titleY + 24.0f * m_dpiScale);
    m_d2dContext->DrawText(title, static_cast<UINT32>(wcslen(title)), m_formatHud.Get(), titleRect, m_brushText.Get());

    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    const wchar_t sub[] = L"LightPhotoViewer Quick Reference Guide";
    D2D1_RECT_F subRect = D2D1::RectF(cardX + headerPad, titleY + 22.0f * m_dpiScale, cardX + cardW - 60.0f * m_dpiScale, titleY + 40.0f * m_dpiScale);
    m_d2dContext->DrawText(sub, static_cast<UINT32>(wcslen(sub)), m_formatKeyDesc.Get(), subRect, m_brushTextMuted.Get());

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

    float divY = cardY + 60.0f * m_dpiScale;
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(cardX + headerPad, divY), D2D1::Point2F(cardX + cardW - headerPad, divY), m_brushBorder.Get(), 1.0f);

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
                { L"→ / Space",   L"Next image" },
                { L"← / Back",    L"Previous image" },
                { L"Home / End",  L"First / Last file" },
                { L"Delete",      L"Send to Recycle Bin" },
                { L"F5 / S",      L"Toggle Slideshow" },
            }
        },
        {
            L"FILE & TABS",
            {
                { L"Ctrl + O",       L"Open image..." },
                { L"Ctrl + S",       L"Export / Convert..." },
                { L"Ctrl + P",       L"Print image..." },
                { L"Ctrl + W",       L"Close active tab" },
                { L"Ctrl+Shift+T",   L"Reopen closed tab" },
                { L"Ctrl + Tab",     L"Next tab" },
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
                { L"B",           L"Toggle Backdrop" },
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
        m_brushAccent->SetColor(D2D1::ColorF(0x06b6d4));
        D2D1_RECT_F stRect = D2D1::RectF(cx, cy, cx + colW, cy + 18.0f * m_dpiScale);
        m_d2dContext->DrawText(sec.title, static_cast<UINT32>(wcslen(sec.title)), m_formatHelpCat.Get(), stRect, m_brushAccent.Get());
        cy += 24.0f * m_dpiScale;

        float rowH = 25.0f * m_dpiScale;
        for (const auto& item : sec.items) {
            float keycapW = std::min(96.0f * m_dpiScale, colW * 0.44f);
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

    float footerY = cardY + cardH - 32.0f * m_dpiScale;
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(cardX + headerPad, footerY), D2D1::Point2F(cardX + cardW - headerPad, footerY), m_brushBorder.Get(), 1.0f);

    const wchar_t footer[] = L"Press F1 or Esc or click outside to dismiss \x2022 Zero Runtime Dependencies";
    D2D1_RECT_F footRect = D2D1::RectF(cardX + headerPad, footerY + 6.0f * m_dpiScale, cardX + cardW - headerPad, footerY + 26.0f * m_dpiScale);
    m_brushTextMuted->SetColor(D2D1::ColorF(0x64748b));
    m_d2dContext->DrawText(footer, static_cast<UINT32>(wcslen(footer)), m_formatKeyDesc.Get(), footRect, m_brushTextMuted.Get());
}

void D2DRenderer::RenderEmptyState(const TabManager& tabManager, POINT mousePt) {
    float vpW = static_cast<float>(m_width);
    float vpH = static_cast<float>(m_height);

    m_recentHits.clear();
    const auto& recentFiles = tabManager.GetRecentFiles();

    float topY = vpH * 0.12f;

    // Title
    m_brushText->SetColor(D2D1::ColorF(0xf8fafc));
    const wchar_t title[] = L"LightPhotoViewer";
    D2D1_RECT_F tRect = D2D1::RectF(0, topY, vpW, topY + 36.0f * m_dpiScale);
    m_d2dContext->DrawText(title, static_cast<UINT32>(wcslen(title)), m_formatTitle.Get(), tRect, m_brushText.Get());

    // Subtitle
    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    const wchar_t sub[] = L"Drag & drop images here, or press Ctrl+O to open a photo";
    D2D1_RECT_F sRect = D2D1::RectF(0, topY + 44.0f * m_dpiScale, vpW, topY + 70.0f * m_dpiScale);
    m_d2dContext->DrawText(sub, static_cast<UINT32>(wcslen(sub)), m_formatSubtitle.Get(), sRect, m_brushTextMuted.Get());

    float curY = topY + 84.0f * m_dpiScale;

    // Recent Files Section
    if (!recentFiles.empty()) {
        float listW = std::min(580.0f * m_dpiScale, vpW - 40.0f);
        float listX = (vpW - listW) / 2.0f;

        m_brushAccent->SetColor(D2D1::ColorF(0x06b6d4));
        const wchar_t rTitle[] = L"RECENT DOCUMENTS";
        D2D1_RECT_F rRect = D2D1::RectF(listX, curY, listX + listW, curY + 20.0f * m_dpiScale);
        m_d2dContext->DrawText(rTitle, static_cast<UINT32>(wcslen(rTitle)), m_formatHelpCat.Get(), rRect, m_brushAccent.Get());
        curY += 26.0f * m_dpiScale;

        float itemH = 34.0f * m_dpiScale;
        for (size_t i = 0; i < recentFiles.size() && i < 6; ++i) {
            D2D1_RECT_F rowRect = D2D1::RectF(listX, curY, listX + listW, curY + itemH);

            bool isHover = (mousePt.x >= rowRect.left && mousePt.x <= rowRect.right &&
                            mousePt.y >= rowRect.top && mousePt.y <= rowRect.bottom);

            RecentFileHitBox hit;
            hit.index = static_cast<int>(i);
            hit.path = recentFiles[i];
            hit.rect = rowRect;
            hit.isHovered = isHover;
            m_recentHits.push_back(hit);

            if (isHover) {
                m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 0.9f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(rowRect, 6.0f, 6.0f), m_brushChrome.Get());
                m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.9f));
                m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(rowRect, 6.0f, 6.0f), m_brushBorder.Get(), 1.0f);
            }

            // Number Badge
            std::wstring numBadge = L"[" + std::to_wstring(i + 1) + L"]";
            D2D1_RECT_F bRect = D2D1::RectF(listX + 8.0f * m_dpiScale, curY, listX + 38.0f * m_dpiScale, curY + itemH);
            m_brushAccent->SetColor(D2D1::ColorF(0x06b6d4));
            m_d2dContext->DrawText(numBadge.c_str(), static_cast<UINT32>(numBadge.length()), m_formatKeycap.Get(), bRect, m_brushAccent.Get());

            // Extract file name and directory
            std::filesystem::path fp(recentFiles[i]);
            std::wstring fn = fp.filename().wstring();
            std::wstring dir = fp.parent_path().wstring();

            float textLeft = listX + 44.0f * m_dpiScale;
            D2D1_RECT_F fnRect = D2D1::RectF(textLeft, curY, listX + listW * 0.55f, curY + itemH);
            m_brushText->SetColor(isHover ? D2D1::ColorF(0xffffff) : D2D1::ColorF(0xe2e8f0));
            m_d2dContext->DrawText(fn.c_str(), static_cast<UINT32>(fn.length()), m_formatTab.Get(), fnRect, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            D2D1_RECT_F dirRect = D2D1::RectF(listX + listW * 0.56f, curY, listX + listW - 12.0f * m_dpiScale, curY + itemH);
            m_brushTextMuted->SetColor(D2D1::ColorF(0x64748b));
            m_d2dContext->DrawText(dir.c_str(), static_cast<UINT32>(dir.length()), m_formatKeyDesc.Get(), dirRect, m_brushTextMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

            curY += itemH + 4.0f * m_dpiScale;
        }
        curY += 10.0f * m_dpiScale;
    }

    // Shortcut Hints Card
    float cardW = 540.0f * m_dpiScale;
    float cardH = 80.0f * m_dpiScale;
    float cardX = (vpW - cardW) / 2.0f;

    D2D1_RECT_F cRect = D2D1::RectF(cardX, curY, cardX + cardW, curY + cardH);
    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.7f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(cRect, 8.0f, 8.0f), m_brushChrome.Get());
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(cRect, 8.0f, 8.0f), m_brushBorder.Get(), 1.0f);

    const wchar_t hints[] =
        L"1 … 6 : Open Recent File      Ctrl+Shift+T : Reopen Closed Tab\n"
        L"F1 : All Shortcuts & Help      Ctrl+O : Open File Dialog      B : Toggle Backdrop";

    D2D1_RECT_F hRect = D2D1::RectF(cardX + 16.0f * m_dpiScale, curY + 12.0f * m_dpiScale, cardX + cardW - 16.0f * m_dpiScale, curY + cardH - 12.0f * m_dpiScale);
    m_d2dContext->DrawText(hints, static_cast<UINT32>(wcslen(hints)), m_formatInfo.Get(), hRect, m_brushTextMuted.Get());
}

const std::vector<PaperPreset>& D2DRenderer::GetPaperPresets() {
    static const std::vector<PaperPreset> presets = {
        { DMPAPER_A4,     L"A4 (210 × 297 mm)",     210.0f, 297.0f },
        { DMPAPER_LETTER, L"Letter (8.5 × 11 in)",  215.9f, 279.4f },
        { DMPAPER_LEGAL,  L"Legal (8.5 × 14 in)",   215.9f, 355.6f },
        { DMPAPER_A3,     L"A3 (297 × 420 mm)",     297.0f, 420.0f },
        { DMPAPER_A5,     L"A5 (148 × 210 mm)",     148.0f, 210.0f },
        { 0,              L"4 × 6 in (Photo)",       101.6f, 152.4f },
        { 0,              L"5 × 7 in (Photo)",       127.0f, 177.8f },
        { 0,              L"8 × 10 in (Photo)",      203.2f, 254.0f },
    };
    return presets;
}

PrintUiId D2DRenderer::HitTestPrintUi(POINT pt) {
    m_hitDropdownIndex = -1;

    if (pt.x >= m_printCloseRect.left && pt.x <= m_printCloseRect.right &&
        pt.y >= m_printCloseRect.top && pt.y <= m_printCloseRect.bottom) {
        return PrintUiId::CloseModal;
    }

    // Check hit items in reverse order so topmost popovers/dropdowns are hit-tested first!
    for (auto it = m_printHits.rbegin(); it != m_printHits.rend(); ++it) {
        if (pt.x >= it->rect.left && pt.x <= it->rect.right &&
            pt.y >= it->rect.top && pt.y <= it->rect.bottom) {
            m_hitDropdownIndex = it->itemIndex;
            return it->id;
        }
    }

    if (pt.x >= m_printCardRect.left && pt.x <= m_printCardRect.right &&
        pt.y >= m_printCardRect.top && pt.y <= m_printCardRect.bottom) {
        return PrintUiId::CardBody;
    }
    return PrintUiId::None;
}

bool D2DRenderer::RasterizeSvgToPixels(ID2D1SvgDocument* svgDoc, UINT targetW, UINT targetH, std::vector<uint32_t>& outPixels) {
    if (!svgDoc || targetW == 0 || targetH == 0 || !m_d3dDevice || !m_d2dContext) return false;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = targetW;
    texDesc.Height = targetH;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> renderTex;
    HRESULT hr = m_d3dDevice->CreateTexture2D(&texDesc, nullptr, &renderTex);
    if (FAILED(hr)) return false;

    ComPtr<IDXGISurface> dxgiSurface;
    hr = renderTex.As(&dxgiSurface);
    if (FAILED(hr)) return false;

    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );
    ComPtr<ID2D1Bitmap1> offscreenTarget;
    hr = m_d2dContext->CreateBitmapFromDxgiSurface(dxgiSurface.Get(), &bp, &offscreenTarget);
    if (FAILED(hr)) return false;

    ComPtr<ID2D1Image> oldTarget;
    m_d2dContext->GetTarget(&oldTarget);
    m_d2dContext->SetTarget(offscreenTarget.Get());

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0xffffff, 1.0f));

    ComPtr<ID2D1DeviceContext5> ctx5;
    if (SUCCEEDED(m_d2dContext.As(&ctx5))) {
        D2D1_SIZE_F vp = svgDoc->GetViewportSize();
        if (vp.width > 0.0f && vp.height > 0.0f) {
            float sx = static_cast<float>(targetW) / vp.width;
            float sy = static_cast<float>(targetH) / vp.height;
            ctx5->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy));
        }
        ctx5->DrawSvgDocument(svgDoc);
        ctx5->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    m_d2dContext->EndDraw();
    m_d2dContext->SetTarget(oldTarget.Get());

    texDesc.Usage = D3D11_USAGE_STAGING;
    texDesc.BindFlags = 0;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> stagingTex;
    hr = m_d3dDevice->CreateTexture2D(&texDesc, nullptr, &stagingTex);
    if (FAILED(hr)) return false;

    m_d3dContext->CopyResource(stagingTex.Get(), renderTex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = m_d3dContext->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    outPixels.resize(targetW * targetH);
    for (UINT y = 0; y < targetH; ++y) {
        const uint32_t* srcRow = reinterpret_cast<const uint32_t*>(static_cast<const BYTE*>(mapped.pData) + y * mapped.RowPitch);
        memcpy(&outPixels[y * targetW], srcRow, targetW * sizeof(uint32_t));
    }
    m_d3dContext->Unmap(stagingTex.Get(), 0);

    return true;
}

void D2DRenderer::RenderPrintPreview(TabItem* activeTab, const PrintSettings& settings, POINT mousePt) {
    m_printHits.clear();

    float clientW = static_cast<float>(m_width);
    float clientH = static_cast<float>(m_height);

    // 1. Dark Backdrop Scrim
    m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.75f));
    m_d2dContext->FillRectangle(D2D1::RectF(0, 0, clientW, clientH), m_brushBg.Get());

    // 2. Modal Card
    float cardW = std::min(920.0f * m_dpiScale, clientW - 24.0f);
    float cardH = std::min(580.0f * m_dpiScale, clientH - 24.0f);
    float cardX = (clientW - cardW) / 2.0f;
    float cardY = (clientH - cardH) / 2.0f;

    m_printCardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.98f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_printCardRect, 12.0f, 12.0f), m_brushChrome.Get());

    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.9f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(m_printCardRect, 12.0f, 12.0f), m_brushBorder.Get(), 1.5f);

    // 3. Header Bar
    float headerPad = 20.0f * m_dpiScale;
    float titleY = cardY + 16.0f * m_dpiScale;

    m_brushText->SetColor(D2D1::ColorF(0xf8fafc));
    const wchar_t title[] = L"Print Preview";
    D2D1_RECT_F titleRect = D2D1::RectF(cardX + headerPad, titleY, cardX + cardW - 60.0f * m_dpiScale, titleY + 22.0f * m_dpiScale);
    m_d2dContext->DrawText(title, static_cast<UINT32>(wcslen(title)), m_formatHelpCat.Get(), titleRect, m_brushText.Get());

    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    const wchar_t sub[] = L"High-Resolution Native Printing";
    D2D1_RECT_F subRect = D2D1::RectF(cardX + headerPad, titleY + 20.0f * m_dpiScale, cardX + cardW - 60.0f * m_dpiScale, titleY + 36.0f * m_dpiScale);
    m_d2dContext->DrawText(sub, static_cast<UINT32>(wcslen(sub)), m_formatKeyDesc.Get(), subRect, m_brushTextMuted.Get());

    // Close button ('✕')
    float closeSize = 24.0f * m_dpiScale;
    float closeX1 = cardX + cardW - headerPad - closeSize;
    float closeY1 = cardY + 16.0f * m_dpiScale;
    m_printCloseRect = D2D1::RectF(closeX1, closeY1, closeX1 + closeSize, closeY1 + closeSize);

    bool isCloseHov = (mousePt.x >= m_printCloseRect.left && mousePt.x <= m_printCloseRect.right &&
                       mousePt.y >= m_printCloseRect.top && mousePt.y <= m_printCloseRect.bottom);

    if (isCloseHov) {
        m_brushChrome->SetColor(D2D1::ColorF(0xef4444, 0.85f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_printCloseRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushText->SetColor(D2D1::ColorF(0xffffff));
    } else {
        m_brushText->SetColor(D2D1::ColorF(0x94a3b8));
    }
    const wchar_t closeIcon[] = L"✕";
    m_d2dContext->DrawText(closeIcon, 1, m_formatHud.Get(), m_printCloseRect, m_brushText.Get());

    // Divider Line below header
    float divY = cardY + 54.0f * m_dpiScale;
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(cardX + headerPad, divY), D2D1::Point2F(cardX + cardW - headerPad, divY), m_brushBorder.Get(), 1.0f);

    // 4. Split Layout
    float sidebarW = 310.0f * m_dpiScale;
    float leftAreaX = cardX + 16.0f * m_dpiScale;
    float leftAreaY = divY + 12.0f * m_dpiScale;
    float leftAreaW = cardW - sidebarW - 32.0f * m_dpiScale;
    float leftAreaH = cardH - (divY - cardY) - 24.0f * m_dpiScale;

    // Vertical Divider
    float vDivX = cardX + cardW - sidebarW - 8.0f * m_dpiScale;
    m_d2dContext->DrawLine(D2D1::Point2F(vDivX, divY + 8.0f * m_dpiScale), D2D1::Point2F(vDivX, cardY + cardH - 16.0f * m_dpiScale), m_brushBorder.Get(), 1.0f);

    // 5. Left Area: Virtual Paper Simulation
    const auto& presets = GetPaperPresets();
    int presetIdx = std::clamp(settings.paperPresetIdx, 0, static_cast<int>(presets.size() - 1));
    const auto& preset = presets[presetIdx];

    float pwMm = (settings.orientation == PrintOrientation::Portrait) ? preset.widthMm : preset.heightMm;
    float phMm = (settings.orientation == PrintOrientation::Portrait) ? preset.heightMm : preset.widthMm;
    float paperAspect = pwMm / phMm;

    float availW = leftAreaW - 32.0f * m_dpiScale;
    float availH = leftAreaH - 44.0f * m_dpiScale; // reserve 44px for bottom caption
    float paperW = availW;
    float paperH = availH;
    if (availW / availH > paperAspect) {
        paperH = availH;
        paperW = paperH * paperAspect;
    } else {
        paperW = availW;
        paperH = paperW / paperAspect;
    }

    float paperX = leftAreaX + (leftAreaW - paperW) / 2.0f;
    float paperY = leftAreaY + (availH - paperH) / 2.0f;
    D2D1_RECT_F paperRect = D2D1::RectF(paperX, paperY, paperX + paperW, paperY + paperH);

    // Drop Shadow
    m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.45f));
    m_d2dContext->FillRectangle(D2D1::RectF(paperX + 5.0f, paperY + 5.0f, paperX + paperW + 5.0f, paperY + paperH + 5.0f), m_brushBg.Get());

    // Paper Sheet (White)
    m_brushText->SetColor(D2D1::ColorF(0xffffff, 1.0f));
    m_d2dContext->FillRectangle(paperRect, m_brushText.Get());
    m_brushBorder->SetColor(D2D1::ColorF(0xd4d4d8, 1.0f));
    m_d2dContext->DrawRectangle(paperRect, m_brushBorder.Get(), 1.0f);

    // Margins
    float marginMm = 12.7f; // Normal: 0.5 in
    if (settings.marginMode == PrintMarginMode::Narrow) marginMm = 6.35f;
    else if (settings.marginMode == PrintMarginMode::Borderless) marginMm = 0.0f;

    float mmToPx = paperW / pwMm;
    float marginPxX = marginMm * mmToPx;
    float marginPxY = marginMm * mmToPx;

    D2D1_RECT_F marginRect = D2D1::RectF(
        paperX + marginPxX,
        paperY + marginPxY,
        paperX + paperW - marginPxX,
        paperY + paperH - marginPxY
    );

    if (marginMm > 0.0f) {
        m_brushBorder->SetColor(D2D1::ColorF(0x94a3b8, 0.6f));
        m_d2dContext->DrawRectangle(marginRect, m_brushBorder.Get(), 1.0f);
    }

    // Image Rendering
    if (activeTab && activeTab->document && activeTab->document->isValid) {
        float imgW = static_cast<float>(activeTab->document->GetWidth());
        float imgH = static_cast<float>(activeTab->document->GetHeight());
        if (imgW > 0 && imgH > 0) {
            bool isRot90 = (activeTab->rotationDegrees == 90 || activeTab->rotationDegrees == 270);
            float visW = isRot90 ? imgH : imgW;
            float visH = isRot90 ? imgW : imgH;
            float visAspect = visW / visH;

            float mw = std::max(1.0f, marginRect.right - marginRect.left);
            float mh = std::max(1.0f, marginRect.bottom - marginRect.top);
            float marginAspect = mw / mh;

            float destW = mw;
            float destH = mh;
            bool clipNeeded = false;

            if (settings.scaleMode == PrintScaleMode::FitToPage) {
                if (marginAspect > visAspect) {
                    destH = mh;
                    destW = destH * visAspect;
                } else {
                    destW = mw;
                    destH = destW / visAspect;
                }
            } else if (settings.scaleMode == PrintScaleMode::FillPage) {
                clipNeeded = true;
                if (marginAspect > visAspect) {
                    destW = mw;
                    destH = destW / visAspect;
                } else {
                    destH = mh;
                    destW = destH * visAspect;
                }
            } else { // ActualSize (1:1 physical size at 96 DPI)
                clipNeeded = true;
                float physW_mm = (visW / 96.0f) * 25.4f;
                float physH_mm = (visH / 96.0f) * 25.4f;
                destW = physW_mm * mmToPx;
                destH = physH_mm * mmToPx;
            }

            float destX = marginRect.left + (mw - destW) / 2.0f;
            float destY = marginRect.top + (mh - destH) / 2.0f;
            D2D1_RECT_F destRect = D2D1::RectF(destX, destY, destX + destW, destY + destH);

            if (clipNeeded) {
                m_d2dContext->PushAxisAlignedClip(marginRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            }

            float cx = (destRect.left + destRect.right) / 2.0f;
            float cy = (destRect.top + destRect.bottom) / 2.0f;

            D2D1_MATRIX_3X2_F xform = D2D1::Matrix3x2F::Rotation(static_cast<float>(activeTab->rotationDegrees), D2D1::Point2F(cx, cy))
                * D2D1::Matrix3x2F::Scale(activeTab->flipH ? -1.0f : 1.0f, activeTab->flipV ? -1.0f : 1.0f, D2D1::Point2F(cx, cy));
            m_d2dContext->SetTransform(xform);

            D2D1_RECT_F drawRect = destRect;
            if (isRot90) {
                drawRect = D2D1::RectF(cx - destH / 2.0f, cy - destW / 2.0f, cx + destH / 2.0f, cy + destW / 2.0f);
            }

            if (activeTab->document->metadata.isSvg && activeTab->svgDocument) {
                ComPtr<ID2D1DeviceContext5> ctx5;
                if (SUCCEEDED(m_d2dContext.As(&ctx5))) {
                    float sx = (drawRect.right - drawRect.left) / imgW;
                    float sy = (drawRect.bottom - drawRect.top) / imgH;
                    D2D1_MATRIX_3X2_F svgXform = D2D1::Matrix3x2F::Translation(-imgW / 2.0f, -imgH / 2.0f)
                        * D2D1::Matrix3x2F::Scale(sx, sy)
                        * D2D1::Matrix3x2F::Translation(cx, cy)
                        * xform;
                    ctx5->SetTransform(svgXform);
                    ctx5->DrawSvgDocument(activeTab->svgDocument.Get());
                }
            } else {
                size_t frameCount = activeTab->document->GetFrameCount();
                if (frameCount > 0) {
                    size_t fIdx = settings.isAnimated
                        ? (settings.selectedFrameIdx % frameCount)
                        : (activeTab->currentFrame % frameCount);
                    if (fIdx >= activeTab->gpuBitmaps.size()) {
                        activeTab->gpuBitmaps.resize(frameCount);
                    }
                    if (!activeTab->gpuBitmaps[fIdx]) {
                        const FrameData* fd = activeTab->document->GetFrame(fIdx);
                        if (fd && fd->bitmap) {
                            m_d2dContext->CreateBitmapFromWicBitmap(fd->bitmap.Get(), nullptr, &activeTab->gpuBitmaps[fIdx]);
                        }
                    }
                    if (activeTab->gpuBitmaps[fIdx]) {
                        m_d2dContext->DrawBitmap(activeTab->gpuBitmaps[fIdx].Get(), drawRect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
                    }
                }
            }

            m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
            if (clipNeeded) {
                m_d2dContext->PopAxisAlignedClip();
            }
        }
    }

    // Virtual Paper Footer
    wchar_t szFooter[256];
    swprintf_s(szFooter, L"%s • %s • %s • %s",
        preset.name,
        settings.orientation == PrintOrientation::Portrait ? L"Portrait" : L"Landscape",
        settings.scaleMode == PrintScaleMode::FitToPage ? L"Fit to Page" : (settings.scaleMode == PrintScaleMode::FillPage ? L"Fill (Crop)" : L"1:1 Actual Size"),
        settings.marginMode == PrintMarginMode::Normal ? L"Normal 0.5\"" : (settings.marginMode == PrintMarginMode::Narrow ? L"Narrow 0.25\"" : L"Borderless")
    );
    D2D1_RECT_F footerRect = D2D1::RectF(leftAreaX, cardY + cardH - 34.0f * m_dpiScale, leftAreaX + leftAreaW, cardY + cardH - 12.0f * m_dpiScale);
    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    m_d2dContext->DrawText(szFooter, static_cast<UINT32>(wcslen(szFooter)), m_formatKeyDesc.Get(), footerRect, m_brushTextMuted.Get());

    // 6. Right Controls Sidebar
    float sideX = cardX + cardW - sidebarW + 8.0f * m_dpiScale;
    float sideW = sidebarW - 24.0f * m_dpiScale;
    float curY = divY + 12.0f * m_dpiScale;

    auto DrawSectionLabel = [&](const wchar_t* txt) {
        D2D1_RECT_F r = D2D1::RectF(sideX, curY, sideX + sideW, curY + 16.0f * m_dpiScale);
        m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(txt, static_cast<UINT32>(wcslen(txt)), m_formatKeyDesc.Get(), r, m_brushTextMuted.Get());
        curY += 18.0f * m_dpiScale;
    };

    auto DrawDropdownSelector = [&](const wchar_t* valText, PrintUiId btnId, bool isOpen, D2D1_RECT_F& outRect) {
        float boxH = 28.0f * m_dpiScale;
        outRect = D2D1::RectF(sideX, curY, sideX + sideW, curY + boxH);
        m_printHits.push_back({ btnId, -1, outRect });

        bool isHov = (mousePt.x >= outRect.left && mousePt.x <= outRect.right && mousePt.y >= outRect.top && mousePt.y <= outRect.bottom);
        if (isOpen) {
            m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 1.0f));
            m_brushBorder->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
        } else if (isHov) {
            m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 0.9f));
            m_brushBorder->SetColor(D2D1::ColorF(0x475569, 0.9f));
        } else {
            m_brushChrome->SetColor(D2D1::ColorF(0x222226, 0.8f));
            m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
        }

        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(outRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(outRect, 4.0f, 4.0f), m_brushBorder.Get(), isOpen ? 1.5f : 1.0f);

        D2D1_RECT_F textR = D2D1::RectF(sideX + 10.0f * m_dpiScale, curY, sideX + sideW - 24.0f * m_dpiScale, curY + boxH);
        D2D1_RECT_F chevR = D2D1::RectF(sideX + sideW - 22.0f * m_dpiScale, curY, sideX + sideW - 4.0f * m_dpiScale, curY + boxH);

        m_brushText->SetColor(isOpen ? D2D1::ColorF(0xffffff) : (isHov ? D2D1::ColorF(0xffffff) : D2D1::ColorF(0xf1f5f9)));
        m_d2dContext->DrawText(valText, static_cast<UINT32>(wcslen(valText)), m_formatKeyDesc.Get(), textR, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        m_brushTextMuted->SetColor(isOpen ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        const wchar_t* chev = isOpen ? L"▴" : L"▾";
        m_d2dContext->DrawText(chev, 1, m_formatKeycap.Get(), chevR, m_brushTextMuted.Get());

        curY += boxH + 8.0f * m_dpiScale;
    };

    auto DrawPills = [&](const std::vector<std::pair<const wchar_t*, PrintUiId>>& pills, int activeIdx) {
        float pillH = 26.0f * m_dpiScale;
        float spacing = 6.0f * m_dpiScale;
        float pillW = (sideW - (pills.size() - 1) * spacing) / static_cast<float>(pills.size());
        for (size_t i = 0; i < pills.size(); ++i) {
            float px = sideX + i * (pillW + spacing);
            D2D1_RECT_F pRect = D2D1::RectF(px, curY, px + pillW, curY + pillH);
            m_printHits.push_back({ pills[i].second, -1, pRect });

            bool isActive = (static_cast<int>(i) == activeIdx);
            bool isHov = (mousePt.x >= pRect.left && mousePt.x <= pRect.right && mousePt.y >= pRect.top && mousePt.y <= pRect.bottom);

            if (isActive) {
                m_brushChrome->SetColor(D2D1::ColorF(0x0284c7, 0.9f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushChrome.Get());
                m_brushText->SetColor(D2D1::ColorF(0xffffff));
            } else {
                m_brushChrome->SetColor(isHov ? D2D1::ColorF(0x334155, 0.8f) : D2D1::ColorF(0x1e293b, 0.6f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushChrome.Get());
                m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
                m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
                m_brushText->SetColor(isHov ? D2D1::ColorF(0xffffff) : D2D1::ColorF(0x94a3b8));
            }

            m_d2dContext->DrawText(pills[i].first, static_cast<UINT32>(wcslen(pills[i].first)), m_formatKeycap.Get(), pRect, m_brushText.Get());
        }
        curY += pillH + 8.0f * m_dpiScale;
    };

    // 1. Printer Dropdown Box
    D2D1_RECT_F printerBoxRect;
    std::wstring printerName = settings.printers.empty() ? L"Default Printer" : settings.printers[settings.selectedPrinterIdx % settings.printers.size()];
    DrawSectionLabel(L"PRINTER");
    DrawDropdownSelector(printerName.c_str(), PrintUiId::PrinterDropdownBtn, settings.openDropdown == PrintDropdownState::Printer, printerBoxRect);

    // 2. Paper Size Dropdown Box
    D2D1_RECT_F paperBoxRect;
    DrawSectionLabel(L"PAPER SIZE");
    DrawDropdownSelector(preset.name, PrintUiId::PaperDropdownBtn, settings.openDropdown == PrintDropdownState::PaperSize, paperBoxRect);

    // 2b. GIF / Multi-Frame Stepper
    if (settings.isAnimated && settings.totalFrameCount > 1) {
        DrawSectionLabel(L"ANIMATION FRAME");
        float boxH = 28.0f * m_dpiScale;
        float btnW = 28.0f * m_dpiScale;

        D2D1_RECT_F prevR = D2D1::RectF(sideX, curY, sideX + btnW, curY + boxH);
        D2D1_RECT_F nextR = D2D1::RectF(sideX + sideW - btnW, curY, sideX + sideW, curY + boxH);
        D2D1_RECT_F textR = D2D1::RectF(sideX + btnW + 4.0f * m_dpiScale, curY, sideX + sideW - btnW - 4.0f * m_dpiScale, curY + boxH);

        m_printHits.push_back({ PrintUiId::FramePrev, -1, prevR });
        m_printHits.push_back({ PrintUiId::FrameNext, -1, nextR });

        bool prevHov = (mousePt.x >= prevR.left && mousePt.x <= prevR.right && mousePt.y >= prevR.top && mousePt.y <= prevR.bottom);
        bool nextHov = (mousePt.x >= nextR.left && mousePt.x <= nextR.right && mousePt.y >= nextR.top && mousePt.y <= nextR.bottom);

        D2D1_RECT_F boxRect = D2D1::RectF(sideX, curY, sideX + sideW, curY + boxH);
        m_brushChrome->SetColor(D2D1::ColorF(0x222226, 0.8f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(boxRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(boxRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);

        m_brushText->SetColor(prevHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(L"◀", 1, m_formatKeycap.Get(), prevR, m_brushText.Get());

        m_brushText->SetColor(nextHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(L"▶", 1, m_formatKeycap.Get(), nextR, m_brushText.Get());

        wchar_t szFrame[64];
        swprintf_s(szFrame, L"Frame %zu of %zu", settings.selectedFrameIdx + 1, settings.totalFrameCount);
        m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
        m_d2dContext->DrawText(szFrame, static_cast<UINT32>(wcslen(szFrame)), m_formatKeycap.Get(), textR, m_brushText.Get());

        curY += boxH + 8.0f * m_dpiScale;
    }

    // 3. Orientation
    DrawSectionLabel(L"ORIENTATION");
    DrawPills({ { L"Portrait", PrintUiId::OrientPortrait }, { L"Landscape", PrintUiId::OrientLandscape } }, settings.orientation == PrintOrientation::Portrait ? 0 : 1);

    // 4. Scale Mode
    DrawSectionLabel(L"SCALE MODE");
    DrawPills({ { L"Fit", PrintUiId::ScaleFit }, { L"Fill", PrintUiId::ScaleFill }, { L"1:1", PrintUiId::ScaleActual } }, settings.scaleMode == PrintScaleMode::FitToPage ? 0 : (settings.scaleMode == PrintScaleMode::FillPage ? 1 : 2));

    // 5. Margins
    DrawSectionLabel(L"MARGINS");
    DrawPills({ { L"Normal", PrintUiId::MarginNormal }, { L"Narrow", PrintUiId::MarginNarrow }, { L"None", PrintUiId::MarginNone } }, settings.marginMode == PrintMarginMode::Normal ? 0 : (settings.marginMode == PrintMarginMode::Narrow ? 1 : 2));

    // 6. Copies Stepper + Keyboard Input Box
    DrawSectionLabel(L"COPIES");
    float boxH = 28.0f * m_dpiScale;
    float btnW = 28.0f * m_dpiScale;

    D2D1_RECT_F prevR = D2D1::RectF(sideX, curY, sideX + btnW, curY + boxH);
    D2D1_RECT_F nextR = D2D1::RectF(sideX + sideW - btnW, curY, sideX + sideW, curY + boxH);
    D2D1_RECT_F inputR = D2D1::RectF(sideX + btnW + 4.0f * m_dpiScale, curY, sideX + sideW - btnW - 4.0f * m_dpiScale, curY + boxH);

    m_printHits.push_back({ PrintUiId::CopiesMinus, -1, prevR });
    m_printHits.push_back({ PrintUiId::CopiesInputBox, -1, inputR });
    m_printHits.push_back({ PrintUiId::CopiesPlus, -1, nextR });

    bool prevHov = (mousePt.x >= prevR.left && mousePt.x <= prevR.right && mousePt.y >= prevR.top && mousePt.y <= prevR.bottom);
    bool nextHov = (mousePt.x >= nextR.left && mousePt.x <= nextR.right && mousePt.y >= nextR.top && mousePt.y <= nextR.bottom);
    bool inputHov = (mousePt.x >= inputR.left && mousePt.x <= inputR.right && mousePt.y >= inputR.top && mousePt.y <= inputR.bottom);

    // Minus Button
    m_brushChrome->SetColor(prevHov ? D2D1::ColorF(0x334155, 0.9f) : D2D1::ColorF(0x27272a, 0.8f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(prevR, 4.0f, 4.0f), m_brushChrome.Get());
    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(prevR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
    m_brushText->SetColor(prevHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
    m_d2dContext->DrawText(L"−", 1, m_formatKeycap.Get(), prevR, m_brushText.Get());

    // Plus Button
    m_brushChrome->SetColor(nextHov ? D2D1::ColorF(0x334155, 0.9f) : D2D1::ColorF(0x27272a, 0.8f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(nextR, 4.0f, 4.0f), m_brushChrome.Get());
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(nextR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
    m_brushText->SetColor(nextHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
    m_d2dContext->DrawText(L"+", 1, m_formatKeycap.Get(), nextR, m_brushText.Get());

    // Copies Text / Input Box
    if (settings.isEditingCopies) {
        m_brushChrome->SetColor(D2D1::ColorF(0x0f172a, 1.0f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushBorder.Get(), 1.5f);

        std::wstring displayStr = settings.copiesEditBuffer.empty() ? L"|" : (settings.copiesEditBuffer + L"|");
        m_brushText->SetColor(D2D1::ColorF(0x38bdf8));
        m_d2dContext->DrawText(displayStr.c_str(), static_cast<UINT32>(displayStr.length()), m_formatKeycap.Get(), inputR, m_brushText.Get());
    } else {
        m_brushChrome->SetColor(inputHov ? D2D1::ColorF(0x27272a, 0.9f) : D2D1::ColorF(0x222226, 0.8f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(inputHov ? D2D1::ColorF(0x475569, 0.9f) : D2D1::ColorF(0x3f3f46, 0.8f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);

        wchar_t szCopies[32];
        swprintf_s(szCopies, L"%d", settings.copies);
        m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
        m_d2dContext->DrawText(szCopies, static_cast<UINT32>(wcslen(szCopies)), m_formatKeycap.Get(), inputR, m_brushText.Get());
    }

    curY += boxH + 8.0f * m_dpiScale;
    curY += 4.0f * m_dpiScale;

    // Action Buttons
    float btnH = 30.0f * m_dpiScale;
    D2D1_RECT_F sysRect = D2D1::RectF(sideX, curY, sideX + sideW, curY + btnH);
    m_printHits.push_back({ PrintUiId::SystemDialog, -1, sysRect });
    bool sysHov = (mousePt.x >= sysRect.left && mousePt.x <= sysRect.right && mousePt.y >= sysRect.top && mousePt.y <= sysRect.bottom);
    m_brushChrome->SetColor(sysHov ? D2D1::ColorF(0x334155, 0.9f) : D2D1::ColorF(0x27272a, 0.8f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(sysRect, 4.0f, 4.0f), m_brushChrome.Get());
    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(sysRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
    m_brushText->SetColor(sysHov ? D2D1::ColorF(0xffffff) : D2D1::ColorF(0x94a3b8));
    const wchar_t sysText[] = L"⚙️ System Dialog...";
    m_d2dContext->DrawText(sysText, static_cast<UINT32>(wcslen(sysText)), m_formatKeycap.Get(), sysRect, m_brushText.Get());

    curY += btnH + 8.0f * m_dpiScale;

    float printBtnH = 34.0f * m_dpiScale;
    D2D1_RECT_F printRect = D2D1::RectF(sideX, curY, sideX + sideW, curY + printBtnH);
    m_printHits.push_back({ PrintUiId::ExecutePrint, -1, printRect });
    bool printHov = (mousePt.x >= printRect.left && mousePt.x <= printRect.right && mousePt.y >= printRect.top && mousePt.y <= printRect.bottom);
    m_brushChrome->SetColor(printHov ? D2D1::ColorF(0x0284c7, 1.0f) : D2D1::ColorF(0x0369a1, 0.95f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(printRect, 6.0f, 6.0f), m_brushChrome.Get());
    m_brushText->SetColor(D2D1::ColorF(0xffffff));
    const wchar_t printText[] = L"🖨️ Print (Enter)";
    m_d2dContext->DrawText(printText, static_cast<UINT32>(wcslen(printText)), m_formatKeycap.Get(), printRect, m_brushText.Get());

    // 7. Floating Dropdown Popover Lists (Rendered last with topmost z-index)
    if (settings.openDropdown == PrintDropdownState::Printer && !settings.printers.empty()) {
        float itemH = 28.0f * m_dpiScale;
        float listH = itemH * settings.printers.size() + 8.0f * m_dpiScale;
        float listY = printerBoxRect.bottom + 2.0f;
        D2D1_RECT_F listRect = D2D1::RectF(sideX, listY, sideX + sideW, listY + listH);
        m_dropdownListRect = listRect;

        // Shadow
        m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.65f));
        m_d2dContext->FillRectangle(D2D1::RectF(sideX + 3.0f, listY + 3.0f, sideX + sideW + 3.0f, listY + listH + 3.0f), m_brushBg.Get());

        // Card
        m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.98f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(listRect, 6.0f, 6.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x475569, 0.95f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(listRect, 6.0f, 6.0f), m_brushBorder.Get(), 1.2f);

        for (size_t i = 0; i < settings.printers.size(); ++i) {
            float iy = listY + 4.0f * m_dpiScale + i * itemH;
            D2D1_RECT_F itemR = D2D1::RectF(sideX + 4.0f * m_dpiScale, iy, sideX + sideW - 4.0f * m_dpiScale, iy + itemH);
            m_printHits.push_back({ PrintUiId::DropdownItem, static_cast<int>(i), itemR });

            bool isHov = (mousePt.x >= itemR.left && mousePt.x <= itemR.right && mousePt.y >= itemR.top && mousePt.y <= itemR.bottom);
            bool isSel = (static_cast<int>(i) == settings.selectedPrinterIdx);

            if (isHov) {
                m_brushChrome->SetColor(D2D1::ColorF(0x334155, 0.95f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(itemR, 4.0f, 4.0f), m_brushChrome.Get());
            } else if (isSel) {
                m_brushChrome->SetColor(D2D1::ColorF(0x1e293b, 0.7f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(itemR, 4.0f, 4.0f), m_brushChrome.Get());
            }

            D2D1_RECT_F textR = D2D1::RectF(itemR.left + 22.0f * m_dpiScale, itemR.top, itemR.right - 8.0f * m_dpiScale, itemR.bottom);
            D2D1_RECT_F checkR = D2D1::RectF(itemR.left + 4.0f * m_dpiScale, itemR.top, itemR.left + 20.0f * m_dpiScale, itemR.bottom);

            if (isSel) {
                m_brushAccent->SetColor(D2D1::ColorF(0x38bdf8));
                m_d2dContext->DrawText(L"✓", 1, m_formatKeycap.Get(), checkR, m_brushAccent.Get());
            }

            m_brushText->SetColor(isHov ? D2D1::ColorF(0xffffff) : (isSel ? D2D1::ColorF(0xf1f5f9) : D2D1::ColorF(0x94a3b8)));
            m_d2dContext->DrawText(settings.printers[i].c_str(), static_cast<UINT32>(settings.printers[i].length()), m_formatKeyDesc.Get(), textR, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    } else if (settings.openDropdown == PrintDropdownState::PaperSize) {
        float itemH = 26.0f * m_dpiScale;
        float listH = itemH * presets.size() + 8.0f * m_dpiScale;
        float listY = paperBoxRect.bottom + 2.0f;
        if (listY + listH > cardY + cardH - 12.0f * m_dpiScale) {
            listY = cardY + cardH - 12.0f * m_dpiScale - listH;
        }
        D2D1_RECT_F listRect = D2D1::RectF(sideX, listY, sideX + sideW, listY + listH);
        m_dropdownListRect = listRect;

        m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.65f));
        m_d2dContext->FillRectangle(D2D1::RectF(sideX + 3.0f, listY + 3.0f, sideX + sideW + 3.0f, listY + listH + 3.0f), m_brushBg.Get());

        m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.98f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(listRect, 6.0f, 6.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x475569, 0.95f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(listRect, 6.0f, 6.0f), m_brushBorder.Get(), 1.2f);

        for (size_t i = 0; i < presets.size(); ++i) {
            float iy = listY + 4.0f * m_dpiScale + i * itemH;
            D2D1_RECT_F itemR = D2D1::RectF(sideX + 4.0f * m_dpiScale, iy, sideX + sideW - 4.0f * m_dpiScale, iy + itemH);
            m_printHits.push_back({ PrintUiId::DropdownItem, static_cast<int>(i), itemR });

            bool isHov = (mousePt.x >= itemR.left && mousePt.x <= itemR.right && mousePt.y >= itemR.top && mousePt.y <= itemR.bottom);
            bool isSel = (static_cast<int>(i) == presetIdx);

            if (isHov) {
                m_brushChrome->SetColor(D2D1::ColorF(0x334155, 0.95f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(itemR, 4.0f, 4.0f), m_brushChrome.Get());
            } else if (isSel) {
                m_brushChrome->SetColor(D2D1::ColorF(0x1e293b, 0.7f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(itemR, 4.0f, 4.0f), m_brushChrome.Get());
            }

            D2D1_RECT_F textR = D2D1::RectF(itemR.left + 22.0f * m_dpiScale, itemR.top, itemR.right - 8.0f * m_dpiScale, itemR.bottom);
            D2D1_RECT_F checkR = D2D1::RectF(itemR.left + 4.0f * m_dpiScale, itemR.top, itemR.left + 20.0f * m_dpiScale, itemR.bottom);

            if (isSel) {
                m_brushAccent->SetColor(D2D1::ColorF(0x38bdf8));
                m_d2dContext->DrawText(L"✓", 1, m_formatKeycap.Get(), checkR, m_brushAccent.Get());
            }

            m_brushText->SetColor(isHov ? D2D1::ColorF(0xffffff) : (isSel ? D2D1::ColorF(0xf1f5f9) : D2D1::ColorF(0x94a3b8)));
            m_d2dContext->DrawText(presets[i].name, static_cast<UINT32>(wcslen(presets[i].name)), m_formatKeyDesc.Get(), textR, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    } else {
        m_dropdownListRect = { 0, 0, 0, 0 };
    }
}

ExportUiId D2DRenderer::HitTestExportUi(POINT pt) {
    m_hitExportDropdownIndex = -1;

    if (pt.x >= m_exportCloseRect.left && pt.x <= m_exportCloseRect.right &&
        pt.y >= m_exportCloseRect.top && pt.y <= m_exportCloseRect.bottom) {
        return ExportUiId::CloseModal;
    }

    // Reverse iteration ensures floating dropdown items (pushed last) take hit priority
    for (auto it = m_exportHits.rbegin(); it != m_exportHits.rend(); ++it) {
        if (pt.x >= it->rect.left && pt.x <= it->rect.right &&
            pt.y >= it->rect.top && pt.y <= it->rect.bottom) {
            m_hitExportDropdownIndex = it->itemIndex;
            return it->id;
        }
    }

    if (pt.x >= m_exportCardRect.left && pt.x <= m_exportCardRect.right &&
        pt.y >= m_exportCardRect.top && pt.y <= m_exportCardRect.bottom) {
        return ExportUiId::CardBody;
    }
    return ExportUiId::None;
}

void D2DRenderer::RenderExportModal(TabItem* activeTab, const ExportSettings& settings, POINT mousePt) {
    m_exportHits.clear();
    m_exportSliderRect = { 0, 0, 0, 0 };

    float clientW = static_cast<float>(m_width);
    float clientH = static_cast<float>(m_height);

    // 1. Dark Backdrop Scrim
    m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.75f));
    m_d2dContext->FillRectangle(D2D1::RectF(0, 0, clientW, clientH), m_brushBg.Get());

    // 2. Modal Card
    float cardW = std::min(920.0f * m_dpiScale, clientW - 24.0f);
    float cardH = std::min(590.0f * m_dpiScale, clientH - 24.0f);
    float cardX = (clientW - cardW) / 2.0f;
    float cardY = (clientH - cardH) / 2.0f;

    m_exportCardRect = D2D1::RectF(cardX, cardY, cardX + cardW, cardY + cardH);

    m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.98f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_exportCardRect, 12.0f, 12.0f), m_brushChrome.Get());

    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.9f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(m_exportCardRect, 12.0f, 12.0f), m_brushBorder.Get(), 1.5f);

    // 3. Header Bar
    float headerPad = 20.0f * m_dpiScale;
    float titleY = cardY + 14.0f * m_dpiScale;

    m_brushText->SetColor(D2D1::ColorF(0xf8fafc));
    const wchar_t title[] = L"Export & Convert Image";
    D2D1_RECT_F titleRect = D2D1::RectF(cardX + headerPad, titleY, cardX + cardW - 60.0f * m_dpiScale, titleY + 22.0f * m_dpiScale);
    m_d2dContext->DrawText(title, static_cast<UINT32>(wcslen(title)), m_formatHelpCat.Get(), titleRect, m_brushText.Get());

    m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
    const wchar_t sub[] = L"High-Speed Native WIC Format Conversion & Resampling";
    D2D1_RECT_F subRect = D2D1::RectF(cardX + headerPad, titleY + 20.0f * m_dpiScale, cardX + cardW - 60.0f * m_dpiScale, titleY + 36.0f * m_dpiScale);
    m_d2dContext->DrawText(sub, static_cast<UINT32>(wcslen(sub)), m_formatKeyDesc.Get(), subRect, m_brushTextMuted.Get());

    // Close button ('✕')
    float closeSize = 24.0f * m_dpiScale;
    float closeX1 = cardX + cardW - headerPad - closeSize;
    float closeY1 = cardY + 14.0f * m_dpiScale;
    m_exportCloseRect = D2D1::RectF(closeX1, closeY1, closeX1 + closeSize, closeY1 + closeSize);

    bool isCloseHov = (mousePt.x >= m_exportCloseRect.left && mousePt.x <= m_exportCloseRect.right &&
                       mousePt.y >= m_exportCloseRect.top && mousePt.y <= m_exportCloseRect.bottom);
    if (isCloseHov) {
        m_brushChrome->SetColor(D2D1::ColorF(0xef4444, 0.85f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(m_exportCloseRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushText->SetColor(D2D1::ColorF(0xffffff));
    } else {
        m_brushText->SetColor(D2D1::ColorF(0x94a3b8));
    }
    const wchar_t closeIcon[] = L"✕";
    m_d2dContext->DrawText(closeIcon, 1, m_formatHud.Get(), m_exportCloseRect, m_brushText.Get());

    // Divider Line below header
    float divY = cardY + 52.0f * m_dpiScale;
    m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 1.0f));
    m_d2dContext->DrawLine(D2D1::Point2F(cardX + headerPad, divY), D2D1::Point2F(cardX + cardW - headerPad, divY), m_brushBorder.Get(), 1.0f);

    // 4. Split Layout
    float sidebarW = 324.0f * m_dpiScale;
    float leftAreaX = cardX + 16.0f * m_dpiScale;
    float leftAreaY = divY + 12.0f * m_dpiScale;
    float leftAreaW = cardW - sidebarW - 32.0f * m_dpiScale;
    float leftAreaH = cardH - (divY - cardY) - 20.0f * m_dpiScale;

    float vDivX = cardX + cardW - sidebarW - 8.0f * m_dpiScale;
    m_d2dContext->DrawLine(D2D1::Point2F(vDivX, divY + 8.0f * m_dpiScale), D2D1::Point2F(vDivX, cardY + cardH - 14.0f * m_dpiScale), m_brushBorder.Get(), 1.0f);

    const auto& formats = ImageEngine::GetExportFormats();
    int fmtIdx = std::clamp(static_cast<int>(settings.format), 0, static_cast<int>(formats.size() - 1));
    const ExportFormatInfo& fmtInfo = formats[fmtIdx];

    ExportMatte effectiveMatte = settings.matte;
    if (!fmtInfo.supportsAlpha && effectiveMatte == ExportMatte::KeepAlpha) {
        effectiveMatte = ExportMatte::White;
    }

    // 5. Left Area: Live Preview Canvas + Telemetry Footer
    float availW = std::max(40.0f, leftAreaW - 32.0f * m_dpiScale);
    float availH = std::max(40.0f, leftAreaH - 58.0f * m_dpiScale);

    float outAspect = (settings.targetHeight > 0)
        ? (static_cast<float>(settings.targetWidth) / static_cast<float>(settings.targetHeight))
        : 1.0f;

    float prevW = availW;
    float prevH = availH;
    if (availW / availH > outAspect) {
        prevH = availH;
        prevW = prevH * outAspect;
    } else {
        prevW = availW;
        prevH = prevW / outAspect;
    }

    float prevX = leftAreaX + (leftAreaW - prevW) / 2.0f;
    float prevY = leftAreaY + (availH - prevH) / 2.0f;
    D2D1_RECT_F prevRect = D2D1::RectF(prevX, prevY, prevX + prevW, prevY + prevH);

    // Drop Shadow
    m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.45f));
    m_d2dContext->FillRectangle(D2D1::RectF(prevX + 5.0f, prevY + 5.0f, prevX + prevW + 5.0f, prevY + prevH + 5.0f), m_brushBg.Get());

    // Matte / Checkerboard Backdrop
    if (effectiveMatte == ExportMatte::White) {
        m_brushText->SetColor(D2D1::ColorF(0xffffff, 1.0f));
        m_d2dContext->FillRectangle(prevRect, m_brushText.Get());
    } else if (effectiveMatte == ExportMatte::Black) {
        m_brushBg->SetColor(D2D1::ColorF(0x000000, 1.0f));
        m_d2dContext->FillRectangle(prevRect, m_brushBg.Get());
    } else {
        DrawCheckerboard(prevRect);
    }

    // Image Preview Content
    if (activeTab && activeTab->document && activeTab->document->isValid) {
        float imgW = static_cast<float>(activeTab->document->GetWidth());
        float imgH = static_cast<float>(activeTab->document->GetHeight());
        if (imgW > 0.0f && imgH > 0.0f) {
            int rot = settings.bakeTransforms ? activeTab->rotationDegrees : 0;
            bool flipH = settings.bakeTransforms ? activeTab->flipH : false;
            bool flipV = settings.bakeTransforms ? activeTab->flipV : false;
            bool isRot90 = (rot == 90 || rot == 270);

            m_d2dContext->PushAxisAlignedClip(prevRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

            float cx = (prevRect.left + prevRect.right) / 2.0f;
            float cy = (prevRect.top + prevRect.bottom) / 2.0f;

            D2D1_MATRIX_3X2_F xform =
                D2D1::Matrix3x2F::Rotation(static_cast<float>(rot), D2D1::Point2F(cx, cy)) *
                D2D1::Matrix3x2F::Scale(flipH ? -1.0f : 1.0f, flipV ? -1.0f : 1.0f, D2D1::Point2F(cx, cy));
            m_d2dContext->SetTransform(xform);

            D2D1_RECT_F drawRect = prevRect;
            if (isRot90) {
                drawRect = D2D1::RectF(cx - prevH / 2.0f, cy - prevW / 2.0f, cx + prevH / 2.0f, cy + prevW / 2.0f);
            }

            if (activeTab->document->metadata.isSvg && activeTab->svgDocument) {
                ComPtr<ID2D1DeviceContext5> ctx5;
                if (SUCCEEDED(m_d2dContext.As(&ctx5))) {
                    float sx = (drawRect.right - drawRect.left) / imgW;
                    float sy = (drawRect.bottom - drawRect.top) / imgH;
                    D2D1_MATRIX_3X2_F svgXform =
                        D2D1::Matrix3x2F::Translation(-imgW / 2.0f, -imgH / 2.0f) *
                        D2D1::Matrix3x2F::Scale(sx, sy) *
                        D2D1::Matrix3x2F::Translation(cx, cy) *
                        xform;
                    ctx5->SetTransform(svgXform);
                    ctx5->DrawSvgDocument(activeTab->svgDocument.Get());
                }
            } else {
                size_t frameCount = activeTab->document->GetFrameCount();
                if (frameCount > 0) {
                    size_t fIdx = settings.isAnimated
                        ? (settings.selectedFrameIdx % frameCount)
                        : (activeTab->currentFrame % frameCount);
                    if (fIdx >= activeTab->gpuBitmaps.size()) {
                        activeTab->gpuBitmaps.resize(frameCount);
                    }
                    if (!activeTab->gpuBitmaps[fIdx]) {
                        const FrameData* fd = activeTab->document->GetFrame(fIdx);
                        if (fd && fd->bitmap) {
                            m_d2dContext->CreateBitmapFromWicBitmap(fd->bitmap.Get(), nullptr, &activeTab->gpuBitmaps[fIdx]);
                        }
                    }
                    if (activeTab->gpuBitmaps[fIdx]) {
                        m_d2dContext->DrawBitmap(activeTab->gpuBitmaps[fIdx].Get(), drawRect, 1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
                    }
                }
            }

            m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
            m_d2dContext->PopAxisAlignedClip();
        }
    }

    m_brushBorder->SetColor(D2D1::ColorF(0x475569, 0.9f));
    m_d2dContext->DrawRectangle(prevRect, m_brushBorder.Get(), 1.0f);

    // Format Byte Size helper
    auto FormatBytes = [](uint64_t bytes) -> std::wstring {
        wchar_t buf[64];
        if (bytes >= 1024ULL * 1024ULL) {
            swprintf_s(buf, L"%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        } else if (bytes >= 1024ULL) {
            swprintf_s(buf, L"%.1f KB", static_cast<double>(bytes) / 1024.0);
        } else {
            swprintf_s(buf, L"%llu B", bytes);
        }
        return buf;
    };

    // Telemetry Footer Lines
    uint64_t outBytesCount = settings.encodedBytes.size();
    uint64_t srcBytesCount = (activeTab && activeTab->document) ? activeTab->document->metadata.fileSizeBytes : 0;

    std::wstring estStr = settings.encodeValid ? FormatBytes(outBytesCount) : L"Calculating...";
    if (settings.encodeValid && srcBytesCount > 0) {
        double ratio = (static_cast<double>(outBytesCount) - static_cast<double>(srcBytesCount)) * 100.0 / static_cast<double>(srcBytesCount);
        wchar_t rBuf[32];
        if (ratio <= -0.5) swprintf_s(rBuf, L" (%.0f%%)", ratio);
        else if (ratio >= 0.5) swprintf_s(rBuf, L" (+%.0f%%)", ratio);
        else wcscpy_s(rBuf, L" (0%)");
        estStr += rBuf;
    }

    wchar_t line1[256];
    swprintf_s(line1, L"Output: %s (%s) • %u × %u px • Est. Size: %s",
        fmtInfo.shortName, fmtInfo.defaultExt, settings.targetWidth, settings.targetHeight, estStr.c_str());

    D2D1_RECT_F foot1Rect = D2D1::RectF(leftAreaX, cardY + cardH - 48.0f * m_dpiScale, leftAreaX + leftAreaW, cardY + cardH - 28.0f * m_dpiScale);
    m_brushAccent->SetColor(D2D1::ColorF(0x38bdf8));
    m_d2dContext->DrawText(line1, static_cast<UINT32>(wcslen(line1)), m_formatKeycap.Get(), foot1Rect, m_brushAccent.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

    if (activeTab && activeTab->document) {
        const auto& meta = activeTab->document->metadata;
        std::wstring srcSizeStr = FormatBytes(meta.fileSizeBytes);
        wchar_t line2[256];
        swprintf_s(line2, L"Source: %s (%u × %u px • %s • %s)",
            meta.fileName.c_str(), meta.width, meta.height, meta.formatName.c_str(), srcSizeStr.c_str());

        D2D1_RECT_F foot2Rect = D2D1::RectF(leftAreaX, cardY + cardH - 28.0f * m_dpiScale, leftAreaX + leftAreaW, cardY + cardH - 10.0f * m_dpiScale);
        m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(line2, static_cast<UINT32>(wcslen(line2)), m_formatKeyDesc.Get(), foot2Rect, m_brushTextMuted.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    // 6. Right Controls Sidebar
    float sideX = cardX + cardW - sidebarW + 8.0f * m_dpiScale;
    float sideW = sidebarW - 24.0f * m_dpiScale;
    float curY = divY + 10.0f * m_dpiScale;

    auto DrawSectionLabel = [&](const wchar_t* txt) {
        D2D1_RECT_F r = D2D1::RectF(sideX, curY, sideX + sideW, curY + 15.0f * m_dpiScale);
        m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(txt, static_cast<UINT32>(wcslen(txt)), m_formatKeyDesc.Get(), r, m_brushTextMuted.Get());
        curY += 17.0f * m_dpiScale;
    };

    auto DrawPills = [&](const std::vector<std::pair<const wchar_t*, ExportUiId>>& pills, int activeIdx, int disabledIdx = -1) {
        float pillH = 25.0f * m_dpiScale;
        float spacing = 5.0f * m_dpiScale;
        float pillW = (sideW - (pills.size() - 1) * spacing) / static_cast<float>(pills.size());
        for (size_t i = 0; i < pills.size(); ++i) {
            float px = sideX + i * (pillW + spacing);
            D2D1_RECT_F pRect = D2D1::RectF(px, curY, px + pillW, curY + pillH);
            bool isDisabled = (static_cast<int>(i) == disabledIdx);
            if (!isDisabled) {
                m_exportHits.push_back({ pills[i].second, -1, pRect });
            }

            bool isActive = (static_cast<int>(i) == activeIdx) && !isDisabled;
            bool isHov = !isDisabled && (mousePt.x >= pRect.left && mousePt.x <= pRect.right && mousePt.y >= pRect.top && mousePt.y <= pRect.bottom);

            if (isDisabled) {
                m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.4f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushChrome.Get());
                m_brushBorder->SetColor(D2D1::ColorF(0x27272a, 0.5f));
                m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
                m_brushText->SetColor(D2D1::ColorF(0x475569));
            } else if (isActive) {
                m_brushChrome->SetColor(D2D1::ColorF(0x0284c7, 0.9f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushChrome.Get());
                m_brushText->SetColor(D2D1::ColorF(0xffffff));
            } else {
                m_brushChrome->SetColor(isHov ? D2D1::ColorF(0x334155, 0.8f) : D2D1::ColorF(0x1e293b, 0.6f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushChrome.Get());
                m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
                m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(pRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
                m_brushText->SetColor(isHov ? D2D1::ColorF(0xffffff) : D2D1::ColorF(0x94a3b8));
            }

            m_d2dContext->DrawText(pills[i].first, static_cast<UINT32>(wcslen(pills[i].first)), m_formatKeycap.Get(), pRect, m_brushText.Get());
        }
        curY += pillH + 8.0f * m_dpiScale;
    };

    // Row 1: Target Format Dropdown
    DrawSectionLabel(L"TARGET FORMAT");
    float fmtBoxH = 28.0f * m_dpiScale;
    D2D1_RECT_F fmtBoxRect = D2D1::RectF(sideX, curY, sideX + sideW, curY + fmtBoxH);
    m_exportHits.push_back({ ExportUiId::FormatDropdownBtn, -1, fmtBoxRect });

    bool fmtHov = (mousePt.x >= fmtBoxRect.left && mousePt.x <= fmtBoxRect.right &&
                   mousePt.y >= fmtBoxRect.top && mousePt.y <= fmtBoxRect.bottom);
    if (settings.isFormatDropdownOpen) {
        m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 1.0f));
        m_brushBorder->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
    } else if (fmtHov) {
        m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 0.9f));
        m_brushBorder->SetColor(D2D1::ColorF(0x475569, 0.9f));
    } else {
        m_brushChrome->SetColor(D2D1::ColorF(0x222226, 0.8f));
        m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
    }
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(fmtBoxRect, 4.0f, 4.0f), m_brushChrome.Get());
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(fmtBoxRect, 4.0f, 4.0f), m_brushBorder.Get(), settings.isFormatDropdownOpen ? 1.5f : 1.0f);

    D2D1_RECT_F fmtTextR = D2D1::RectF(sideX + 10.0f * m_dpiScale, curY, sideX + sideW - 24.0f * m_dpiScale, curY + fmtBoxH);
    D2D1_RECT_F fmtChevR = D2D1::RectF(sideX + sideW - 22.0f * m_dpiScale, curY, sideX + sideW - 4.0f * m_dpiScale, curY + fmtBoxH);
    m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
    m_d2dContext->DrawText(fmtInfo.displayName, static_cast<UINT32>(wcslen(fmtInfo.displayName)), m_formatKeyDesc.Get(), fmtTextR, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    m_brushTextMuted->SetColor(settings.isFormatDropdownOpen ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
    const wchar_t* chev = settings.isFormatDropdownOpen ? L"▴" : L"▾";
    m_d2dContext->DrawText(chev, 1, m_formatKeycap.Get(), fmtChevR, m_brushTextMuted.Get());
    curY += fmtBoxH + 8.0f * m_dpiScale;

    // Row 2: Multi-Frame / GIF Stepper (if animated)
    if (settings.isAnimated && settings.totalFrameCount > 1) {
        DrawSectionLabel(L"ANIMATION FRAME");
        float boxH = 26.0f * m_dpiScale;
        float btnW = 28.0f * m_dpiScale;

        D2D1_RECT_F prevR = D2D1::RectF(sideX, curY, sideX + btnW, curY + boxH);
        D2D1_RECT_F nextR = D2D1::RectF(sideX + sideW - btnW, curY, sideX + sideW, curY + boxH);
        D2D1_RECT_F textR = D2D1::RectF(sideX + btnW + 4.0f * m_dpiScale, curY, sideX + sideW - btnW - 4.0f * m_dpiScale, curY + boxH);

        m_exportHits.push_back({ ExportUiId::FramePrev, -1, prevR });
        m_exportHits.push_back({ ExportUiId::FrameNext, -1, nextR });

        bool prevHov = (mousePt.x >= prevR.left && mousePt.x <= prevR.right && mousePt.y >= prevR.top && mousePt.y <= prevR.bottom);
        bool nextHov = (mousePt.x >= nextR.left && mousePt.x <= nextR.right && mousePt.y >= nextR.top && mousePt.y <= nextR.bottom);

        D2D1_RECT_F boxRect = D2D1::RectF(sideX, curY, sideX + sideW, curY + boxH);
        m_brushChrome->SetColor(D2D1::ColorF(0x222226, 0.8f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(boxRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(boxRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);

        m_brushText->SetColor(prevHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(L"◀", 1, m_formatKeycap.Get(), prevR, m_brushText.Get());

        m_brushText->SetColor(nextHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(L"▶", 1, m_formatKeycap.Get(), nextR, m_brushText.Get());

        wchar_t szFrame[64];
        swprintf_s(szFrame, L"Frame %zu of %zu", settings.selectedFrameIdx + 1, settings.totalFrameCount);
        m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
        m_d2dContext->DrawText(szFrame, static_cast<UINT32>(wcslen(szFrame)), m_formatKeycap.Get(), textR, m_brushText.Get());

        curY += boxH + 8.0f * m_dpiScale;
    }

    // Row 3: Quality Control
    DrawSectionLabel(L"COMPRESSION & QUALITY");
    float qBoxH = 26.0f * m_dpiScale;
    if (fmtInfo.supportsQuality) {
        float btnW = 26.0f * m_dpiScale;
        float inputW = 48.0f * m_dpiScale;
        float gap = 5.0f * m_dpiScale;
        float trackW = sideW - (btnW * 2.0f + inputW + gap * 3.0f);

        D2D1_RECT_F minusR = D2D1::RectF(sideX, curY, sideX + btnW, curY + qBoxH);
        D2D1_RECT_F trackR = D2D1::RectF(minusR.right + gap, curY, minusR.right + gap + trackW, curY + qBoxH);
        D2D1_RECT_F inputR = D2D1::RectF(trackR.right + gap, curY, trackR.right + gap + inputW, curY + qBoxH);
        D2D1_RECT_F plusR  = D2D1::RectF(inputR.right + gap, curY, inputR.right + gap + btnW, curY + qBoxH);

        m_exportSliderRect = trackR;
        m_exportHits.push_back({ ExportUiId::QualityMinus, -1, minusR });
        m_exportHits.push_back({ ExportUiId::QualitySliderTrack, -1, trackR });
        m_exportHits.push_back({ ExportUiId::QualityInputBox, -1, inputR });
        m_exportHits.push_back({ ExportUiId::QualityPlus, -1, plusR });

        bool mHov = (mousePt.x >= minusR.left && mousePt.x <= minusR.right && mousePt.y >= minusR.top && mousePt.y <= minusR.bottom);
        bool pHov = (mousePt.x >= plusR.left && mousePt.x <= plusR.right && mousePt.y >= plusR.top && mousePt.y <= plusR.bottom);
        bool iHov = (mousePt.x >= inputR.left && mousePt.x <= inputR.right && mousePt.y >= inputR.top && mousePt.y <= inputR.bottom);

        // Minus button
        m_brushChrome->SetColor(mHov ? D2D1::ColorF(0x334155, 0.9f) : D2D1::ColorF(0x27272a, 0.8f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(minusR, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(minusR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
        m_brushText->SetColor(mHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(L"−", 1, m_formatKeycap.Get(), minusR, m_brushText.Get());

        // Slider Track & Thumb
        float barH = 6.0f * m_dpiScale;
        float barY = curY + (qBoxH - barH) / 2.0f;
        D2D1_RECT_F barBg = D2D1::RectF(trackR.left + 4.0f * m_dpiScale, barY, trackR.right - 4.0f * m_dpiScale, barY + barH);
        m_brushChrome->SetColor(D2D1::ColorF(0x27272a, 1.0f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(barBg, 3.0f, 3.0f), m_brushChrome.Get());

        float t = static_cast<float>(std::clamp(settings.quality, 1, 100) - 1) / 99.0f;
        float fillRight = barBg.left + t * (barBg.right - barBg.left);
        D2D1_RECT_F barFill = D2D1::RectF(barBg.left, barY, fillRight, barY + barH);
        m_brushAccent->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(barFill, 3.0f, 3.0f), m_brushAccent.Get());

        float thumbR = 6.5f * m_dpiScale;
        D2D1_ELLIPSE thumb = D2D1::Ellipse(D2D1::Point2F(fillRight, curY + qBoxH / 2.0f), thumbR, thumbR);
        m_brushText->SetColor(D2D1::ColorF(0xf8fafc, 1.0f));
        m_d2dContext->FillEllipse(thumb, m_brushText.Get());
        m_brushAccent->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
        m_d2dContext->DrawEllipse(thumb, m_brushAccent.Get(), 1.5f);

        // Quality Input Box
        if (settings.activeEditField == ExportEditField::Quality) {
            m_brushChrome->SetColor(D2D1::ColorF(0x0f172a, 1.0f));
            m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushChrome.Get());
            m_brushBorder->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
            m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushBorder.Get(), 1.5f);
            std::wstring qStr = settings.editBuffer.empty() ? L"|" : (settings.editBuffer + L"|");
            m_brushText->SetColor(D2D1::ColorF(0x38bdf8));
            m_d2dContext->DrawText(qStr.c_str(), static_cast<UINT32>(qStr.length()), m_formatKeycap.Get(), inputR, m_brushText.Get());
        } else {
            m_brushChrome->SetColor(iHov ? D2D1::ColorF(0x27272a, 0.9f) : D2D1::ColorF(0x222226, 0.8f));
            m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushChrome.Get());
            m_brushBorder->SetColor(iHov ? D2D1::ColorF(0x475569, 0.9f) : D2D1::ColorF(0x3f3f46, 0.8f));
            m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(inputR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
            wchar_t qBuf[16];
            swprintf_s(qBuf, L"%d%%", settings.quality);
            m_brushText->SetColor(D2D1::ColorF(0xf1f5f9));
            m_d2dContext->DrawText(qBuf, static_cast<UINT32>(wcslen(qBuf)), m_formatKeycap.Get(), inputR, m_brushText.Get());
        }

        // Plus button
        m_brushChrome->SetColor(pHov ? D2D1::ColorF(0x334155, 0.9f) : D2D1::ColorF(0x27272a, 0.8f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(plusR, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(plusR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
        m_brushText->SetColor(pHov ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(L"+", 1, m_formatKeycap.Get(), plusR, m_brushText.Get());
    } else {
        D2D1_RECT_F badgeR = D2D1::RectF(sideX, curY, sideX + sideW, curY + qBoxH);
        m_brushChrome->SetColor(D2D1::ColorF(0x1e293b, 0.55f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(badgeR, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x334155, 0.7f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(badgeR, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
        const wchar_t* qInfo = (settings.format == ExportFormat::Gif)
            ? L"256-Color Quantized Palette (Dithered)"
            : (settings.format == ExportFormat::Tiff ? L"Lossless LZW Compression (100%)" : L"Lossless Pixel Encoding (100%)");
        m_brushTextMuted->SetColor(D2D1::ColorF(0x94a3b8));
        m_d2dContext->DrawText(qInfo, static_cast<UINT32>(wcslen(qInfo)), m_formatKeycap.Get(), badgeR, m_brushTextMuted.Get());
    }
    curY += qBoxH + 8.0f * m_dpiScale;

    // Row 4: Resolution Scale Presets
    DrawSectionLabel(L"RESOLUTION SCALE");
    DrawPills({
        { L"100%",   ExportUiId::Scale100 },
        { L"75%",    ExportUiId::Scale75 },
        { L"50%",    ExportUiId::Scale50 },
        { L"25%",    ExportUiId::Scale25 },
        { L"Custom", ExportUiId::ScaleCustom }
    }, static_cast<int>(settings.scalePreset));

    // Row 5: Dimensions (Width x Height + Aspect Lock)
    DrawSectionLabel(L"DIMENSIONS (PX)");
    float dimH = 26.0f * m_dpiScale;
    float lockW = 28.0f * m_dpiScale;
    float dimGap = 5.0f * m_dpiScale;
    float boxW = (sideW - lockW - dimGap * 2.0f) / 2.0f;

    D2D1_RECT_F wRect    = D2D1::RectF(sideX, curY, sideX + boxW, curY + dimH);
    D2D1_RECT_F lockRect = D2D1::RectF(wRect.right + dimGap, curY, wRect.right + dimGap + lockW, curY + dimH);
    D2D1_RECT_F hRect    = D2D1::RectF(lockRect.right + dimGap, curY, lockRect.right + dimGap + boxW, curY + dimH);

    m_exportHits.push_back({ ExportUiId::WidthInputBox, -1, wRect });
    m_exportHits.push_back({ ExportUiId::AspectLockBtn, -1, lockRect });
    m_exportHits.push_back({ ExportUiId::HeightInputBox, -1, hRect });

    auto DrawDimInput = [&](D2D1_RECT_F r, const wchar_t* prefix, UINT val, ExportEditField field) {
        bool isEd = (settings.activeEditField == field);
        bool isHov = (mousePt.x >= r.left && mousePt.x <= r.right && mousePt.y >= r.top && mousePt.y <= r.bottom);
        if (isEd) {
            m_brushChrome->SetColor(D2D1::ColorF(0x0f172a, 1.0f));
            m_brushBorder->SetColor(D2D1::ColorF(0x0284c7, 1.0f));
        } else {
            m_brushChrome->SetColor(isHov ? D2D1::ColorF(0x27272a, 0.9f) : D2D1::ColorF(0x222226, 0.8f));
            m_brushBorder->SetColor(isHov ? D2D1::ColorF(0x475569, 0.9f) : D2D1::ColorF(0x3f3f46, 0.8f));
        }
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(r, 4.0f, 4.0f), m_brushChrome.Get());
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(r, 4.0f, 4.0f), m_brushBorder.Get(), isEd ? 1.5f : 1.0f);

        std::wstring text = isEd
            ? (std::wstring(prefix) + (settings.editBuffer.empty() ? L"|" : (settings.editBuffer + L"|")))
            : (std::wstring(prefix) + std::to_wstring(val));
        m_brushText->SetColor(isEd ? D2D1::ColorF(0x38bdf8) : D2D1::ColorF(0xf1f5f9));
        m_d2dContext->DrawText(text.c_str(), static_cast<UINT32>(text.length()), m_formatKeycap.Get(), r, m_brushText.Get());
    };

    DrawDimInput(wRect, L"W: ", settings.targetWidth, ExportEditField::Width);
    DrawDimInput(hRect, L"H: ", settings.targetHeight, ExportEditField::Height);

    bool lockHov = (mousePt.x >= lockRect.left && mousePt.x <= lockRect.right && mousePt.y >= lockRect.top && mousePt.y <= lockRect.bottom);
    if (settings.lockAspect) {
        m_brushChrome->SetColor(D2D1::ColorF(0x0284c7, 0.85f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(lockRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushText->SetColor(D2D1::ColorF(0xffffff));
    } else {
        m_brushChrome->SetColor(lockHov ? D2D1::ColorF(0x334155, 0.8f) : D2D1::ColorF(0x1e293b, 0.6f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(lockRect, 4.0f, 4.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(lockRect, 4.0f, 4.0f), m_brushBorder.Get(), 1.0f);
        m_brushText->SetColor(D2D1::ColorF(0x94a3b8));
    }
    const wchar_t* lockIcon = settings.lockAspect ? L"🔒" : L"🔓";
    m_d2dContext->DrawText(lockIcon, static_cast<UINT32>(wcslen(lockIcon)), m_formatKeycap.Get(), lockRect, m_brushText.Get());
    curY += dimH + 8.0f * m_dpiScale;

    // Row 6: Background / Alpha Matte
    DrawSectionLabel(L"TRANSPARENCY / MATTE");
    int activeMatteIdx = static_cast<int>(effectiveMatte);
    int disabledMatteIdx = fmtInfo.supportsAlpha ? -1 : 0; // Disable 'Keep Alpha' for JPEG / GIF
    DrawPills({
        { L"Keep Alpha", ExportUiId::MatteKeepAlpha },
        { L"White #FFF", ExportUiId::MatteWhite },
        { L"Black #000", ExportUiId::MatteBlack }
    }, activeMatteIdx, disabledMatteIdx);

    // Row 7: Orientation Transforms
    DrawSectionLabel(L"VIEW TRANSFORMS");
    DrawPills({
        { L"Bake Rot/Flip", ExportUiId::TransformBake },
        { L"Original",      ExportUiId::TransformOriginal }
    }, settings.bakeTransforms ? 0 : 1);

    // Row 8: Bottom Action Buttons
    float actionY = cardY + cardH - 46.0f * m_dpiScale;
    float btnH = 32.0f * m_dpiScale;
    float cancelW = 96.0f * m_dpiScale;

    D2D1_RECT_F cancelRect = D2D1::RectF(sideX, actionY, sideX + cancelW, actionY + btnH);
    D2D1_RECT_F exportRect = D2D1::RectF(cancelRect.right + 6.0f * m_dpiScale, actionY, sideX + sideW, actionY + btnH);

    m_exportHits.push_back({ ExportUiId::CancelBtn, -1, cancelRect });
    m_exportHits.push_back({ ExportUiId::ExecuteExport, -1, exportRect });

    bool cancelHov = (mousePt.x >= cancelRect.left && mousePt.x <= cancelRect.right && mousePt.y >= cancelRect.top && mousePt.y <= cancelRect.bottom);
    m_brushChrome->SetColor(cancelHov ? D2D1::ColorF(0x334155, 0.9f) : D2D1::ColorF(0x27272a, 0.8f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(cancelRect, 6.0f, 6.0f), m_brushChrome.Get());
    m_brushBorder->SetColor(D2D1::ColorF(0x3f3f46, 0.8f));
    m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(cancelRect, 6.0f, 6.0f), m_brushBorder.Get(), 1.0f);
    m_brushText->SetColor(cancelHov ? D2D1::ColorF(0xffffff) : D2D1::ColorF(0x94a3b8));
    const wchar_t cancelTxt[] = L"Cancel";
    m_d2dContext->DrawText(cancelTxt, static_cast<UINT32>(wcslen(cancelTxt)), m_formatKeycap.Get(), cancelRect, m_brushText.Get());

    bool exportHov = (mousePt.x >= exportRect.left && mousePt.x <= exportRect.right && mousePt.y >= exportRect.top && mousePt.y <= exportRect.bottom);
    m_brushChrome->SetColor(exportHov ? D2D1::ColorF(0x0284c7, 1.0f) : D2D1::ColorF(0x0369a1, 0.95f));
    m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(exportRect, 6.0f, 6.0f), m_brushChrome.Get());
    m_brushText->SetColor(D2D1::ColorF(0xffffff));
    const wchar_t exportTxt[] = L"💾 Export As... (Enter)";
    m_d2dContext->DrawText(exportTxt, static_cast<UINT32>(wcslen(exportTxt)), m_formatKeycap.Get(), exportRect, m_brushText.Get());

    // 7. Floating Format Dropdown Popover (Topmost Z-Index)
    if (settings.isFormatDropdownOpen) {
        float itemH = 26.0f * m_dpiScale;
        float listH = itemH * formats.size() + 8.0f * m_dpiScale;
        float listY = fmtBoxRect.bottom + 2.0f;
        D2D1_RECT_F listRect = D2D1::RectF(sideX, listY, sideX + sideW, listY + listH);

        m_brushBg->SetColor(D2D1::ColorF(0x000000, 0.65f));
        m_d2dContext->FillRectangle(D2D1::RectF(sideX + 3.0f, listY + 3.0f, sideX + sideW + 3.0f, listY + listH + 3.0f), m_brushBg.Get());

        m_brushChrome->SetColor(D2D1::ColorF(0x18181b, 0.98f));
        m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(listRect, 6.0f, 6.0f), m_brushChrome.Get());
        m_brushBorder->SetColor(D2D1::ColorF(0x475569, 0.95f));
        m_d2dContext->DrawRoundedRectangle(D2D1::RoundedRect(listRect, 6.0f, 6.0f), m_brushBorder.Get(), 1.2f);

        for (size_t i = 0; i < formats.size(); ++i) {
            float iy = listY + 4.0f * m_dpiScale + i * itemH;
            D2D1_RECT_F itemR = D2D1::RectF(sideX + 4.0f * m_dpiScale, iy, sideX + sideW - 4.0f * m_dpiScale, iy + itemH);
            m_exportHits.push_back({ ExportUiId::FormatDropdownItem, static_cast<int>(i), itemR });

            bool isHov = (mousePt.x >= itemR.left && mousePt.x <= itemR.right && mousePt.y >= itemR.top && mousePt.y <= itemR.bottom);
            bool isSel = (static_cast<int>(i) == fmtIdx);

            if (isHov) {
                m_brushChrome->SetColor(D2D1::ColorF(0x334155, 0.95f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(itemR, 4.0f, 4.0f), m_brushChrome.Get());
            } else if (isSel) {
                m_brushChrome->SetColor(D2D1::ColorF(0x1e293b, 0.7f));
                m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(itemR, 4.0f, 4.0f), m_brushChrome.Get());
            }

            D2D1_RECT_F textR = D2D1::RectF(itemR.left + 22.0f * m_dpiScale, itemR.top, itemR.right - 8.0f * m_dpiScale, itemR.bottom);
            D2D1_RECT_F checkR = D2D1::RectF(itemR.left + 4.0f * m_dpiScale, itemR.top, itemR.left + 20.0f * m_dpiScale, itemR.bottom);

            if (isSel) {
                m_brushAccent->SetColor(D2D1::ColorF(0x38bdf8));
                m_d2dContext->DrawText(L"✓", 1, m_formatKeycap.Get(), checkR, m_brushAccent.Get());
            }

            m_brushText->SetColor(isHov ? D2D1::ColorF(0xffffff) : (isSel ? D2D1::ColorF(0xf1f5f9) : D2D1::ColorF(0x94a3b8)));
            m_d2dContext->DrawText(formats[i].displayName, static_cast<UINT32>(wcslen(formats[i].displayName)), m_formatKeyDesc.Get(), textR, m_brushText.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
    }
}
