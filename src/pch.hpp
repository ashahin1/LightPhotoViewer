#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <winspool.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <wincodec.h>
#include <wrl/client.h>

// DirectX / Direct2D
#include <d3d11_1.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <d2d1_3.h>
#include <d2d1effects.h>
#include <dwrite.h>
#include <dxgi1_2.h>

// Standard C++ Library
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <filesystem>
#include <unordered_map>
#include <deque>
#include <functional>

using Microsoft::WRL::ComPtr;
