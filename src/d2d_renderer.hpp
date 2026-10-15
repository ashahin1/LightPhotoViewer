#pragma once

#include "pch.hpp"
#include "tab_manager.hpp"

enum class HudButtonId {
    None,
    Prev,
    Next,
    ZoomOut,
    Zoom100,
    ZoomIn,
    ZoomFit,
    RotateCCW,
    RotateCW,
    Info,
    Delete
};

struct HudButtonInfo {
    HudButtonId id;
    std::wstring label;
    D2D1_RECT_F rect;
    bool isHovered;
};

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    bool Initialize(HWND hWnd);
    void Resize(UINT width, UINT height);
    void Render(TabManager& tabManager, float hudAlpha, bool showExif, POINT mousePt);

    // Hit testing
    int HitTestTab(POINT pt, const TabManager& tabManager, bool& outCloseButton);
    HudButtonId HitTestHud(POINT pt);

    float GetDpiScale() const { return m_dpiScale; }
    void UpdateDpi(float dpi);

    float GetTabStripHeight() const { return 36.0f * m_dpiScale; }

private:
    HWND m_hWnd = nullptr;
    float m_dpiScale = 1.0f;
    UINT m_width = 0;
    UINT m_height = 0;

    // Direct3D & DXGI
    ComPtr<ID3D11Device> m_d3dDevice;
    ComPtr<ID3D11DeviceContext> m_d3dContext;
    ComPtr<IDXGISwapChain1> m_swapChain;

    // Direct2D
    ComPtr<ID2D1Factory1> m_d2dFactory;
    ComPtr<ID2D1Device> m_d2dDevice;
    ComPtr<ID2D1DeviceContext> m_d2dContext;
    ComPtr<ID2D1Bitmap1> m_d2dTargetBitmap;

    // DirectWrite
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<IDWriteTextFormat> m_formatTab;
    ComPtr<IDWriteTextFormat> m_formatHud;
    ComPtr<IDWriteTextFormat> m_formatInfo;
    ComPtr<IDWriteTextFormat> m_formatTitle;
    ComPtr<IDWriteTextFormat> m_formatSubtitle;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> m_brushBg;
    ComPtr<ID2D1SolidColorBrush> m_brushChrome;
    ComPtr<ID2D1SolidColorBrush> m_brushAccent;
    ComPtr<ID2D1SolidColorBrush> m_brushText;
    ComPtr<ID2D1SolidColorBrush> m_brushTextMuted;
    ComPtr<ID2D1SolidColorBrush> m_brushBorder;

    // Cached HUD button rects
    std::vector<HudButtonInfo> m_hudButtons;
    D2D1_RECT_F m_hudRect = { 0, 0, 0, 0 };

    bool CreateDeviceIndependentResources();
    bool CreateDeviceResources();
    void DiscardDeviceResources();
    void RecreateTargetBitmap();

    void RenderTabs(TabManager& tabManager, POINT mousePt);
    void RenderImageCanvas(TabItem* tab, float viewportTop, float viewportHeight);
    void RenderHud(float hudAlpha, POINT mousePt);
    void RenderExifOverlay(TabItem* tab);
    void RenderEmptyState();
};
