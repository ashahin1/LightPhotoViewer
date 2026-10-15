#pragma once

#include "pch.hpp"
#include "image_engine.hpp"
#include "folder_navigator.hpp"

struct TabItem {
    std::wstring title;
    std::unique_ptr<FolderNavigator> navigator;
    std::shared_ptr<ImageDocument> document;

    // Viewport transform
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool isFitToWindow = true; // User preference: fit to window by default
    int rotationDegrees = 0;   // 0, 90, 180, 270
    bool flipH = false;
    bool flipV = false;

    // Animation playback
    size_t currentFrame = 0;
    ULONGLONG lastFrameTick = 0;

    // Direct2D GPU bitmaps (instantiated on render thread)
    std::vector<ComPtr<ID2D1Bitmap1>> gpuBitmaps;

    void ResetTransforms() {
        zoom = 1.0f;
        panX = 0.0f;
        panY = 0.0f;
        isFitToWindow = true;
        rotationDegrees = 0;
        flipH = false;
        flipV = false;
        currentFrame = 0;
        lastFrameTick = 0;
    }

    void ClearGpuBitmaps() {
        gpuBitmaps.clear();
    }
};

class TabManager {
public:
    TabManager() = default;
    ~TabManager() = default;

    bool OpenTab(const std::wstring& filePath);
    bool CloseTab(size_t index);
    bool SelectTab(size_t index);
    void NextTab();
    void PrevTab();

    size_t GetTabCount() const { return m_tabs.size(); }
    int GetActiveIndex() const { return m_activeIndex; }
    TabItem* GetActiveTab();
    const TabItem* GetTab(size_t index) const;

    bool ShouldShowTabs() const { return m_tabs.size() > 1; }

private:
    std::vector<std::unique_ptr<TabItem>> m_tabs;
    int m_activeIndex = -1;
};
