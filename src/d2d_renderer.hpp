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

enum class CanvasBackground {
    Dark,
    Checkerboard,
    Light
};

enum class PrintOrientation { Portrait, Landscape };
enum class PrintScaleMode   { FitToPage, FillPage, ActualSize };
enum class PrintMarginMode  { Normal, Narrow, Borderless };

struct PaperPreset {
    short dmPaperCode;
    const wchar_t* name;
    float widthMm;
    float heightMm;
};

struct PrintSettings {
    bool isActive = false;
    std::vector<std::wstring> printers;
    int selectedPrinterIdx = 0;
    int paperPresetIdx = 0; // 0: A4, 1: Letter, 2: Legal, 3: A3, 4: A5, 5: 4x6", 6: 5x7", 7: 8x10"
    PrintOrientation orientation = PrintOrientation::Portrait;
    PrintScaleMode scaleMode = PrintScaleMode::FitToPage;
    PrintMarginMode marginMode = PrintMarginMode::Normal;
    int copies = 1;
};

enum class PrintUiId {
    None,
    CloseModal,
    CardBody,
    PrinterPrev,
    PrinterNext,
    PaperPrev,
    PaperNext,
    OrientPortrait,
    OrientLandscape,
    ScaleFit,
    ScaleFill,
    ScaleActual,
    MarginNormal,
    MarginNarrow,
    MarginNone,
    CopiesMinus,
    CopiesPlus,
    SystemDialog,
    ExecutePrint
};

struct PrintHitBox {
    PrintUiId id;
    D2D1_RECT_F rect;
};

struct HudButtonInfo {
    HudButtonId id;
    std::wstring label;
    D2D1_RECT_F rect;
    bool isHovered;
};

struct RecentFileHitBox {
    int index;
    std::wstring path;
    D2D1_RECT_F rect;
    bool isHovered;
};

class D2DRenderer {
public:
    D2DRenderer();
    ~D2DRenderer();

    bool Initialize(HWND hWnd);
    void Resize(UINT width, UINT height);
    void Render(TabManager& tabManager, float hudAlpha, bool showExif, bool showHelp, bool isSlideshow, const PrintSettings& printSettings, POINT mousePt);

    // Hit testing
    int HitTestTab(POINT pt, const TabManager& tabManager, bool& outCloseButton);
    bool HitTestTabAdd(POINT pt);
    HudButtonId HitTestHud(POINT pt);
    bool HitTestHelpClose(POINT pt);
    bool HitTestHelpCard(POINT pt);
    int HitTestRecentFile(POINT pt, std::wstring& outPath);
    PrintUiId HitTestPrintUi(POINT pt);

    float GetDpiScale() const { return m_dpiScale; }
    void UpdateDpi(float dpi);

    float GetTabStripHeight() const { return 36.0f * m_dpiScale; }

    void CycleBackgroundMode();
    CanvasBackground GetBackgroundMode() const { return m_bgMode; }

    static const std::vector<PaperPreset>& GetPaperPresets();
    bool RasterizeSvgToPixels(ID2D1SvgDocument* svgDoc, UINT targetW, UINT targetH, std::vector<uint32_t>& outPixels);

private:
    HWND m_hWnd = nullptr;
    float m_dpiScale = 1.0f;
    UINT m_width = 0;
    UINT m_height = 0;
    CanvasBackground m_bgMode = CanvasBackground::Dark;

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
    ComPtr<IDWriteTextFormat> m_formatHelpCat;
    ComPtr<IDWriteTextFormat> m_formatKeycap;
    ComPtr<IDWriteTextFormat> m_formatKeyDesc;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> m_brushBg;
    ComPtr<ID2D1SolidColorBrush> m_brushChrome;
    ComPtr<ID2D1SolidColorBrush> m_brushAccent;
    ComPtr<ID2D1SolidColorBrush> m_brushText;
    ComPtr<ID2D1SolidColorBrush> m_brushTextMuted;
    ComPtr<ID2D1SolidColorBrush> m_brushBorder;

    // Cached UI Element Rects
    std::vector<HudButtonInfo> m_hudButtons;
    D2D1_RECT_F m_hudRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_helpCardRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_helpCloseRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_tabAddRect = { 0, 0, 0, 0 };
    std::vector<RecentFileHitBox> m_recentHits;
    std::vector<PrintHitBox> m_printHits;
    D2D1_RECT_F m_printCardRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_printCloseRect = { 0, 0, 0, 0 };

    bool CreateDeviceIndependentResources();
    bool CreateDeviceResources();
    void DiscardDeviceResources();
    void RecreateTargetBitmap();

    void RenderTabs(TabManager& tabManager, POINT mousePt);
    void RenderImageCanvas(TabItem* tab, float viewportTop, float viewportHeight);
    void RenderHud(TabItem* activeTab, float hudAlpha, POINT mousePt);
    void RenderExifOverlay(TabItem* tab);
    void RenderHelpOverlay(POINT mousePt);
    void RenderPrintPreview(TabItem* activeTab, const PrintSettings& settings, POINT mousePt);
    void RenderEmptyState(const TabManager& tabManager, POINT mousePt);
    void RenderSlideshowBadge();
    void DrawCheckerboard(D2D1_RECT_F rect);
};
