#include "pch.hpp"
#include "app_window.hpp"
#include "../resources/resource_ids.hpp"

static const wchar_t* WND_CLASS_NAME = L"LightPhotoViewer_WndClass";
static const UINT_PTR TIMER_ANIM_AND_HUD = 1;

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

    // Enable Windows 10/11 Immersive Dark Mode for title bar
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

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

        int zoomPercent = static_cast<int>(tab->zoom * 100.0f + 0.5f);
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
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    TabItem* tab = m_tabManager.GetActiveTab();
    bool hasDoc = (tab && tab->document && tab->document->isValid);

    AppendMenuW(hMenu, MF_STRING, IDM_FILE_OPEN, L"Open Image...\tCtrl+O");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    if (hasDoc) {
        AppendMenuW(hMenu, MF_STRING, IDM_NAV_NEXT, L"Next Image\tRight / Space");
        AppendMenuW(hMenu, MF_STRING, IDM_NAV_PREV, L"Previous Image\tLeft / Backspace");
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

        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_COPY, L"Copy Image\tCtrl+C");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_COPY_PATH, L"Copy File Path\tCtrl+Shift+C");
        AppendMenuW(hMenu, MF_STRING, IDM_FILE_REVEAL, L"Reveal in File Explorer");
        AppendMenuW(hMenu, MF_STRING, IDM_FILE_DELETE, L"Delete to Recycle Bin\tDel");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hMenu, MF_STRING, IDM_VIEW_INFO, L"Toggle EXIF Info\tI");
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
        case HudButtonId::ZoomFit:   ZoomFit(); break;
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
            m_renderer.Render(m_tabManager, m_hudAlpha, m_showExif, m_showHelp, m_mousePt);
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

            // If Help Overlay is active, handle close button or backdrop click
            if (m_showHelp) {
                if (m_renderer.HitTestHelpClose(pt) || !m_renderer.HitTestHelpCard(pt)) {
                    m_showHelp = false;
                    Invalidate();
                }
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

            if (ctrlDown) {
                switch (wParam) {
                    case 'O': OpenFileDialog(); return 0;
                    case 'W': {
                        if (m_tabManager.GetActiveIndex() >= 0) {
                            m_tabManager.CloseTab(m_tabManager.GetActiveIndex());
                            UpdateWindowTitle();
                            Invalidate();
                        }
                        return 0;
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
                    return 0;

                case VK_LEFT:
                case VK_BACK:
                    PrevImage();
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
                case VK_NUMPAD0:
                    ZoomFit();
                    return 0;

                case '1':
                case VK_NUMPAD1:
                    Zoom100();
                    return 0;

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
                case VK_RETURN:
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
                    if (m_showHelp) {
                        m_showHelp = false;
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
                if (tab && tab->document && tab->document->metadata.isAnimated) {
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

                if (needsRepaint) {
                    Invalidate();
                }
            }
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
