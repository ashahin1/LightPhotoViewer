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
    Export,
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

enum class PrintDropdownState {
    None,
    Printer,
    PaperSize
};

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

    // Multi-Frame / GIF Animation State
    bool isAnimated = false;
    size_t selectedFrameIdx = 0;
    size_t totalFrameCount = 1;

    // Dropdown & Input State
    PrintDropdownState openDropdown = PrintDropdownState::None;
    bool isEditingCopies = false;
    std::wstring copiesEditBuffer;

    void CommitCopiesEdit() {
        if (isEditingCopies) {
            if (!copiesEditBuffer.empty()) {
                int val = _wtoi(copiesEditBuffer.c_str());
                copies = std::clamp(val, 1, 99);
            }
            isEditingCopies = false;
            copiesEditBuffer.clear();
        }
    }

    void CancelCopiesEdit() {
        isEditingCopies = false;
        copiesEditBuffer.clear();
    }
};

enum class PrintUiId {
    None,
    CloseModal,
    CardBody,
    PrinterDropdownBtn,
    PaperDropdownBtn,
    DropdownItem,
    FramePrev,
    FrameNext,
    OrientPortrait,
    OrientLandscape,
    ScaleFit,
    ScaleFill,
    ScaleActual,
    MarginNormal,
    MarginNarrow,
    MarginNone,
    CopiesMinus,
    CopiesInputBox,
    CopiesPlus,
    SystemDialog,
    ExecutePrint
};

struct PrintHitBox {
    PrintUiId id;
    int itemIndex = -1;
    D2D1_RECT_F rect;
};

enum class ExportScalePreset : int {
    Scale100 = 0,
    Scale75  = 1,
    Scale50  = 2,
    Scale25  = 3,
    Custom   = 4
};

enum class ExportEditField : int {
    None    = 0,
    Quality = 1,
    Width   = 2,
    Height  = 3
};

struct ExportSettings {
    bool isActive = false;
    ExportFormat format = ExportFormat::Jpeg;
    int quality = 92; // 1..100

    ExportScalePreset scalePreset = ExportScalePreset::Scale100;
    UINT baseWidth = 1;
    UINT baseHeight = 1;
    UINT targetWidth = 1;
    UINT targetHeight = 1;
    bool lockAspect = true;

    bool bakeTransforms = true;
    ExportMatte matte = ExportMatte::KeepAlpha;

    // Multi-frame / GIF support
    bool isAnimated = false;
    size_t selectedFrameIdx = 0;
    size_t totalFrameCount = 1;

    // UI interactive states
    bool isFormatDropdownOpen = false;
    bool isDraggingQualitySlider = false;
    ExportEditField activeEditField = ExportEditField::None;
    std::wstring editBuffer;

    // Live encoded buffer & exact byte size
    std::vector<BYTE> encodedBytes;
    bool encodeValid = false;

    void ApplyScalePreset(ExportScalePreset preset) {
        scalePreset = preset;
        float factor = 1.0f;
        if (preset == ExportScalePreset::Scale75) factor = 0.75f;
        else if (preset == ExportScalePreset::Scale50) factor = 0.50f;
        else if (preset == ExportScalePreset::Scale25) factor = 0.25f;
        if (preset != ExportScalePreset::Custom) {
            targetWidth  = std::max(1U, static_cast<UINT>(baseWidth * factor + 0.5f));
            targetHeight = std::max(1U, static_cast<UINT>(baseHeight * factor + 0.5f));
        }
    }

    void CommitEditField() {
        if (activeEditField == ExportEditField::None) return;
        if (!editBuffer.empty()) {
            int val = _wtoi(editBuffer.c_str());
            if (activeEditField == ExportEditField::Quality) {
                quality = std::clamp(val, 1, 100);
            } else if (activeEditField == ExportEditField::Width) {
                targetWidth = static_cast<UINT>(std::clamp(val, 1, 8192));
                scalePreset = ExportScalePreset::Custom;
                if (lockAspect && baseWidth > 0) {
                    targetHeight = std::max(1U, static_cast<UINT>((static_cast<double>(targetWidth) * baseHeight) / baseWidth + 0.5));
                }
            } else if (activeEditField == ExportEditField::Height) {
                targetHeight = static_cast<UINT>(std::clamp(val, 1, 8192));
                scalePreset = ExportScalePreset::Custom;
                if (lockAspect && baseHeight > 0) {
                    targetWidth = std::max(1U, static_cast<UINT>((static_cast<double>(targetHeight) * baseWidth) / baseHeight + 0.5));
                }
            }
        }
        activeEditField = ExportEditField::None;
        editBuffer.clear();
    }

    void CancelEditField() {
        activeEditField = ExportEditField::None;
        editBuffer.clear();
    }
};

enum class ExportUiId {
    None,
    CloseModal,
    CardBody,
    FormatDropdownBtn,
    FormatDropdownItem,
    FramePrev,
    FrameNext,
    QualityMinus,
    QualitySliderTrack,
    QualityInputBox,
    QualityPlus,
    Scale100,
    Scale75,
    Scale50,
    Scale25,
    ScaleCustom,
    WidthInputBox,
    AspectLockBtn,
    HeightInputBox,
    TransformBake,
    TransformOriginal,
    MatteKeepAlpha,
    MatteWhite,
    MatteBlack,
    CancelBtn,
    ExecuteExport
};

struct ExportHitBox {
    ExportUiId id;
    int itemIndex = -1;
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
    void Render(TabManager& tabManager, float hudAlpha, bool showExif, bool showHelp, bool isSlideshow, const PrintSettings& printSettings, const ExportSettings& exportSettings, POINT mousePt);

    // Hit testing
    int HitTestTab(POINT pt, const TabManager& tabManager, bool& outCloseButton);
    bool HitTestTabAdd(POINT pt);
    HudButtonId HitTestHud(POINT pt);
    bool HitTestHelpClose(POINT pt);
    bool HitTestHelpCard(POINT pt);
    int HitTestRecentFile(POINT pt, std::wstring& outPath);
    PrintUiId HitTestPrintUi(POINT pt);
    int GetHitDropdownIndex() const { return m_hitDropdownIndex; }
    ExportUiId HitTestExportUi(POINT pt);
    int GetHitExportDropdownIndex() const { return m_hitExportDropdownIndex; }
    D2D1_RECT_F GetExportQualitySliderRect() const { return m_exportSliderRect; }

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
    D2D1_RECT_F m_dropdownListRect = { 0, 0, 0, 0 };
    int m_hitDropdownIndex = -1;

    std::vector<ExportHitBox> m_exportHits;
    D2D1_RECT_F m_exportCardRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_exportCloseRect = { 0, 0, 0, 0 };
    D2D1_RECT_F m_exportSliderRect = { 0, 0, 0, 0 };
    int m_hitExportDropdownIndex = -1;

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
    void RenderExportModal(TabItem* activeTab, const ExportSettings& settings, POINT mousePt);
    void RenderEmptyState(const TabManager& tabManager, POINT mousePt);
    void RenderSlideshowBadge();
    void DrawCheckerboard(D2D1_RECT_F rect);
};
