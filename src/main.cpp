#include "pch.hpp"
#include "app_window.hpp"
#include "../resources/resource_ids.hpp"

static void PrintTestMessage(const wchar_t* msg) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleW(hOut, msg, static_cast<DWORD>(wcslen(msg)), &written, nullptr);
    }
}

#define TAB_ASSERT(cond) do { \
    if (!(cond)) { \
        wchar_t errBuf[256]; \
        swprintf_s(errBuf, L"  [FAIL] Assertion failed: %s (line %d)\n", _CRT_WIDE(#cond), __LINE__); \
        PrintTestMessage(errBuf); \
        return 1; \
    } \
} while (0)

static int RunTabUnitTests() {
    PrintTestMessage(L"Running Tab Context Menu & Tab Lifecycle Unit Tests...\n");

    // Test 1: Empty TabManager state
    {
        TabManager tm;
        TAB_ASSERT(tm.GetTabCount() == 0);
        TAB_ASSERT(tm.GetActiveIndex() == -1);
        TAB_ASSERT(!tm.CanReopenClosedTab());
        TAB_ASSERT(tm.GetClosedTabCount() == 0);
        TAB_ASSERT(!tm.CloseTab(0));
        TAB_ASSERT(!tm.CloseOtherTabs(0));
        TAB_ASSERT(!tm.CloseTabsToTheRight(0));
    }

    // Test 2: Single Tab Menu Enable/Disable Flags
    {
        TabManager tm;
        TAB_ASSERT(tm.OpenTab(L"samples\\01_highres.png"));
        TAB_ASSERT(tm.GetTabCount() == 1);
        TAB_ASSERT(tm.GetActiveIndex() == 0);

        HMENU hMenu = AppWindow::BuildTabContextMenu(tm, 0);
        TAB_ASSERT(hMenu != nullptr);

        // a) Close Tab: enabled
        UINT stateClose = GetMenuState(hMenu, IDM_TAB_CLOSE, MF_BYCOMMAND);
        TAB_ASSERT(!(stateClose & MF_GRAYED));

        // b) Close Other Tabs: DISABLED (only 1 tab open)
        UINT stateOther = GetMenuState(hMenu, IDM_TAB_CLOSE_OTHERS, MF_BYCOMMAND);
        TAB_ASSERT(stateOther & MF_GRAYED);

        // c) Close Tabs to the Right: DISABLED (tab 0 is right-most)
        UINT stateRight = GetMenuState(hMenu, IDM_TAB_CLOSE_RIGHT, MF_BYCOMMAND);
        TAB_ASSERT(stateRight & MF_GRAYED);

        // d) Reopen Closed Tab: DISABLED (closed-tab stack empty)
        UINT stateReopen = GetMenuState(hMenu, IDM_TAB_REOPEN, MF_BYCOMMAND);
        TAB_ASSERT(stateReopen & MF_GRAYED);

        // e) Copy File Path & Open Containing Folder: ENABLED (has file)
        UINT stateCopy = GetMenuState(hMenu, IDM_TAB_COPY_PATH, MF_BYCOMMAND);
        TAB_ASSERT(!(stateCopy & MF_GRAYED));
        UINT stateFolder = GetMenuState(hMenu, IDM_TAB_OPEN_FOLDER, MF_BYCOMMAND);
        TAB_ASSERT(!(stateFolder & MF_GRAYED));

        DestroyMenu(hMenu);
        PrintTestMessage(L"  [PASS] Single tab menu enable/disable flags verified.\n");
    }

    // Test 3: Multiple Tabs Menu Enable/Disable Flags
    {
        TabManager tm;
        TAB_ASSERT(tm.OpenTab(L"samples\\01_highres.png"));
        TAB_ASSERT(tm.OpenTab(L"samples\\02_sunset.jpg"));
        TAB_ASSERT(tm.OpenTab(L"samples\\03_animated_spinner.gif"));
        TAB_ASSERT(tm.GetTabCount() == 3);

        // Target: Tab 0 (first tab)
        HMENU hMenu0 = AppWindow::BuildTabContextMenu(tm, 0);
        TAB_ASSERT(hMenu0 != nullptr);
        // Close Other Tabs: ENABLED (3 tabs open)
        TAB_ASSERT(!(GetMenuState(hMenu0, IDM_TAB_CLOSE_OTHERS, MF_BYCOMMAND) & MF_GRAYED));
        // Close Tabs to the Right: ENABLED (tabs 1 and 2 are to the right)
        TAB_ASSERT(!(GetMenuState(hMenu0, IDM_TAB_CLOSE_RIGHT, MF_BYCOMMAND) & MF_GRAYED));
        DestroyMenu(hMenu0);

        // Target: Tab 2 (right-most tab)
        HMENU hMenu2 = AppWindow::BuildTabContextMenu(tm, 2);
        TAB_ASSERT(hMenu2 != nullptr);
        // Close Other Tabs: ENABLED
        TAB_ASSERT(!(GetMenuState(hMenu2, IDM_TAB_CLOSE_OTHERS, MF_BYCOMMAND) & MF_GRAYED));
        // Close Tabs to the Right: DISABLED (tab 2 is right-most)
        TAB_ASSERT(GetMenuState(hMenu2, IDM_TAB_CLOSE_RIGHT, MF_BYCOMMAND) & MF_GRAYED);
        DestroyMenu(hMenu2);

        PrintTestMessage(L"  [PASS] Multiple tabs menu enable/disable flags verified.\n");
    }

    // Test 4: CloseTabsToTheRight retaining correct active index and remaining tabs
    {
        TabManager tm;
        TAB_ASSERT(tm.OpenTab(L"samples\\01_highres.png"));
        TAB_ASSERT(tm.OpenTab(L"samples\\02_sunset.jpg"));
        TAB_ASSERT(tm.OpenTab(L"samples\\03_animated_spinner.gif"));
        TAB_ASSERT(tm.GetTabCount() == 3);
        TAB_ASSERT(tm.GetActiveIndex() == 2);

        int callbacksFired = 0;
        TAB_ASSERT(tm.CloseTabsToTheRight(0, [&](TabItem*) { ++callbacksFired; }));
        TAB_ASSERT(callbacksFired == 2);
        TAB_ASSERT(tm.GetTabCount() == 1);
        TAB_ASSERT(tm.GetActiveIndex() == 0); // Active clamped from 2 to 0
        TAB_ASSERT(tm.CanReopenClosedTab());
        TAB_ASSERT(tm.GetClosedTabCount() == 2);

        // When closed stack is not empty, Reopen Closed Tab menu item must be ENABLED
        HMENU hMenuAfterClose = AppWindow::BuildTabContextMenu(tm, 0);
        TAB_ASSERT(hMenuAfterClose != nullptr);
        TAB_ASSERT(!(GetMenuState(hMenuAfterClose, IDM_TAB_REOPEN, MF_BYCOMMAND) & MF_GRAYED));
        DestroyMenu(hMenuAfterClose);

        PrintTestMessage(L"  [PASS] CloseTabsToTheRight retaining active index & remaining tabs verified.\n");
    }

    // Test 5: CloseOtherTabs preserving only target tab
    {
        TabManager tm;
        TAB_ASSERT(tm.OpenTab(L"samples\\01_highres.png"));
        TAB_ASSERT(tm.OpenTab(L"samples\\02_sunset.jpg"));
        TAB_ASSERT(tm.OpenTab(L"samples\\03_animated_spinner.gif"));
        TAB_ASSERT(tm.GetTabCount() == 3);

        // Keep index 1 (02_sunset.jpg) while active was 0
        tm.SelectTab(0);
        TAB_ASSERT(tm.GetActiveIndex() == 0);

        int callbacksFired = 0;
        TAB_ASSERT(tm.CloseOtherTabs(1, [&](TabItem*) { ++callbacksFired; }));
        TAB_ASSERT(callbacksFired == 2);
        TAB_ASSERT(tm.GetTabCount() == 1);
        TAB_ASSERT(tm.GetActiveIndex() == 0);
        TAB_ASSERT(tm.GetActiveTab() != nullptr);
        TAB_ASSERT(tm.GetActiveTab()->title.find(L"02_sunset.jpg") != std::wstring::npos);

        PrintTestMessage(L"  [PASS] CloseOtherTabs preserving only target tab verified.\n");
    }

    // Test 6: ReopenClosedTab restoring tabs & viewing state
    {
        TabManager tm;
        TAB_ASSERT(tm.OpenTab(L"samples\\01_highres.png"));
        TAB_ASSERT(tm.OpenTab(L"samples\\02_sunset.jpg"));

        // Customize tab 1 transforms
        TabItem* t1 = tm.GetTab(1);
        TAB_ASSERT(t1 != nullptr);
        t1->zoom = 2.5f;
        t1->panX = 120.0f;
        t1->panY = -80.0f;
        t1->rotationDegrees = 90;
        t1->isFitToWindow = false;

        TAB_ASSERT(tm.CloseTab(1));
        TAB_ASSERT(tm.GetTabCount() == 1);
        TAB_ASSERT(tm.CanReopenClosedTab());

        TAB_ASSERT(tm.ReopenClosedTab());
        TAB_ASSERT(tm.GetTabCount() == 2);
        TAB_ASSERT(tm.GetActiveIndex() == 1);

        TabItem* reopenedTab = tm.GetActiveTab();
        TAB_ASSERT(reopenedTab != nullptr);
        TAB_ASSERT(reopenedTab->zoom == 2.5f);
        TAB_ASSERT(reopenedTab->panX == 120.0f);
        TAB_ASSERT(reopenedTab->panY == -80.0f);
        TAB_ASSERT(reopenedTab->rotationDegrees == 90);
        TAB_ASSERT(!reopenedTab->isFitToWindow);

        PrintTestMessage(L"  [PASS] ReopenClosedTab state restoration verified.\n");
    }

    PrintTestMessage(L"ALL TAB UNIT TESTS PASSED SUCCESSFULLY!\n");
    return 0;
}

int WINAPI wWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) return 1;

    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool silent = false;
    bool registerFlag = false;
    bool unregisterFlag = false;
    bool testTabsFlag = false;
    std::vector<std::wstring> filesToOpen;

    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (_wcsicmp(arg.c_str(), L"/register") == 0 || _wcsicmp(arg.c_str(), L"-register") == 0) {
                registerFlag = true;
            } else if (_wcsicmp(arg.c_str(), L"/unregister") == 0 || _wcsicmp(arg.c_str(), L"-unregister") == 0) {
                unregisterFlag = true;
            } else if (_wcsicmp(arg.c_str(), L"/silent") == 0 || _wcsicmp(arg.c_str(), L"-silent") == 0) {
                silent = true;
            } else if (_wcsicmp(arg.c_str(), L"/test-tabs") == 0 || _wcsicmp(arg.c_str(), L"-test-tabs") == 0) {
                testTabsFlag = true;
            } else if (arg.rfind(L"/", 0) != 0 && arg.rfind(L"-", 0) != 0) {
                filesToOpen.push_back(arg);
            }
        }
        LocalFree(argv);
    }

    if (testTabsFlag) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        int res = RunTabUnitTests();
        FreeConsole();
        CoUninitialize();
        ExitProcess(static_cast<UINT>(res));
    }

    if (registerFlag) {
        AppWindow::RegisterShellAssociations();
        if (!silent) {
            MessageBoxW(nullptr, L"LightPhotoViewer successfully registered!\n\nTo make it your default image viewer, choose it in Windows Settings > Apps > Default apps, or use \"Open with\" > \"Choose another app\" on any image.\n\nUse /unregister to remove the associations.", L"LightPhotoViewer", MB_OK | MB_ICONINFORMATION);
        }
        CoUninitialize();
        return 0;
    }

    if (unregisterFlag) {
        AppWindow::UnregisterShellAssociations();
        if (!silent) {
            MessageBoxW(nullptr, L"LightPhotoViewer shell associations successfully removed.", L"LightPhotoViewer", MB_OK | MB_ICONINFORMATION);
        }
        CoUninitialize();
        return 0;
    }

    // Single-instance handling: Forward files to running window if open
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, L"Local\\LightPhotoViewer_SingleInstance_Mutex");
    bool alreadyRunning = (GetLastError() == ERROR_ALREADY_EXISTS);

    if (alreadyRunning) {
        HWND hExistingWnd = FindWindowW(L"LightPhotoViewer_WndClass", nullptr);
        if (hExistingWnd) {
            if (!filesToOpen.empty()) {
                for (const auto& file : filesToOpen) {
                    wchar_t fullPath[MAX_PATH];
                    if (GetFullPathNameW(file.c_str(), MAX_PATH, fullPath, nullptr) > 0) {
                        COPYDATASTRUCT cds = {};
                        cds.dwData = 1;
                        cds.cbData = static_cast<DWORD>((wcslen(fullPath) + 1) * sizeof(wchar_t));
                        cds.lpData = fullPath;
                        SendMessageW(hExistingWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
                    }
                }
            } else {
                if (IsIconic(hExistingWnd)) ShowWindow(hExistingWnd, SW_RESTORE);
                SetForegroundWindow(hExistingWnd);
            }
            CloseHandle(hMutex);
            CoUninitialize();
            return 0;
        }
    }

    AppWindow::InitializeDarkMode();

    AppWindow app;
    if (!app.Create(nCmdShow, filesToOpen)) {
        CloseHandle(hMutex);
        CoUninitialize();
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CloseHandle(hMutex);
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
