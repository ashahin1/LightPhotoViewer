#include "pch.hpp"
#include "folder_navigator.hpp"
#include "image_engine.hpp"

FolderNavigator::FolderNavigator() {
    m_stopWorker = false;
    m_workerThread = std::thread(&FolderNavigator::WorkerLoop, this);
}

FolderNavigator::~FolderNavigator() {
    m_stopWorker = true;
    m_predecodeRequested = true;
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

bool FolderNavigator::IsSupportedExtension(const std::wstring& ext) {
    std::wstring lower = ext;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

    if (lower == L".jpg"  || lower == L".jpeg" || lower == L".jpe" ||
        lower == L".png"  || lower == L".gif"  || lower == L".webp" ||
        lower == L".bmp"  || lower == L".dib"  || lower == L".ico"  ||
        lower == L".tiff" || lower == L".tif"  || lower == L".avif" ||
        lower == L".svg"  || lower == L".heic" || lower == L".heif") {
        return true;
    }
    return ImageEngine::IsRawExtension(lower);
}

void FolderNavigator::ScanFolder(const std::wstring& folderPath, const std::wstring& initialFile) {
    std::lock_guard<std::mutex> lock(m_navMutex);
    m_files.clear();
    m_currentIndex = -1;
    m_currentFolder = folderPath;

    std::wstring searchPattern = folderPath + L"\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileExW(
        searchPattern.c_str(),
        FindExInfoBasic,
        &findData,
        FindExSearchNameMatch,
        nullptr,
        FIND_FIRST_EX_LARGE_FETCH
    );

    if (hFind == INVALID_HANDLE_VALUE) {
        if (!initialFile.empty()) {
            m_files.push_back(initialFile);
            m_currentIndex = 0;
        }
        return;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const wchar_t* dot = wcsrchr(findData.cFileName, L'.');
            if (dot && IsSupportedExtension(dot)) {
                m_files.push_back(folderPath + L"\\" + findData.cFileName);
            }
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);

    std::sort(m_files.begin(), m_files.end(), [](const std::wstring& a, const std::wstring& b) {
        return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
    });

    for (size_t i = 0; i < m_files.size(); ++i) {
        if (_wcsicmp(m_files[i].c_str(), initialFile.c_str()) == 0) {
            m_currentIndex = static_cast<int>(i);
            break;
        }
    }

    if (m_currentIndex < 0 && !initialFile.empty()) {
        m_files.push_back(initialFile);
        std::sort(m_files.begin(), m_files.end(), [](const std::wstring& a, const std::wstring& b) {
            return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
        });
        for (size_t i = 0; i < m_files.size(); ++i) {
            if (_wcsicmp(m_files[i].c_str(), initialFile.c_str()) == 0) {
                m_currentIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (m_currentIndex < 0 && !m_files.empty()) {
        m_currentIndex = 0;
    }
}

bool FolderNavigator::OpenFile(const std::wstring& filePath) {
    try {
        std::filesystem::path p(filePath);
        std::wstring absPath = std::filesystem::absolute(p).wstring();
        std::wstring parentDir = p.parent_path().wstring();

        ScanFolder(parentDir, absPath);
        RequestPredecode();
        return true;
    } catch (...) {
        return false;
    }
}

bool FolderNavigator::HasFiles() const {
    std::lock_guard<std::mutex> lock(m_navMutex);
    return !m_files.empty();
}

size_t FolderNavigator::GetTotalFiles() const {
    std::lock_guard<std::mutex> lock(m_navMutex);
    return m_files.size();
}

int FolderNavigator::GetCurrentIndex() const {
    std::lock_guard<std::mutex> lock(m_navMutex);
    return m_currentIndex;
}

void FolderNavigator::RefreshFolder() {
    std::wstring folder;
    std::wstring current;
    {
        std::lock_guard<std::mutex> lock(m_navMutex);
        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_files.size())) {
            current = m_files[m_currentIndex];
            folder = m_currentFolder;
        }
    }
    if (!folder.empty()) {
        ScanFolder(folder, current);
        RequestPredecode();
    }
}

std::wstring FolderNavigator::GetCurrentFilePath() const {
    std::lock_guard<std::mutex> lock(m_navMutex);
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_files.size())) {
        return m_files[m_currentIndex];
    }
    return L"";
}

std::shared_ptr<ImageDocument> FolderNavigator::LoadOrFetch(const std::wstring& path) {
    if (path.empty()) return nullptr;

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(path);
        if (it != m_cache.end()) {
            return it->second;
        }
    }

    auto doc = ImageEngine::Instance().LoadFromFile(path);
    if (doc) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cache[path] = doc;
    }
    return doc;
}

std::shared_ptr<ImageDocument> FolderNavigator::GetCurrentDocument() {
    std::wstring path = GetCurrentFilePath();
    auto doc = LoadOrFetch(path);
    RequestPredecode();
    return doc;
}

std::shared_ptr<ImageDocument> FolderNavigator::NavigateNext() {
    std::wstring nextPath;
    {
        std::lock_guard<std::mutex> lock(m_navMutex);
        if (m_files.empty()) return nullptr;
        m_currentIndex = (m_currentIndex + 1) % static_cast<int>(m_files.size());
        nextPath = m_files[m_currentIndex];
    }
    auto doc = LoadOrFetch(nextPath);
    RequestPredecode();
    return doc;
}

std::shared_ptr<ImageDocument> FolderNavigator::NavigatePrev() {
    std::wstring prevPath;
    {
        std::lock_guard<std::mutex> lock(m_navMutex);
        if (m_files.empty()) return nullptr;
        m_currentIndex = (m_currentIndex - 1 + static_cast<int>(m_files.size())) % static_cast<int>(m_files.size());
        prevPath = m_files[m_currentIndex];
    }
    auto doc = LoadOrFetch(prevPath);
    RequestPredecode();
    return doc;
}

std::shared_ptr<ImageDocument> FolderNavigator::NavigateFirst() {
    std::wstring firstPath;
    {
        std::lock_guard<std::mutex> lock(m_navMutex);
        if (m_files.empty()) return nullptr;
        m_currentIndex = 0;
        firstPath = m_files[0];
    }
    auto doc = LoadOrFetch(firstPath);
    RequestPredecode();
    return doc;
}

std::shared_ptr<ImageDocument> FolderNavigator::NavigateLast() {
    std::wstring lastPath;
    {
        std::lock_guard<std::mutex> lock(m_navMutex);
        if (m_files.empty()) return nullptr;
        m_currentIndex = static_cast<int>(m_files.size()) - 1;
        lastPath = m_files[m_currentIndex];
    }
    auto doc = LoadOrFetch(lastPath);
    RequestPredecode();
    return doc;
}

void FolderNavigator::RequestPredecode() {
    m_predecodeRequested = true;
    m_cv.notify_one();
}

void FolderNavigator::WorkerLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (!m_stopWorker) {
        {
            std::unique_lock<std::mutex> cvLock(m_cvMutex);
            m_cv.wait(cvLock, [this]() {
                return m_stopWorker.load() || m_predecodeRequested.load();
            });
            if (m_stopWorker) break;
            m_predecodeRequested = false;
        }

        std::wstring nextToPreload;
        std::wstring prevToPreload;
        std::wstring currentPath;

        {
            std::lock_guard<std::mutex> lock(m_navMutex);
            if (m_currentIndex >= 0 && !m_files.empty()) {
                currentPath = m_files[m_currentIndex];
                int nextIdx = (m_currentIndex + 1) % static_cast<int>(m_files.size());
                int prevIdx = (m_currentIndex - 1 + static_cast<int>(m_files.size())) % static_cast<int>(m_files.size());

                if (nextIdx != m_currentIndex) nextToPreload = m_files[nextIdx];
                if (prevIdx != m_currentIndex && prevIdx != nextIdx) prevToPreload = m_files[prevIdx];
            }
        }

        if (!nextToPreload.empty() && !m_stopWorker) {
            bool needed = false;
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                needed = (m_cache.find(nextToPreload) == m_cache.end());
            }
            if (needed) {
                auto doc = ImageEngine::Instance().LoadFromFile(nextToPreload);
                if (doc) {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    m_cache[nextToPreload] = doc;
                }
            }
        }

        if (!prevToPreload.empty() && !m_stopWorker) {
            bool needed = false;
            {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                needed = (m_cache.find(prevToPreload) == m_cache.end());
            }
            if (needed) {
                auto doc = ImageEngine::Instance().LoadFromFile(prevToPreload);
                if (doc) {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    m_cache[prevToPreload] = doc;
                }
            }
        }

        // Evict distant cached items
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            for (auto it = m_cache.begin(); it != m_cache.end(); ) {
                if (it->first != currentPath && it->first != nextToPreload && it->first != prevToPreload) {
                    it = m_cache.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    CoUninitialize();
}
