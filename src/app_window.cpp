#include "pch.hpp"
#include "app_window.hpp"
#include "../resources/resource_ids.hpp"

static const wchar_t* WND_CLASS_NAME = L"LightPhotoViewer_WndClass";
static const UINT_PTR TIMER_ANIM_AND_HUD = 1;

namespace {

enum class PreferredAppMode {
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3,
    Max = 4
};

using fnSetPreferredAppMode = PreferredAppMode (WINAPI *)(PreferredAppMode appMode);
using fnAllowDarkModeForWindow = BOOL (WINAPI *)(HWND hWnd, BOOL allow);
using fnFlushMenuThemes = void (WINAPI *)();
using fnRefreshImmersiveColorPolicyState = void (WINAPI *)();

static fnSetPreferredAppMode s_fnSetPreferredAppMode = nullptr;
static fnAllowDarkModeForWindow s_fnAllowDarkModeForWindow = nullptr;
static fnFlushMenuThemes s_fnFlushMenuThemes = nullptr;
static fnRefreshImmersiveColorPolicyState s_fnRefreshImmersiveColorPolicyState = nullptr;

} // namespace

void AppWindow::InitializeDarkMode() {
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if (!hUxTheme) {
        hUxTheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (hUxTheme) {
        s_fnSetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
        s_fnAllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133)));
        s_fnFlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));
        s_fnRefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(GetProcAddress(hUxTheme, MAKEINTRESOURCEA(104)));

        if (s_fnSetPreferredAppMode) {
            s_fnSetPreferredAppMode(PreferredAppMode::AllowDark);
        }
        if (s_fnFlushMenuThemes) {
            s_fnFlushMenuThemes();
        }
    }
}

bool AppWindow::IsSystemDarkMode() {
    DWORD data = 0;
    DWORD size = sizeof(data);
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&data), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (data == 0); // 0 = Dark Mode, 1 = Light Mode
        }
        RegCloseKey(hKey);
    }
    return true; // Default to dark mode for photo viewer
}

void AppWindow::UpdateTheme() {
    if (!m_hWnd) return;

    BOOL isDark = IsSystemDarkMode();

    // DWMWA_USE_IMMERSIVE_DARK_MODE is 20 in Windows 10 2004+ and Windows 11
    BOOL val = isDark ? TRUE : FALSE;
    DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &val, sizeof(val));
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19;
    DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &val, sizeof(val));

    if (s_fnAllowDarkModeForWindow) {
        s_fnAllowDarkModeForWindow(m_hWnd, isDark);
    }

    if (s_fnFlushMenuThemes) {
        s_fnFlushMenuThemes();
    }
}

AppWindow::AppWindow() {
    m_lastMouseMoveTick = GetTickCount64();
}

AppWindow::~AppWindow() {
}

LRESULT CALLBACK AppWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = static_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<AppWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool AppWindow::Create(int nCmdShow, const std::vector<std::wstring>& initialFiles) {
    InitializeDarkMode();

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr; // D2D paints background
    wc.lpszClassName = WND_CLASS_NAME;

    RegisterClassExW(&wc);

    // Initial window size
    int initialW = 1200;
    int initialH = 800;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = std::max(50, (screenW - initialW) / 2);
    int posY = std::max(50, (screenH - initialH) / 2);

    m_hWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        WND_CLASS_NAME,
        L"LightPhotoViewer",
        WS_OVERLAPPEDWINDOW,
        posX, posY, initialW, initialH,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    if (!m_hWnd) return false;

    // Synchronize title bar and menus with system dark/light theme
    UpdateTheme();

    // Initialize Direct2D Renderer
    if (!m_renderer.Initialize(m_hWnd)) {
        return false;
    }

    // Open initial files passed on CLI
    for (const auto& file : initialFiles) {
        m_tabManager.OpenTab(file);
    }

    // Start 60fps timer for GIF frame animation and smooth HUD fading
    SetTimer(m_hWnd, TIMER_ANIM_AND_HUD, 16, nullptr);

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
    UpdateWindowTitle();

    return true;
}

void AppWindow::Invalidate() {
    if (m_hWnd) {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void AppWindow::UpdateWindowTitle() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab && tab->document && tab->document->isValid) {
        const auto& meta = tab->document->metadata;

        std::wstring title;
        if (tab->navigator && tab->navigator->HasFiles()) {
            title += L"[" + std::to_wstring(tab->navigator->GetCurrentIndex() + 1) + L"/" +
                     std::to_wstring(tab->navigator->GetTotalFiles()) + L"] ";
        }

        title += meta.fileName + L" (" + std::to_wstring(meta.width) + L"×" + std::to_wstring(meta.height) + L")";

        float currentZoom = tab->zoom;
        if (tab->isFitToWindow && m_hWnd) {
            RECT rc;
            if (GetClientRect(m_hWnd, &rc)) {
                float tabStripHeight = m_tabManager.ShouldShowTabs() ? m_renderer.GetTabStripHeight() : 0.0f;
                float vpW = static_cast<float>(rc.right - rc.left);
                float vpH = static_cast<float>(rc.bottom - rc.top) - tabStripHeight;
                bool isRotated90 = (tab->rotationDegrees == 90 || tab->rotationDegrees == 270);
                float visualW = isRotated90 ? static_cast<float>(meta.height) : static_cast<float>(meta.width);
                float visualH = isRotated90 ? static_cast<float>(meta.width) : static_cast<float>(meta.height);
                if (visualW > 0.0f && visualH > 0.0f && vpW > 0.0f && vpH > 0.0f) {
                    float pad = 24.0f * m_renderer.GetDpiScale();
                    float availW = std::max(10.0f, vpW - pad);
                    float availH = std::max(10.0f, vpH - pad);
                    currentZoom = std::min(availW / visualW, availH / visualH);
                    tab->zoom = currentZoom;
                }
            }
        }

        int zoomPercent = static_cast<int>(currentZoom * 100.0f + 0.5f);
        title += L" - " + std::to_wstring(zoomPercent) + L"%";

        if (tab->rotationDegrees != 0) {
            title += L" [" + std::to_wstring(tab->rotationDegrees) + L"°]";
        }

        title += L" - LightPhotoViewer";
        SetWindowTextW(m_hWnd, title.c_str());
    } else {
        SetWindowTextW(m_hWnd, L"LightPhotoViewer");
    }
}

void AppWindow::ToggleFullscreen() {
    DWORD style = GetWindowLong(m_hWnd, GWL_STYLE);

    if (!m_isFullscreen) {
        m_savedPlacement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(m_hWnd, &m_savedPlacement);

        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(m_hWnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(
                m_hWnd,
                HWND_TOP,
                mi.rcMonitor.left,
                mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED
            );
            m_isFullscreen = true;
        }
    } else {
        SetWindowLong(m_hWnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(m_hWnd, &m_savedPlacement);
        SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        m_isFullscreen = false;
    }

    Invalidate();
}

void AppWindow::OpenFileDialog() {
    wchar_t filePath[MAX_PATH] = L"";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter =
        L"All Supported Images (*.jpg;*.png;*.webp;*.gif;*.bmp;*.tiff;*.avif;*.svg)\0*.jpg;*.jpeg;*.jpe;*.png;*.webp;*.gif;*.bmp;*.dib;*.tiff;*.tif;*.avif;*.svg;*.heic\0"
        L"JPEG Images (*.jpg;*.jpeg)\0*.jpg;*.jpeg;*.jpe\0"
        L"PNG Images (*.png)\0*.png\0"
        L"WebP Images (*.webp)\0*.webp\0"
        L"GIF Images (*.gif)\0*.gif\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ENABLESIZING;

    if (GetOpenFileNameW(&ofn)) {
        m_tabManager.OpenTab(filePath);
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::DeleteCurrentFile() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (!tab || !tab->navigator) return;

    std::wstring path = tab->navigator->GetCurrentFilePath();
    if (path.empty()) return;

    std::wstring confirmMsg = L"Are you sure you want to send this image to the Recycle Bin?\n\n" + path;
    if (MessageBoxW(m_hWnd, confirmMsg.c_str(), L"Delete Image", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    // Windows Shell Delete to Recycle Bin
    wchar_t doubleNullPath[MAX_PATH + 2] = {};
    wcsncpy_s(doubleNullPath, path.c_str(), MAX_PATH);

    SHFILEOPSTRUCTW sfo = {};
    sfo.hwnd = m_hWnd;
    sfo.wFunc = FO_DELETE;
    sfo.pFrom = doubleNullPath;
    sfo.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

    if (SHFileOperationW(&sfo) == 0 && !sfo.fAnyOperationsAborted) {
        tab->navigator->RefreshFolder();
        tab->document = tab->navigator->GetCurrentDocument();
        tab->ClearGpuBitmaps();
        tab->ResetTransforms();
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::ShowContextMenu(int x, int y) {
    if (s_fnFlushMenuThemes) {
        s_fnFlushMenuThemes();
    }

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    TabItem* tab = m_tabManager.GetActiveTab();
    bool hasDoc = (tab && tab->document && tab->document->isValid);

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"Open Image...\tCtrl+O");
    AppendMenuW(hMenu, MF_STRING, IDM_FILE_REOPEN_TAB, L"Reopen Closed Tab\tCtrl+Shift+T");
    if (hasDoc) {
        AppendMenuW(hMenu, MF_STRING, IDM_FILE_PRINT, L"Print...\tCtrl+P");
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    if (hasDoc) {
        AppendMenuW(hMenu, MF_STRING, IDM_NAV_NEXT, L"Next Image\tRight / Space");
        AppendMenuW(hMenu, MF_STRING, IDM_NAV_PREV, L"Previous Image\tLeft / Backspace");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_SLIDESHOW, m_isSlideshow ? L"Stop Slideshow\tF5 / S" : L"Start Slideshow\tF5 / S");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_ZOOM_FIT, L"Fit to Window\t0");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_ZOOM_100, L"100% Actual Size\t1");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_ZOOM_IN, L"Zoom In\t+");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_ZOOM_OUT, L"Zoom Out\t-");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_ROTATE_CW, L"Rotate 90° Clockwise\tR");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_ROTATE_CCW, L"Rotate 90° Counter-Clockwise\tL");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_FLIP_H, L"Flip Horizontal\tH");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_FLIP_V, L"Flip Vertical\tV");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_BACKDROP, L"Cycle Backdrop (Dark / Grid / Light)\tB");
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_INFO, L"Toggle EXIF Info\tI");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_COPY, L"Copy Image\tCtrl+C");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_COPY_PATH, L"Copy File Path\tCtrl+Shift+C");
        AppendMenuW(hMenu, MF_STRING, IDM_FILE_REVEAL, L"Reveal in File Explorer");
        AppendMenuW(hMenu, MF_STRING, IDM_FILE_DELETE, L"Delete to Recycle Bin\tDel");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    } else {
        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_BACKDROP, L"Cycle Backdrop (Dark / Grid / Light)\tB");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    AppendMenuW(hMenu, MF_STRING, IDM_VIEW_FULLSCREEN, L"Fullscreen\tF / F11");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_HELP_SHORTCUTS, L"Keyboard Shortcuts...\tF1");

    if (m_tabManager.GetTabCount() > 0) {
        AppendMenuW(hMenu, MF_STRING, IDM_FILE_CLOSE_TAB, L"Close Tab\tCtrl+W");
    }

    TrackPopupMenuEx(hMenu, TPM_RIGHTBUTTON, x, y, m_hWnd, nullptr);
    DestroyMenu(hMenu);
}

void AppWindow::ToggleHelp() {
    m_showHelp = !m_showHelp;
    Invalidate();
}

void AppWindow::ToggleSlideshow() {
    m_isSlideshow = !m_isSlideshow;
    if (m_isSlideshow) {
        m_lastSlideshowTick = GetTickCount64();
    }
    Invalidate();
}

void AppWindow::CycleBackdrop() {
    m_renderer.CycleBackgroundMode();
    Invalidate();
}

void AppWindow::ReopenClosedTab() {
    if (m_tabManager.ReopenClosedTab()) {
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::NextImage() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab && tab->navigator) {
        auto nextDoc = tab->navigator->NavigateNext();
        if (nextDoc) {
            tab->document = nextDoc;
            tab->title = tab->document->metadata.fileName;
            tab->ClearGpuBitmaps();
            tab->ResetTransforms();
            UpdateWindowTitle();
            Invalidate();
        }
    }
}

void AppWindow::PrevImage() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab && tab->navigator) {
        auto prevDoc = tab->navigator->NavigatePrev();
        if (prevDoc) {
            tab->document = prevDoc;
            tab->title = tab->document->metadata.fileName;
            tab->ClearGpuBitmaps();
            tab->ResetTransforms();
            UpdateWindowTitle();
            Invalidate();
        }
    }
}

void AppWindow::RotateCW() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->rotationDegrees = (tab->rotationDegrees + 90) % 360;
        tab->isFitToWindow = true; // Auto-fit to new orientation
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::RotateCCW() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->rotationDegrees = (tab->rotationDegrees + 270) % 360;
        tab->isFitToWindow = true; // Auto-fit to new orientation
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::FlipHorizontal() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->flipH = !tab->flipH;
        Invalidate();
    }
}

void AppWindow::FlipVertical() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->flipV = !tab->flipV;
        Invalidate();
    }
}

void AppWindow::ZoomIn() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->zoom = std::min(50.0f, tab->zoom * 1.25f);
        tab->isFitToWindow = false;
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::ZoomOut() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->zoom = std::max(0.02f, tab->zoom * 0.8f);
        tab->isFitToWindow = false;
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::ZoomFit() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->isFitToWindow = true;
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::Zoom100() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        tab->zoom = 1.0f;
        tab->isFitToWindow = false;
        RECT rc;
        GetClientRect(m_hWnd, &rc);
        float tabStripH = m_tabManager.ShouldShowTabs() ? m_renderer.GetTabStripHeight() : 0.0f;
        tab->panX = (rc.right - rc.left) / 2.0f;
        tab->panY = tabStripH + (rc.bottom - rc.top - tabStripH) / 2.0f;
        UpdateWindowTitle();
        Invalidate();
    }
}

void AppWindow::HandleHudAction(HudButtonId id) {
    switch (id) {
        case HudButtonId::Prev:      PrevImage(); break;
        case HudButtonId::Next:      NextImage(); break;
        case HudButtonId::ZoomOut:   ZoomOut(); break;
        case HudButtonId::Zoom100:   Zoom100(); break;
        case HudButtonId::ZoomIn:    ZoomIn(); break;
        case HudButtonId::ZoomFit: {
            TabItem* tab = m_tabManager.GetActiveTab();
            if (tab) {
                if (tab->isFitToWindow) {
                    Zoom100();
                } else {
                    ZoomFit();
                }
            }
            break;
        }
        case HudButtonId::RotateCCW: RotateCCW(); break;
        case HudButtonId::RotateCW:  RotateCW(); break;
        case HudButtonId::Info:      m_showExif = !m_showExif; Invalidate(); break;
        case HudButtonId::Delete:    DeleteCurrentFile(); break;
        default: break;
    }
}

LRESULT AppWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);
            m_renderer.Resize(w, h);
            UpdateWindowTitle();
            Invalidate();
            return 0;
        }

        case WM_DPICHANGED: {
            UINT newDpi = HIWORD(wParam);
            m_renderer.UpdateDpi(static_cast<float>(newDpi));
            RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(
                m_hWnd,
                nullptr,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
            Invalidate();
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(m_hWnd, &ps);
            m_renderer.Render(m_tabManager, m_hudAlpha, m_showExif, m_showHelp, m_isSlideshow, m_printSettings, m_mousePt);
            EndPaint(m_hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent GDI flicker

        case WM_MOUSEMOVE: {
            m_mousePt.x = GET_X_LPARAM(lParam);
            m_mousePt.y = GET_Y_LPARAM(lParam);

            // Reset HUD auto-fade timer on mouse movement
            m_lastMouseMoveTick = GetTickCount64();
            m_hudAlpha = 1.0f;

            if (m_isPanning) {
                TabItem* tab = m_tabManager.GetActiveTab();
                if (tab) {
                    float dx = static_cast<float>(m_mousePt.x - m_lastPanPt.x);
                    float dy = static_cast<float>(m_mousePt.y - m_lastPanPt.y);
                    tab->panX += dx;
                    tab->panY += dy;
                    tab->isFitToWindow = false;
                    m_lastPanPt = m_mousePt;
                    Invalidate();
                }
            } else {
                Invalidate();
            }

            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE: {
            m_mousePt = { -1000, -1000 };
            Invalidate();
            return 0;
        }

        case WM_LBUTTONDOWN: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

            // If Print Preview is active, handle overlay controls or backdrop click
            if (m_printSettings.isActive) {
                PrintUiId printId = m_renderer.HitTestPrintUi(pt);

                // 1. Check if clicking inside an active dropdown list
                if (m_printSettings.openDropdown != PrintDropdownState::None) {
                    if (printId == PrintUiId::DropdownItem) {
                        int idx = m_renderer.GetHitDropdownIndex();
                        if (m_printSettings.openDropdown == PrintDropdownState::Printer) {
                            if (idx >= 0 && idx < static_cast<int>(m_printSettings.printers.size())) {
                                m_printSettings.selectedPrinterIdx = idx;
                            }
                        } else if (m_printSettings.openDropdown == PrintDropdownState::PaperSize) {
                            if (idx >= 0 && idx < static_cast<int>(D2DRenderer::GetPaperPresets().size())) {
                                m_printSettings.paperPresetIdx = idx;
                            }
                        }
                        m_printSettings.openDropdown = PrintDropdownState::None;
                        Invalidate();
                        return 0;
                    } else {
                        // Clicked outside dropdown list items: close dropdown
                        PrintDropdownState prevOpen = m_printSettings.openDropdown;
                        m_printSettings.openDropdown = PrintDropdownState::None;
                        // If they clicked the toggle button of the already-open dropdown, just close it
                        if ((prevOpen == PrintDropdownState::Printer && printId == PrintUiId::PrinterDropdownBtn) ||
                            (prevOpen == PrintDropdownState::PaperSize && printId == PrintUiId::PaperDropdownBtn)) {
                            Invalidate();
                            return 0;
                        }
                    }
                }

                // 2. Commit Copies edit if clicking anywhere other than the copies box
                if (m_printSettings.isEditingCopies && printId != PrintUiId::CopiesInputBox) {
                    m_printSettings.CommitCopiesEdit();
                }

                // 3. Dispatch control actions
                switch (printId) {
                    case PrintUiId::CloseModal:
                        ClosePrintPreview();
                        break;
                    case PrintUiId::CardBody:
                        Invalidate();
                        break;
                    case PrintUiId::PrinterDropdownBtn:
                        m_printSettings.openDropdown = (m_printSettings.openDropdown == PrintDropdownState::Printer)
                            ? PrintDropdownState::None
                            : PrintDropdownState::Printer;
                        Invalidate();
                        break;
                    case PrintUiId::PaperDropdownBtn:
                        m_printSettings.openDropdown = (m_printSettings.openDropdown == PrintDropdownState::PaperSize)
                            ? PrintDropdownState::None
                            : PrintDropdownState::PaperSize;
                        Invalidate();
                        break;
                    case PrintUiId::CopiesInputBox:
                        m_printSettings.isEditingCopies = true;
                        m_printSettings.copiesEditBuffer = std::to_wstring(m_printSettings.copies);
                        m_printSettings.openDropdown = PrintDropdownState::None;
                        Invalidate();
                        break;
                    case PrintUiId::CopiesMinus:
                        m_printSettings.CommitCopiesEdit();
                        if (m_printSettings.copies > 1) {
                            m_printSettings.copies--;
                            Invalidate();
                        }
                        break;
                    case PrintUiId::CopiesPlus:
                        m_printSettings.CommitCopiesEdit();
                        if (m_printSettings.copies < 99) {
                            m_printSettings.copies++;
                            Invalidate();
                        }
                        break;
                    case PrintUiId::None:
                        ClosePrintPreview();
                        break;
                    default:
                        HandlePrintUiClick(printId);
                        break;
                }
                return 0;
            }

            // If Help Overlay is active, handle close button or backdrop click
            if (m_showHelp) {
                if (m_renderer.HitTestHelpClose(pt) || !m_renderer.HitTestHelpCard(pt)) {
                    m_showHelp = false;
                    Invalidate();
                }
                return 0;
            }

            // Check if clicked Recent File on empty state
            if (m_tabManager.GetTabCount() == 0) {
                std::wstring recentPath;
                if (m_renderer.HitTestRecentFile(pt, recentPath) >= 0) {
                    m_tabManager.OpenTab(recentPath);
                    UpdateWindowTitle();
                    Invalidate();
                    return 0;
                }
            }

            // Check if clicked Tab Strip '+' add button
            if (m_renderer.HitTestTabAdd(pt)) {
                OpenFileDialog();
                return 0;
            }

            // Check if clicked Tab Strip
            bool isCloseBtn = false;
            int hitTab = m_renderer.HitTestTab(pt, m_tabManager, isCloseBtn);
            if (hitTab >= 0) {
                if (isCloseBtn) {
                    m_tabManager.CloseTab(hitTab);
                    UpdateWindowTitle();
                    Invalidate();
                } else {
                    m_tabManager.SelectTab(hitTab);
                    UpdateWindowTitle();
                    Invalidate();
                }
                return 0;
            }

            // Check if clicked HUD button
            HudButtonId hudBtn = m_renderer.HitTestHud(pt);
            if (hudBtn != HudButtonId::None) {
                HandleHudAction(hudBtn);
                return 0;
            }

            // Otherwise, start dragging/panning
            m_isPanning = true;
            m_lastPanPt = pt;
            SetCapture(m_hWnd);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_isPanning) {
                m_isPanning = false;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (pt.y > m_renderer.GetTabStripHeight()) {
                TabItem* tab = m_tabManager.GetActiveTab();
                if (tab) {
                    if (tab->isFitToWindow) {
                        Zoom100();
                    } else {
                        ZoomFit();
                    }
                }
            }
            return 0;
        }

        case WM_MBUTTONUP: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            bool isClose = false;
            int hitTab = m_renderer.HitTestTab(pt, m_tabManager, isClose);
            if (hitTab >= 0) {
                m_tabManager.CloseTab(hitTab);
                UpdateWindowTitle();
                Invalidate();
            } else {
                ZoomFit();
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            POINT screenPt = pt;
            ClientToScreen(m_hWnd, &screenPt);
            ShowContextMenu(screenPt.x, screenPt.y);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(m_hWnd, &pt);

            TabItem* tab = m_tabManager.GetActiveTab();
            if (tab) {
                float oldZoom = tab->zoom;
                float factor = (delta > 0) ? 1.15f : (1.0f / 1.15f);
                float newZoom = std::clamp(oldZoom * factor, 0.02f, 50.0f);

                // Cursor-centered zoom preservation
                tab->panX = pt.x - ((pt.x - tab->panX) / oldZoom) * newZoom;
                tab->panY = pt.y - ((pt.y - tab->panY) / oldZoom) * newZoom;
                tab->zoom = newZoom;
                tab->isFitToWindow = false;

                UpdateWindowTitle();
                Invalidate();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // 1. Strict Modal Isolation: Print Preview
            if (m_printSettings.isActive) {
                // If a dropdown list is open, handle navigation or closing
                if (m_printSettings.openDropdown != PrintDropdownState::None) {
                    if (wParam == VK_ESCAPE) {
                        m_printSettings.openDropdown = PrintDropdownState::None;
                        Invalidate();
                        return 0;
                    }
                    if (wParam == VK_UP) {
                        if (m_printSettings.openDropdown == PrintDropdownState::Printer && !m_printSettings.printers.empty()) {
                            int count = static_cast<int>(m_printSettings.printers.size());
                            m_printSettings.selectedPrinterIdx = (m_printSettings.selectedPrinterIdx - 1 + count) % count;
                            Invalidate();
                        } else if (m_printSettings.openDropdown == PrintDropdownState::PaperSize) {
                            int count = static_cast<int>(D2DRenderer::GetPaperPresets().size());
                            m_printSettings.paperPresetIdx = (m_printSettings.paperPresetIdx - 1 + count) % count;
                            Invalidate();
                        }
                        return 0;
                    }
                    if (wParam == VK_DOWN) {
                        if (m_printSettings.openDropdown == PrintDropdownState::Printer && !m_printSettings.printers.empty()) {
                            int count = static_cast<int>(m_printSettings.printers.size());
                            m_printSettings.selectedPrinterIdx = (m_printSettings.selectedPrinterIdx + 1) % count;
                            Invalidate();
                        } else if (m_printSettings.openDropdown == PrintDropdownState::PaperSize) {
                            int count = static_cast<int>(D2DRenderer::GetPaperPresets().size());
                            m_printSettings.paperPresetIdx = (m_printSettings.paperPresetIdx + 1) % count;
                            Invalidate();
                        }
                        return 0;
                    }
                    if (wParam == VK_RETURN) {
                        m_printSettings.openDropdown = PrintDropdownState::None;
                        Invalidate();
                        return 0;
                    }
                    return 0;
                }

                // If editing Copies count via keyboard
                if (m_printSettings.isEditingCopies) {
                    if (wParam == VK_RETURN) {
                        m_printSettings.CommitCopiesEdit();
                        Invalidate();
                        return 0;
                    }
                    if (wParam == VK_ESCAPE) {
                        m_printSettings.CancelCopiesEdit();
                        Invalidate();
                        return 0;
                    }
                    if (wParam == VK_BACK) {
                        if (!m_printSettings.copiesEditBuffer.empty()) {
                            m_printSettings.copiesEditBuffer.pop_back();
                            int val = m_printSettings.copiesEditBuffer.empty() ? 1 : _wtoi(m_printSettings.copiesEditBuffer.c_str());
                            m_printSettings.copies = std::clamp(val, 1, 99);
                            Invalidate();
                        }
                        return 0;
                    }
                    if ((wParam >= '0' && wParam <= '9') || (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)) {
                        wchar_t digit = (wParam >= '0' && wParam <= '9')
                            ? static_cast<wchar_t>(wParam)
                            : static_cast<wchar_t>('0' + (wParam - VK_NUMPAD0));
                        if (m_printSettings.copiesEditBuffer.length() < 2) {
                            m_printSettings.copiesEditBuffer += digit;
                            int val = _wtoi(m_printSettings.copiesEditBuffer.c_str());
                            m_printSettings.copies = std::clamp(val, 1, 99);
                            Invalidate();
                        }
                        return 0;
                    }
                    return 0;
                }

                // Modal actions
                if (wParam == VK_ESCAPE) {
                    ClosePrintPreview();
                    return 0;
                }
                if (wParam == VK_RETURN || (ctrlDown && wParam == 'P')) {
                    ExecutePrintJob();
                    return 0;
                }

                // Animation frame navigation with [ and ]
                if (m_printSettings.isAnimated && m_printSettings.totalFrameCount > 1) {
                    if (wParam == VK_OEM_4) { // '['
                        if (m_printSettings.selectedFrameIdx == 0) {
                            m_printSettings.selectedFrameIdx = m_printSettings.totalFrameCount - 1;
                        } else {
                            m_printSettings.selectedFrameIdx--;
                        }
                        Invalidate();
                        return 0;
                    }
                    if (wParam == VK_OEM_6) { // ']'
                        m_printSettings.selectedFrameIdx = (m_printSettings.selectedFrameIdx + 1) % m_printSettings.totalFrameCount;
                        Invalidate();
                        return 0;
                    }
                }

                // STRICTLY SUPPRESS all other keys (arrows, Space, Delete, Home, End, R, L, etc.)
                return 0;
            }

            // 2. Strict Modal Isolation: Help Overlay
            if (m_showHelp) {
                if (wParam == VK_ESCAPE || wParam == VK_F1) {
                    ToggleHelp();
                }
                return 0;
            }

            if (ctrlDown) {
                switch (wParam) {
                    case 'O': OpenFileDialog(); return 0;
                    case 'P': {
                        if (m_printSettings.isActive) ExecutePrintJob();
                        else OpenPrintPreview();
                        return 0;
                    }
                    case 'W': {
                        if (m_tabManager.GetActiveIndex() >= 0) {
                            m_tabManager.CloseTab(m_tabManager.GetActiveIndex());
                            UpdateWindowTitle();
                            Invalidate();
                        }
                        return 0;
                    }
                    case 'T': {
                        if (shiftDown) {
                            ReopenClosedTab();
                            return 0;
                        }
                        break;
                    }
                    case 'C': {
                        TabItem* tab = m_tabManager.GetActiveTab();
                        if (tab && tab->document) {
                            const FrameData* fd = tab->document->GetFrame(tab->currentFrame);
                            if (fd) {
                                if (shiftDown) {
                                    // Copy file path to clipboard
                                    std::wstring path = tab->document->metadata.filePath;
                                    if (OpenClipboard(m_hWnd)) {
                                        EmptyClipboard();
                                        size_t bytes = (path.length() + 1) * sizeof(wchar_t);
                                        HGLOBAL hMem = GlobalAlloc(GHND, bytes);
                                        if (hMem) {
                                            memcpy(GlobalLock(hMem), path.c_str(), bytes);
                                            GlobalUnlock(hMem);
                                            SetClipboardData(CF_UNICODETEXT, hMem);
                                        }
                                        CloseClipboard();
                                    }
                                } else {
                                    ImageEngine::Instance().CopyToClipboard(m_hWnd, *fd);
                                }
                            }
                        }
                        return 0;
                    }
                    case VK_TAB: {
                        if (shiftDown) m_tabManager.PrevTab();
                        else m_tabManager.NextTab();
                        UpdateWindowTitle();
                        Invalidate();
                        return 0;
                    }
                }
            }

            switch (wParam) {
                case VK_RIGHT:
                case VK_SPACE:
                    NextImage();
                    if (m_isSlideshow) m_lastSlideshowTick = GetTickCount64();
                    return 0;

                case VK_LEFT:
                case VK_BACK:
                    PrevImage();
                    if (m_isSlideshow) m_lastSlideshowTick = GetTickCount64();
                    return 0;

                case VK_HOME: {
                    TabItem* tab = m_tabManager.GetActiveTab();
                    if (tab && tab->navigator) {
                        tab->document = tab->navigator->NavigateFirst();
                        tab->title = tab->document->metadata.fileName;
                        tab->ClearGpuBitmaps();
                        tab->ResetTransforms();
                        UpdateWindowTitle();
                        Invalidate();
                    }
                    return 0;
                }

                case VK_END: {
                    TabItem* tab = m_tabManager.GetActiveTab();
                    if (tab && tab->navigator) {
                        tab->document = tab->navigator->NavigateLast();
                        tab->title = tab->document->metadata.fileName;
                        tab->ClearGpuBitmaps();
                        tab->ResetTransforms();
                        UpdateWindowTitle();
                        Invalidate();
                    }
                    return 0;
                }

                case '0':
                case VK_NUMPAD0: {
                    TabItem* tab = m_tabManager.GetActiveTab();
                    if (tab) {
                        if (tab->isFitToWindow) {
                            Zoom100();
                        } else {
                            ZoomFit();
                        }
                    }
                    return 0;
                }

                case '1': {
                    if (m_tabManager.GetTabCount() == 0) {
                        const auto& recents = m_tabManager.GetRecentFiles();
                        if (!recents.empty()) {
                            m_tabManager.OpenTab(recents[0]);
                            UpdateWindowTitle();
                            Invalidate();
                            return 0;
                        }
                    } else {
                        Zoom100();
                        return 0;
                    }
                    break;
                }

                case VK_NUMPAD1:
                    Zoom100();
                    return 0;

                case '2':
                case '3':
                case '4':
                case '5':
                case '6': {
                    if (m_tabManager.GetTabCount() == 0) {
                        size_t idx = static_cast<size_t>(wParam - '1');
                        const auto& recents = m_tabManager.GetRecentFiles();
                        if (idx < recents.size()) {
                            m_tabManager.OpenTab(recents[idx]);
                            UpdateWindowTitle();
                            Invalidate();
                            return 0;
                        }
                    }
                    break;
                }

                case VK_OEM_PLUS:
                case VK_ADD:
                    ZoomIn();
                    return 0;

                case VK_OEM_MINUS:
                case VK_SUBTRACT:
                    ZoomOut();
                    return 0;

                case 'F':
                case VK_F11:
                    ToggleFullscreen();
                    return 0;

                case VK_RETURN:
                    if (m_printSettings.isActive) {
                        ExecutePrintJob();
                        return 0;
                    }
                    ToggleFullscreen();
                    return 0;

                case 'R':
                    RotateCW();
                    return 0;

                case 'L':
                    RotateCCW();
                    return 0;

                case 'H':
                    FlipHorizontal();
                    return 0;

                case 'V':
                    FlipVertical();
                    return 0;

                case 'B':
                    CycleBackdrop();
                    return 0;

                case 'S':
                case VK_F5:
                    ToggleSlideshow();
                    return 0;

                case 'I':
                    m_showExif = !m_showExif;
                    Invalidate();
                    return 0;

                case VK_DELETE:
                    DeleteCurrentFile();
                    return 0;

                case VK_F1:
                    ToggleHelp();
                    return 0;

                case VK_ESCAPE:
                    if (m_printSettings.isActive) {
                        ClosePrintPreview();
                        return 0;
                    }
                    if (m_showHelp) {
                        m_showHelp = false;
                        Invalidate();
                    } else if (m_isSlideshow) {
                        m_isSlideshow = false;
                        Invalidate();
                    } else if (m_isFullscreen) {
                        ToggleFullscreen();
                    } else if (m_showExif) {
                        m_showExif = false;
                        Invalidate();
                    }
                    return 0;
            }
            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDM_FILE_OPEN:       OpenFileDialog(); return 0;
                case IDM_FILE_REOPEN_TAB: ReopenClosedTab(); return 0;
                case IDM_FILE_PRINT:      OpenPrintPreview(); return 0;
                case IDM_NAV_NEXT:        NextImage(); return 0;
                case IDM_NAV_PREV:        PrevImage(); return 0;
                case IDM_VIEW_ZOOM_FIT:   ZoomFit(); return 0;
                case IDM_VIEW_ZOOM_100:   Zoom100(); return 0;
                case IDM_VIEW_ZOOM_IN:    ZoomIn(); return 0;
                case IDM_VIEW_ZOOM_OUT:   ZoomOut(); return 0;
                case IDM_VIEW_ROTATE_CW:  RotateCW(); return 0;
                case IDM_VIEW_ROTATE_CCW: RotateCCW(); return 0;
                case IDM_VIEW_FLIP_H:     FlipHorizontal(); return 0;
                case IDM_VIEW_FLIP_V:     FlipVertical(); return 0;
                case IDM_VIEW_BACKDROP:   CycleBackdrop(); return 0;
                case IDM_VIEW_SLIDESHOW:  ToggleSlideshow(); return 0;
                case IDM_VIEW_INFO:       m_showExif = !m_showExif; Invalidate(); return 0;
                case IDM_VIEW_FULLSCREEN: ToggleFullscreen(); return 0;
                case IDM_HELP_SHORTCUTS:  ToggleHelp(); return 0;
                case IDM_FILE_DELETE:     DeleteCurrentFile(); return 0;
                case IDM_FILE_CLOSE_TAB: {
                    if (m_tabManager.GetActiveIndex() >= 0) {
                        m_tabManager.CloseTab(m_tabManager.GetActiveIndex());
                        UpdateWindowTitle();
                        Invalidate();
                    }
                    return 0;
                }
                case IDM_EDIT_COPY: {
                    TabItem* tab = m_tabManager.GetActiveTab();
                    if (tab && tab->document) {
                        const FrameData* fd = tab->document->GetFrame(tab->currentFrame);
                        if (fd) ImageEngine::Instance().CopyToClipboard(m_hWnd, *fd);
                    }
                    return 0;
                }
                case IDM_EDIT_COPY_PATH: {
                    TabItem* tab = m_tabManager.GetActiveTab();
                    if (tab && tab->document) {
                        std::wstring path = tab->document->metadata.filePath;
                        if (OpenClipboard(m_hWnd)) {
                            EmptyClipboard();
                            size_t bytes = (path.length() + 1) * sizeof(wchar_t);
                            HGLOBAL hMem = GlobalAlloc(GHND, bytes);
                            if (hMem) {
                                memcpy(GlobalLock(hMem), path.c_str(), bytes);
                                GlobalUnlock(hMem);
                                SetClipboardData(CF_UNICODETEXT, hMem);
                            }
                            CloseClipboard();
                        }
                    }
                    return 0;
                }
                case IDM_FILE_REVEAL: {
                    TabItem* tab = m_tabManager.GetActiveTab();
                    if (tab && tab->document) {
                        std::wstring param = L"/select,\"" + tab->document->metadata.filePath + L"\"";
                        ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
                    }
                    return 0;
                }
            }
            break;
        }

        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < count; ++i) {
                wchar_t filePath[MAX_PATH];
                if (DragQueryFileW(hDrop, i, filePath, MAX_PATH) > 0) {
                    m_tabManager.OpenTab(filePath);
                }
            }
            DragFinish(hDrop);
            UpdateWindowTitle();
            Invalidate();
            return 0;
        }

        case WM_COPYDATA: {
            COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
            if (cds && cds->lpData && cds->cbData > 0) {
                const wchar_t* filePath = reinterpret_cast<const wchar_t*>(cds->lpData);
                m_tabManager.OpenTab(filePath);
                UpdateWindowTitle();
                Invalidate();

                if (IsIconic(m_hWnd)) ShowWindow(m_hWnd, SW_RESTORE);
                SetForegroundWindow(m_hWnd);
                return TRUE;
            }
            return FALSE;
        }

        case WM_TIMER: {
            if (wParam == TIMER_ANIM_AND_HUD) {
                bool needsRepaint = false;

                // 1. GIF Frame Animation update
                TabItem* tab = m_tabManager.GetActiveTab();
                if (!m_printSettings.isActive && tab && tab->document && tab->document->metadata.isAnimated) {
                    size_t frameCount = tab->document->GetFrameCount();
                    if (frameCount > 1) {
                        ULONGLONG now = GetTickCount64();
                        const FrameData* fd = tab->document->GetFrame(tab->currentFrame);
                        UINT delay = fd ? fd->delayMs : 100;
                        if (now - tab->lastFrameTick >= delay) {
                            tab->currentFrame = (tab->currentFrame + 1) % frameCount;
                            tab->lastFrameTick = now;
                            needsRepaint = true;
                        }
                    }
                }

                // 2. HUD Auto-Fade Animation
                ULONGLONG elapsedSinceMove = GetTickCount64() - m_lastMouseMoveTick;
                if (elapsedSinceMove > 2000) {
                    if (m_hudAlpha > 0.0f) {
                        m_hudAlpha = std::max(0.0f, m_hudAlpha - 0.05f);
                        needsRepaint = true;
                    }
                } else {
                    if (m_hudAlpha < 1.0f) {
                        m_hudAlpha = std::min(1.0f, m_hudAlpha + 0.1f);
                        needsRepaint = true;
                    }
                }

                // 3. Slideshow Progression (auto-advances every 3 seconds)
                if (m_isSlideshow) {
                    ULONGLONG now = GetTickCount64();
                    if (now - m_lastSlideshowTick >= 3000) {
                        m_lastSlideshowTick = now;
                        NextImage();
                    }
                }

                if (needsRepaint) {
                    Invalidate();
                }
            }
            return 0;
        }

        case WM_SETTINGCHANGE: {
            if (s_fnRefreshImmersiveColorPolicyState) {
                s_fnRefreshImmersiveColorPolicyState();
            }
            UpdateTheme();
            Invalidate();
            return 0;
        }

        case WM_THEMECHANGED: {
            UpdateTheme();
            Invalidate();
            return 0;
        }

        case WM_DESTROY:
            KillTimer(m_hWnd, TIMER_ANIM_AND_HUD);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(m_hWnd, msg, wParam, lParam);
}

void AppWindow::RegisterShellAssociations() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    const wchar_t* extensions[] = {
        L".jpg", L".jpeg", L".png", L".webp", L".gif",
        L".bmp", L".ico", L".tiff", L".tif", L".avif", L".svg"
    };

    const wchar_t* progId = L"LightPhotoViewer.Image";

    HKEY hKeyProgId;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\LightPhotoViewer.Image", 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyProgId, nullptr) == ERROR_SUCCESS) {

        const wchar_t desc[] = L"Image Document (LightPhotoViewer)";
        RegSetValueExW(hKeyProgId, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(desc), (DWORD)(wcslen(desc) + 1) * sizeof(wchar_t));

        // Default Icon
        HKEY hKeyIcon;
        if (RegCreateKeyExW(hKeyProgId, L"DefaultIcon", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyIcon, nullptr) == ERROR_SUCCESS) {
            std::wstring iconVal = std::wstring(exePath) + L",0";
            RegSetValueExW(hKeyIcon, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(iconVal.c_str()), (DWORD)(iconVal.length() + 1) * sizeof(wchar_t));
            RegCloseKey(hKeyIcon);
        }

        // Open command
        HKEY hKeyCmd;
        if (RegCreateKeyExW(hKeyProgId, L"shell\\open\\command", 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyCmd, nullptr) == ERROR_SUCCESS) {
            std::wstring cmdVal = L"\"" + std::wstring(exePath) + L"\" \"%1\"";
            RegSetValueExW(hKeyCmd, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmdVal.c_str()), (DWORD)(cmdVal.length() + 1) * sizeof(wchar_t));
            RegCloseKey(hKeyCmd);
        }

        RegCloseKey(hKeyProgId);
    }

    for (const auto& ext : extensions) {
        std::wstring keyPath = L"Software\\Classes\\" + std::wstring(ext) + L"\\OpenWithProgids";
        HKEY hKeyExt;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKeyExt, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hKeyExt, progId, 0, REG_NONE, nullptr, 0);
            RegCloseKey(hKeyExt);
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void AppWindow::UnregisterShellAssociations() {
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Classes\\LightPhotoViewer.Image");

    const wchar_t* extensions[] = {
        L".jpg", L".jpeg", L".png", L".webp", L".gif",
        L".bmp", L".ico", L".tiff", L".tif", L".avif", L".svg"
    };

    for (const auto& ext : extensions) {
        std::wstring keyPath = L"Software\\Classes\\" + std::wstring(ext) + L"\\OpenWithProgids";
        HKEY hKeyExt;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_WRITE, &hKeyExt) == ERROR_SUCCESS) {
            RegDeleteValueW(hKeyExt, L"LightPhotoViewer.Image");
            RegCloseKey(hKeyExt);
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void AppWindow::OpenPrintPreview() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (!tab || !tab->document || !tab->document->isValid) {
        return;
    }

    // 1. Enumerate available printers
    m_printSettings.printers.clear();
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD bytesNeeded = 0;
    DWORD numPrinters = 0;
    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &bytesNeeded, &numPrinters);
    if (bytesNeeded > 0) {
        std::vector<BYTE> buffer(bytesNeeded);
        if (EnumPrintersW(flags, nullptr, 2, buffer.data(), bytesNeeded, &bytesNeeded, &numPrinters)) {
            PRINTER_INFO_2W* pInfo = reinterpret_cast<PRINTER_INFO_2W*>(buffer.data());
            for (DWORD i = 0; i < numPrinters; ++i) {
                if (pInfo[i].pPrinterName) {
                    m_printSettings.printers.push_back(pInfo[i].pPrinterName);
                }
            }
        }
    }
    if (m_printSettings.printers.empty()) {
        m_printSettings.printers.push_back(L"Microsoft Print to PDF");
    }

    // 2. Select system default printer
    wchar_t defaultPrinter[256] = {};
    DWORD size = 256;
    m_printSettings.selectedPrinterIdx = 0;
    if (GetDefaultPrinterW(defaultPrinter, &size)) {
        for (size_t i = 0; i < m_printSettings.printers.size(); ++i) {
            if (_wcsicmp(m_printSettings.printers[i].c_str(), defaultPrinter) == 0) {
                m_printSettings.selectedPrinterIdx = static_cast<int>(i);
                break;
            }
        }
    }

    // 3. Auto-detect orientation to maximize initial fit
    bool isRot90 = (tab->rotationDegrees == 90 || tab->rotationDegrees == 270);
    float vw = static_cast<float>(isRot90 ? tab->document->GetHeight() : tab->document->GetWidth());
    float vh = static_cast<float>(isRot90 ? tab->document->GetWidth() : tab->document->GetHeight());
    m_printSettings.orientation = (vw > vh) ? PrintOrientation::Landscape : PrintOrientation::Portrait;

    // 4. Initialize animation / multi-frame state
    size_t frameCount = tab->document->GetFrameCount();
    m_printSettings.totalFrameCount = frameCount;
    m_printSettings.isAnimated = tab->document->metadata.isAnimated && (frameCount > 1);
    m_printSettings.selectedFrameIdx = (tab->currentFrame < frameCount) ? tab->currentFrame : 0;

    m_printSettings.openDropdown = PrintDropdownState::None;
    m_printSettings.isEditingCopies = false;
    m_printSettings.copiesEditBuffer.clear();
    m_printSettings.isActive = true;
    m_showHelp = false;
    Invalidate();
}

void AppWindow::ClosePrintPreview() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (tab) {
        if (m_printSettings.isAnimated && m_printSettings.totalFrameCount > 1) {
            tab->currentFrame = m_printSettings.selectedFrameIdx;
        }
        tab->lastFrameTick = GetTickCount64();
    }
    m_printSettings.openDropdown = PrintDropdownState::None;
    m_printSettings.isEditingCopies = false;
    m_printSettings.copiesEditBuffer.clear();
    m_printSettings.isActive = false;
    Invalidate();
}

void AppWindow::HandlePrintUiClick(PrintUiId id) {
    switch (id) {
        case PrintUiId::CloseModal:
            ClosePrintPreview();
            break;
        case PrintUiId::FramePrev:
            if (m_printSettings.isAnimated && m_printSettings.totalFrameCount > 1) {
                if (m_printSettings.selectedFrameIdx == 0) {
                    m_printSettings.selectedFrameIdx = m_printSettings.totalFrameCount - 1;
                } else {
                    m_printSettings.selectedFrameIdx--;
                }
                Invalidate();
            }
            break;
        case PrintUiId::FrameNext:
            if (m_printSettings.isAnimated && m_printSettings.totalFrameCount > 1) {
                m_printSettings.selectedFrameIdx = (m_printSettings.selectedFrameIdx + 1) % m_printSettings.totalFrameCount;
                Invalidate();
            }
            break;
        case PrintUiId::OrientPortrait:
            m_printSettings.orientation = PrintOrientation::Portrait;
            Invalidate();
            break;
        case PrintUiId::OrientLandscape:
            m_printSettings.orientation = PrintOrientation::Landscape;
            Invalidate();
            break;
        case PrintUiId::ScaleFit:
            m_printSettings.scaleMode = PrintScaleMode::FitToPage;
            Invalidate();
            break;
        case PrintUiId::ScaleFill:
            m_printSettings.scaleMode = PrintScaleMode::FillPage;
            Invalidate();
            break;
        case PrintUiId::ScaleActual:
            m_printSettings.scaleMode = PrintScaleMode::ActualSize;
            Invalidate();
            break;
        case PrintUiId::MarginNormal:
            m_printSettings.marginMode = PrintMarginMode::Normal;
            Invalidate();
            break;
        case PrintUiId::MarginNarrow:
            m_printSettings.marginMode = PrintMarginMode::Narrow;
            Invalidate();
            break;
        case PrintUiId::MarginNone:
            m_printSettings.marginMode = PrintMarginMode::Borderless;
            Invalidate();
            break;
        case PrintUiId::SystemDialog:
            OpenSystemPrintDialog();
            break;
        case PrintUiId::ExecutePrint:
            ExecutePrintJob();
            break;
        default:
            break;
    }
}

void AppWindow::OpenSystemPrintDialog() {
    PRINTDLGW pd = { sizeof(PRINTDLGW) };
    pd.hwndOwner = m_hWnd;
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    pd.nCopies = static_cast<WORD>(m_printSettings.copies);

    if (PrintDlgW(&pd)) {
        if (pd.hDC) {
            SpoolPageToHdc(pd.hDC);
            DeleteDC(pd.hDC);
        }
        if (pd.hDevMode) GlobalFree(pd.hDevMode);
        if (pd.hDevNames) GlobalFree(pd.hDevNames);
        ClosePrintPreview();
    }
}

void AppWindow::ExecutePrintJob() {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (!tab || !tab->document || !tab->document->isValid) return;

    if (m_printSettings.printers.empty()) {
        MessageBoxW(m_hWnd, L"No printers available on this system.", L"Print Error", MB_OK | MB_ICONWARNING);
        return;
    }

    const std::wstring& printerName = m_printSettings.printers[m_printSettings.selectedPrinterIdx % m_printSettings.printers.size()];

    HANDLE hPrinter = nullptr;
    if (!OpenPrinterW(const_cast<LPWSTR>(printerName.c_str()), &hPrinter, nullptr)) {
        MessageBoxW(m_hWnd, (L"Unable to open printer:\n" + printerName).c_str(), L"Print Error", MB_OK | MB_ICONERROR);
        return;
    }

    LONG dmSize = DocumentPropertiesW(m_hWnd, hPrinter, const_cast<LPWSTR>(printerName.c_str()), nullptr, nullptr, 0);
    if (dmSize <= 0) {
        ClosePrinter(hPrinter);
        MessageBoxW(m_hWnd, L"Failed to query printer properties.", L"Print Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::vector<BYTE> devModeBuffer(dmSize);
    DEVMODEW* pDevMode = reinterpret_cast<DEVMODEW*>(devModeBuffer.data());
    if (DocumentPropertiesW(m_hWnd, hPrinter, const_cast<LPWSTR>(printerName.c_str()), pDevMode, nullptr, DM_OUT_BUFFER) < 0) {
        ClosePrinter(hPrinter);
        MessageBoxW(m_hWnd, L"Failed to initialize DEVMODE structure.", L"Print Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Configure DEVMODE with our PrintSettings
    pDevMode->dmFields |= DM_ORIENTATION | DM_COPIES;
    pDevMode->dmOrientation = (m_printSettings.orientation == PrintOrientation::Landscape) ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
    pDevMode->dmCopies = static_cast<short>(std::clamp(m_printSettings.copies, 1, 99));

    const auto& presets = D2DRenderer::GetPaperPresets();
    int pIdx = std::clamp(m_printSettings.paperPresetIdx, 0, static_cast<int>(presets.size() - 1));
    if (presets[pIdx].dmPaperCode != 0) {
        pDevMode->dmFields |= DM_PAPERSIZE;
        pDevMode->dmPaperSize = presets[pIdx].dmPaperCode;
    } else {
        pDevMode->dmFields |= DM_PAPERLENGTH | DM_PAPERWIDTH;
        pDevMode->dmPaperWidth = static_cast<short>(presets[pIdx].widthMm * 10.0f);
        pDevMode->dmPaperLength = static_cast<short>(presets[pIdx].heightMm * 10.0f);
    }

    DocumentPropertiesW(m_hWnd, hPrinter, const_cast<LPWSTR>(printerName.c_str()), pDevMode, pDevMode, DM_IN_BUFFER | DM_OUT_BUFFER);
    ClosePrinter(hPrinter);

    HDC hdc = CreateDCW(L"WINSPOOL", printerName.c_str(), nullptr, pDevMode);
    if (!hdc) {
        MessageBoxW(m_hWnd, (L"Failed to create printer device context for:\n" + printerName).c_str(), L"Print Error", MB_OK | MB_ICONERROR);
        return;
    }

    SpoolPageToHdc(hdc);
    DeleteDC(hdc);
    ClosePrintPreview();
}

void AppWindow::SpoolPageToHdc(HDC hdc) {
    TabItem* tab = m_tabManager.GetActiveTab();
    if (!tab || !tab->document || !tab->document->isValid || !hdc) return;

    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    int printW = GetDeviceCaps(hdc, HORZRES);
    int printH = GetDeviceCaps(hdc, VERTRES);
    int physW = GetDeviceCaps(hdc, PHYSICALWIDTH);
    int physH = GetDeviceCaps(hdc, PHYSICALHEIGHT);
    int offX = GetDeviceCaps(hdc, PHYSICALOFFSETX);
    int offY = GetDeviceCaps(hdc, PHYSICALOFFSETY);

    if (printW <= 0 || printH <= 0 || dpiX <= 0 || dpiY <= 0) return;

    float marginIn = 0.5f; // Normal: 0.5 in
    if (m_printSettings.marginMode == PrintMarginMode::Narrow) marginIn = 0.25f;
    else if (m_printSettings.marginMode == PrintMarginMode::Borderless) marginIn = 0.0f;

    int mPixX = static_cast<int>(marginIn * dpiX);
    int mPixY = static_cast<int>(marginIn * dpiY);

    int targetLeft = std::max(0, mPixX - offX);
    int targetTop = std::max(0, mPixY - offY);
    int targetRight = std::min(printW, (physW > 0 ? physW : printW) - mPixX - offX);
    int targetBottom = std::min(printH, (physH > 0 ? physH : printH) - mPixY - offY);

    if (targetRight <= targetLeft || targetBottom <= targetTop) {
        targetLeft = 0; targetTop = 0; targetRight = printW; targetBottom = printH;
    }
    int areaW = targetRight - targetLeft;
    int areaH = targetBottom - targetTop;

    // Prepare pixel data
    UINT srcW = tab->document->GetWidth();
    UINT srcH = tab->document->GetHeight();
    if (srcW == 0 || srcH == 0) return;

    bool isRot90 = (tab->rotationDegrees == 90 || tab->rotationDegrees == 270);
    UINT outW = isRot90 ? srcH : srcW;
    UINT outH = isRot90 ? srcW : srcH;

    std::vector<uint32_t> dibPixels;

    if (tab->document->metadata.isSvg && tab->svgDocument) {
        // High-resolution rasterization of SVG to match print area (capped at 3000x3000 max to bound RAM)
        float maxDim = 3000.0f;
        float aspect = static_cast<float>(srcW) / static_cast<float>(srcH);
        UINT rasterW = (aspect >= 1.0f) ? static_cast<UINT>(maxDim) : static_cast<UINT>(maxDim * aspect);
        UINT rasterH = (aspect >= 1.0f) ? static_cast<UINT>(maxDim / aspect) : static_cast<UINT>(maxDim);
        rasterW = std::clamp(rasterW, 100U, 4000U);
        rasterH = std::clamp(rasterH, 100U, 4000U);

        std::vector<uint32_t> svgPixels;
        if (m_renderer.RasterizeSvgToPixels(tab->svgDocument.Get(), rasterW, rasterH, svgPixels)) {
            srcW = rasterW;
            srcH = rasterH;
            outW = isRot90 ? srcH : srcW;
            outH = isRot90 ? srcW : srcH;
            dibPixels.resize(outW * outH);

            for (UINT y = 0; y < outH; ++y) {
                UINT visualY = outH - 1 - y; // Bottom-up
                for (UINT x = 0; x < outW; ++x) {
                    UINT ox = x;
                    UINT oy = visualY;
                    if (tab->flipH) ox = outW - 1 - ox;
                    if (tab->flipV) oy = outH - 1 - oy;

                    UINT sx = ox;
                    UINT sy = oy;
                    switch (tab->rotationDegrees) {
                        case 90:  sx = oy; sy = srcH - 1 - ox; break;
                        case 180: sx = srcW - 1 - ox; sy = srcH - 1 - oy; break;
                        case 270: sx = srcW - 1 - oy; sy = ox; break;
                        default:  break;
                    }
                    dibPixels[y * outW + x] = svgPixels[sy * srcW + sx];
                }
            }
        }
    }

    if (dibPixels.empty()) {
        size_t frameToPrint = m_printSettings.isAnimated ? m_printSettings.selectedFrameIdx : tab->currentFrame;
        const FrameData* fd = tab->document->GetFrame(frameToPrint);
        if (!fd || !fd->bitmap) return;

        std::vector<BYTE> srcBytes(srcW * srcH * 4);
        WICRect rc = { 0, 0, static_cast<INT>(srcW), static_cast<INT>(srcH) };
        if (FAILED(fd->bitmap->CopyPixels(&rc, srcW * 4, static_cast<UINT>(srcBytes.size()), srcBytes.data()))) {
            return;
        }

        dibPixels.resize(outW * outH);
        for (UINT y = 0; y < outH; ++y) {
            UINT visualY = outH - 1 - y; // Bottom-up
            for (UINT x = 0; x < outW; ++x) {
                UINT ox = x;
                UINT oy = visualY;
                if (tab->flipH) ox = outW - 1 - ox;
                if (tab->flipV) oy = outH - 1 - oy;

                UINT sx = ox;
                UINT sy = oy;
                switch (tab->rotationDegrees) {
                    case 90:  sx = oy; sy = srcH - 1 - ox; break;
                    case 180: sx = srcW - 1 - ox; sy = srcH - 1 - oy; break;
                    case 270: sx = srcW - 1 - oy; sy = ox; break;
                    default:  break;
                }

                const BYTE* p = &srcBytes[(sy * srcW + sx) * 4];
                BYTE b = p[0], g = p[1], r = p[2], a = p[3];
                BYTE invA = 255 - a;
                // Composite premultiplied pixels onto pure white background
                BYTE dstB = b + invA;
                BYTE dstG = g + invA;
                BYTE dstR = r + invA;
                dibPixels[y * outW + x] = (dstR << 16) | (dstG << 8) | dstB;
            }
        }
    }

    // Calculate Destination and Source Cropping
    float visAspect = static_cast<float>(outW) / static_cast<float>(outH);
    float areaAspect = static_cast<float>(areaW) / static_cast<float>(areaH);

    int dstX = targetLeft;
    int dstY = targetTop;
    int dstW = areaW;
    int dstH = areaH;

    int srcCropX = 0;
    int srcCropY = 0;
    int srcCropW = outW;
    int srcCropH = outH;

    if (m_printSettings.scaleMode == PrintScaleMode::FitToPage) {
        if (areaAspect > visAspect) {
            dstH = areaH;
            dstW = static_cast<int>(dstH * visAspect);
        } else {
            dstW = areaW;
            dstH = static_cast<int>(dstW / visAspect);
        }
        dstX = targetLeft + (areaW - dstW) / 2;
        dstY = targetTop + (areaH - dstH) / 2;
    } else if (m_printSettings.scaleMode == PrintScaleMode::FillPage) {
        if (areaAspect > visAspect) {
            srcCropW = outW;
            srcCropH = static_cast<int>(outW / areaAspect);
            srcCropY = (outH - srcCropH) / 2;
        } else {
            srcCropH = outH;
            srcCropW = static_cast<int>(outH * areaAspect);
            srcCropX = (outW - srcCropW) / 2;
        }
        dstX = targetLeft;
        dstY = targetTop;
        dstW = areaW;
        dstH = areaH;
    } else { // ActualSize (1:1 physical size at 96 DPI)
        dstW = static_cast<int>((static_cast<float>(outW) / 96.0f) * dpiX);
        dstH = static_cast<int>((static_cast<float>(outH) / 96.0f) * dpiY);
        dstX = targetLeft + (areaW - dstW) / 2;
        dstY = targetTop + (areaH - dstH) / 2;
    }

    DOCINFOW di = { sizeof(DOCINFOW) };
    std::wstring docTitle = L"LightPhotoViewer - " + tab->title;
    di.lpszDocName = docTitle.c_str();

    if (StartDocW(hdc, &di) > 0) {
        if (StartPage(hdc) > 0) {
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, nullptr);

            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = outW;
            bi.bmiHeader.biHeight = outH; // Bottom-up
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;

            StretchDIBits(
                hdc,
                dstX, dstY, dstW, dstH,
                srcCropX, outH - (srcCropY + srcCropH), srcCropW, srcCropH,
                dibPixels.data(),
                &bi,
                DIB_RGB_COLORS,
                SRCCOPY
            );

            EndPage(hdc);
        }
        EndDoc(hdc);
    }
}
