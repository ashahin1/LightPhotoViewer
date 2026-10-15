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
    bool m_isFullscreen = false;
    WINDOWPLACEMENT m_savedPlacement = { sizeof(WINDOWPLACEMENT) };

    // HUD auto-fade
    float m_hudAlpha = 1.0f;
    ULONGLONG m_lastMouseMoveTick = 0;

    void Invalidate();
    void UpdateWindowTitle();
    void ToggleFullscreen();
    void OpenFileDialog();
    void DeleteCurrentFile();
    void ShowContextMenu(int x, int y);
    void HandleHudAction(HudButtonId id);

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
