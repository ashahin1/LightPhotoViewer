#pragma once

#include "pch.hpp"
#include "d2d_renderer.hpp"
#include "tab_manager.hpp"

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    bool Create(int nCmdShow, const std::vector<std::wstring>& initialFiles);
    HWND GetHwnd() const { return m_hWnd; }

    static void RegisterShellAssociations();
    static void UnregisterShellAssociations();
    static void InitializeDarkMode();
    static bool IsSystemDarkMode();
    static HMENU BuildTabContextMenu(const TabManager& tabManager, size_t targetTabIndex);

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hWnd = nullptr;
    D2DRenderer m_renderer;
    TabManager m_tabManager;

    // Interaction & Animation State
    POINT m_mousePt = { 0, 0 };
    POINT m_lastPanPt = { 0, 0 };
    bool m_isPanning = false;
    bool m_showExif = false;
    bool m_showHelp = false;
    bool m_isFullscreen = false;
    bool m_isSlideshow = false;
    ULONGLONG m_lastSlideshowTick = 0;
    WINDOWPLACEMENT m_savedPlacement = { sizeof(WINDOWPLACEMENT) };

    // HUD auto-fade
    float m_hudAlpha = 1.0f;
    ULONGLONG m_lastMouseMoveTick = 0;

    // Print Preview & Spooler
    PrintSettings m_printSettings;

    // Image Export & Conversion Modal
    ExportSettings m_exportSettings;

    // Recent Documents Modal
    RecentModalState m_recentModalState;

    void Invalidate();
    void UpdateWindowTitle();
    void ToggleFullscreen();
    void ToggleHelp();
    void ToggleSlideshow();
    void CycleBackdrop();
    void ReopenClosedTab();
    void OpenFileDialog();
    void DeleteCurrentFile();
    void ShowContextMenu(int x, int y);
    void ShowTabContextMenu(size_t targetTabIndex, int x, int y);
    void CopyTextToClipboard(const std::wstring& text);
    void HandleHudAction(HudButtonId id);

    // System Theme Management
    void UpdateTheme();

    void OpenPrintPreview();
    void ClosePrintPreview();
    void HandlePrintUiClick(PrintUiId id);
    void ExecutePrintJob();
    void OpenSystemPrintDialog();
    void SpoolPageToHdc(HDC hdc);

    void OpenExportModal();
    void CloseExportModal();
    void UpdateExportEstimate();
    void HandleExportUiClick(ExportUiId id);
    void ExecuteExportSaveDialog();

    void OpenRecentModal();
    void CloseRecentModal();
    void ToggleRecentModal();

    void NextImage();
    void PrevImage();
    void RotateCW();
    void RotateCCW();
    void FlipHorizontal();
    void FlipVertical();
    void ZoomIn();
    void ZoomOut();
    void ZoomFit();
    void Zoom100();
};
