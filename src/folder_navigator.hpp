#pragma once

#include "pch.hpp"
#include "image_engine.hpp"

class FolderNavigator {
public:
    FolderNavigator();
    ~FolderNavigator();

    bool OpenFile(const std::wstring& filePath);
    bool HasFiles() const;
    size_t GetTotalFiles() const;
    int GetCurrentIndex() const;
    std::wstring GetCurrentFilePath() const;

    std::shared_ptr<ImageDocument> GetCurrentDocument();
    std::shared_ptr<ImageDocument> NavigateNext();
    std::shared_ptr<ImageDocument> NavigatePrev();
    std::shared_ptr<ImageDocument> NavigateFirst();
    std::shared_ptr<ImageDocument> NavigateLast();

    void RequestPredecode();
    void RefreshFolder();

private:
    mutable std::mutex m_navMutex;
    std::wstring m_currentFolder;
    std::vector<std::wstring> m_files;
    int m_currentIndex = -1;

    // Speculative Pre-Decode LRU Cache & Event Synchronization
    std::mutex m_cvMutex;
    std::condition_variable m_cv;
    std::mutex m_cacheMutex;
    std::unordered_map<std::wstring, std::shared_ptr<ImageDocument>> m_cache;
    std::thread m_workerThread;
    std::atomic<bool> m_stopWorker{ false };
    std::atomic<bool> m_predecodeRequested{ false };

    void WorkerLoop();
    void ScanFolder(const std::wstring& folderPath, const std::wstring& initialFile);
    static bool IsSupportedExtension(const std::wstring& ext);
    std::shared_ptr<ImageDocument> LoadOrFetch(const std::wstring& path);
};
