#include "pch.hpp"
#include "app_window.hpp"

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
            } else if (arg.rfind(L"/", 0) != 0 && arg.rfind(L"-", 0) != 0) {
                filesToOpen.push_back(arg);
            }
        }
        LocalFree(argv);
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
