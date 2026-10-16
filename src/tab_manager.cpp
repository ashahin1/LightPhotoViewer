#include "pch.hpp"
#include "tab_manager.hpp"

static const wchar_t* REG_RECENT_KEY = L"Software\\LightPhotoViewer\\RecentFiles";

TabManager::TabManager() {
    LoadRecentFilesFromRegistry();
}

void TabManager::LoadRecentFilesFromRegistry() {
    m_recentFiles.clear();
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RECENT_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        for (int i = 0; i < 9; ++i) {
            std::wstring valName = L"File" + std::to_wstring(i);
            wchar_t buf[MAX_PATH] = {};
            DWORD bufSize = sizeof(buf);
            DWORD type = REG_SZ;
            if (RegQueryValueExW(hKey, valName.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(buf), &bufSize) == ERROR_SUCCESS) {
                if (buf[0] != L'\0' && std::filesystem::exists(buf)) {
                    m_recentFiles.push_back(buf);
                }
            }
        }
        RegCloseKey(hKey);
    }
}

void TabManager::SaveRecentFilesToRegistry() {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_RECENT_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        // Clear old entries
        for (int i = 0; i < 9; ++i) {
            std::wstring valName = L"File" + std::to_wstring(i);
            RegDeleteValueW(hKey, valName.c_str());
        }
        for (size_t i = 0; i < m_recentFiles.size() && i < 9; ++i) {
            std::wstring valName = L"File" + std::to_wstring(i);
            const auto& path = m_recentFiles[i];
            RegSetValueExW(hKey, valName.c_str(), 0, REG_SZ,
                reinterpret_cast<const BYTE*>(path.c_str()),
                static_cast<DWORD>((path.length() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(hKey);
    }
}

void TabManager::AddRecentFile(const std::wstring& path) {
    if (path.empty()) return;

    // Remove if already exists
    m_recentFiles.erase(
        std::remove_if(m_recentFiles.begin(), m_recentFiles.end(),
            [&path](const std::wstring& item) { return _wcsicmp(item.c_str(), path.c_str()) == 0; }),
        m_recentFiles.end()
    );

    // Insert at front
    m_recentFiles.insert(m_recentFiles.begin(), path);
    if (m_recentFiles.size() > 9) {
        m_recentFiles.resize(9);
    }
    SaveRecentFilesToRegistry();
}

bool TabManager::OpenTab(const std::wstring& filePath) {
    if (filePath.empty()) return false;

    // Check if file is already open in an existing tab
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->navigator && _wcsicmp(m_tabs[i]->navigator->GetCurrentFilePath().c_str(), filePath.c_str()) == 0) {
            m_activeIndex = static_cast<int>(i);
            AddRecentFile(filePath);
            return true;
        }
    }

    auto tab = std::make_unique<TabItem>();
    tab->navigator = std::make_unique<FolderNavigator>();
    if (!tab->navigator->OpenFile(filePath)) {
        return false;
    }

    tab->document = tab->navigator->GetCurrentDocument();
    if (!tab->document) {
        return false;
    }

    try {
        std::filesystem::path p(filePath);
        tab->title = p.filename().wstring();
    } catch (...) {
        tab->title = filePath;
    }

    tab->ResetTransforms();

    m_tabs.push_back(std::move(tab));
    m_activeIndex = static_cast<int>(m_tabs.size()) - 1;
    AddRecentFile(filePath);
    return true;
}

bool TabManager::CloseTab(size_t index) {
    if (index >= m_tabs.size()) return false;

    if (m_tabs[index]->navigator) {
        std::wstring closedPath = m_tabs[index]->navigator->GetCurrentFilePath();
        if (!closedPath.empty()) {
            m_closedTabs.push_front(closedPath);
            if (m_closedTabs.size() > 20) m_closedTabs.pop_back();
        }
    }

    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        m_activeIndex = -1;
    } else if (m_activeIndex >= static_cast<int>(m_tabs.size())) {
        m_activeIndex = static_cast<int>(m_tabs.size()) - 1;
    }
    return true;
}

bool TabManager::ReopenClosedTab() {
    while (!m_closedTabs.empty()) {
        std::wstring path = m_closedTabs.front();
        m_closedTabs.pop_front();
        if (std::filesystem::exists(path)) {
            return OpenTab(path);
        }
    }
    return false;
}

bool TabManager::SelectTab(size_t index) {
    if (index < m_tabs.size()) {
        m_activeIndex = static_cast<int>(index);
        return true;
    }
    return false;
}

void TabManager::NextTab() {
    if (m_tabs.size() > 1) {
        m_activeIndex = (m_activeIndex + 1) % static_cast<int>(m_tabs.size());
    }
}

void TabManager::PrevTab() {
    if (m_tabs.size() > 1) {
        m_activeIndex = (m_activeIndex - 1 + static_cast<int>(m_tabs.size())) % static_cast<int>(m_tabs.size());
    }
}

TabItem* TabManager::GetActiveTab() {
    if (m_activeIndex >= 0 && m_activeIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[m_activeIndex].get();
    }
    return nullptr;
}

const TabItem* TabManager::GetTab(size_t index) const {
    if (index < m_tabs.size()) {
        return m_tabs[index].get();
    }
    return nullptr;
}
