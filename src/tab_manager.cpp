#include "pch.hpp"
#include "tab_manager.hpp"

bool TabManager::OpenTab(const std::wstring& filePath) {
    if (filePath.empty()) return false;

    // Check if file is already open in an existing tab
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->navigator && _wcsicmp(m_tabs[i]->navigator->GetCurrentFilePath().c_str(), filePath.c_str()) == 0) {
            m_activeIndex = static_cast<int>(i);
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
    return true;
}

bool TabManager::CloseTab(size_t index) {
    if (index >= m_tabs.size()) return false;

    m_tabs.erase(m_tabs.begin() + index);

    if (m_tabs.empty()) {
        m_activeIndex = -1;
    } else if (m_activeIndex >= static_cast<int>(m_tabs.size())) {
        m_activeIndex = static_cast<int>(m_tabs.size()) - 1;
    }
    return true;
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
