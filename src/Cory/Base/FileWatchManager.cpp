#include "FileWatchManager.hpp"

#include <Cory/Base/SlotMap.hpp>

#include <efsw/efsw.hpp>

#include "Log.hpp"

#include <atomic>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>
#include <string>
#include <unordered_map>

namespace Cory {
namespace {
namespace fs = std::filesystem;
constexpr efsw::WatchID kInvalidWatchId = std::numeric_limits<efsw::WatchID>::min();

[[nodiscard]] fs::path absoluteNormalized(const fs::path &path)
{
    std::error_code ec;
    auto absolutePath = fs::absolute(path, ec);
    if (ec) return path.lexically_normal();
    return absolutePath.lexically_normal();
}

[[nodiscard]] FileWatchEvent actionToEvent(efsw::Action action)
{
    switch (action) {
    case efsw::Actions::Add:
        return FileWatchEvent::Created;
    case efsw::Actions::Delete:
        return FileWatchEvent::Deleted;
    case efsw::Actions::Modified:
        return FileWatchEvent::Modified;
    default:
        return FileWatchEvent::Unknown;
    }
}

struct FileWatchCallbackState {
    fs::path targetPath;
    Function<void(FileWatchEvent)> callback;
    std::atomic<bool> active{true};
};

struct FileWatchEntry {
    efsw::WatchID watchId{kInvalidWatchId};
    std::shared_ptr<FileWatchCallbackState> state;
};

} // namespace

struct FileWatchManager::Private : public efsw::FileWatchListener {
    Private()
        : watcher(std::make_unique<efsw::FileWatcher>())
    {
        watcher->watch();
    }

    ~Private() override
    {
        stopAll();
    }

    void handleFileAction(efsw::WatchID watchId,
                          const std::string &dir,
                          const std::string &filename,
                          efsw::Action action,
                          std::string oldFilename) override
    {
        std::shared_ptr<FileWatchCallbackState> state;
        {
            std::scoped_lock lock(mutex);
            auto lookup = watchIdToHandle.find(watchId);
            if (lookup == watchIdToHandle.end()) return;
            if (!watches.isValid(lookup->second)) return;
            state = watches[lookup->second].state;
        }

        if (!state || !state->active.load(std::memory_order_relaxed)) return;

        const auto eventType = actionToEvent(action);
        if (eventType == FileWatchEvent::Unknown) return;

        const auto newPath = absoluteNormalized(fs::path(dir) / filename);
        bool relevant = newPath == state->targetPath;

        if (!relevant && !oldFilename.empty()) {
            const auto oldPath = absoluteNormalized(fs::path(dir) / oldFilename);
            relevant = oldPath == state->targetPath;
        }

        if (!relevant) return;

        state->callback(eventType);
    }

    void stopAll()
    {
        std::scoped_lock lock(mutex);
        for (const auto &[watchId, _] : watchIdToHandle) {
            if (watchId != kInvalidWatchId) {
                watcher->removeWatch(watchId);
            }
        }
        watchIdToHandle.clear();
        watches.clear();
    }

    SlotMap<FileWatchEntry> watches;
    std::unique_ptr<efsw::FileWatcher> watcher;
    std::unordered_map<efsw::WatchID, SlotMapHandle> watchIdToHandle;
    std::mutex mutex;
};

std::unique_ptr<FileWatchManager> FileWatchManager::s_instance;

void FileWatchManager::Init()
{
    s_instance = std::make_unique<FileWatchManager>();
}

void FileWatchManager::Shutdown()
{
    s_instance.reset();
}

FileWatchManager &FileWatchManager::instance()
{
    CO_CORE_ASSERT(s_instance != nullptr,
                   "Instance has not been initialized, or already shut down.");
    return *s_instance;
}

FileWatchManager::FileWatchManager()
    : data_(std::make_unique<Private>())
{
}
FileWatchManager::~FileWatchManager() = default;

FileWatchManager::FileWatchManager(FileWatchManager &&rhs) noexcept = default;
FileWatchManager &FileWatchManager::operator=(FileWatchManager &&rhs) noexcept = default;

FileWatchHandle FileWatchManager::watch(FileWatch watch,
                                        Function<void(FileWatchEvent)> callback)
{
    auto targetPath = absoluteNormalized(fs::path{watch.path});
    auto directoryPath = targetPath.parent_path();
    if (directoryPath.empty()) directoryPath = targetPath;
    directoryPath = absoluteNormalized(directoryPath);

    std::error_code ec;
    const bool directoryExists = fs::exists(directoryPath, ec) && fs::is_directory(directoryPath, ec);
    if (!directoryExists) {
        CO_CORE_ERROR("Unable to watch '{}': directory '{}' does not exist or is not accessible.",
                      targetPath.string(),
                      directoryPath.string());
        return {};
    }

    auto state = std::make_shared<FileWatchCallbackState>();
    state->targetPath = targetPath;
    state->callback = std::move(callback);

    efsw::WatchID watchId = kInvalidWatchId;
    try {
        watchId = data_->watcher->addWatch(directoryPath.string(), data_.get(), false);
    }
    catch (const std::exception &e) {
        CO_CORE_ERROR("Failed to start file watch for '{}': {}", targetPath.string(), e.what());
        return {};
    }

    if (watchId < 0) {
        CO_CORE_ERROR("Failed to start file watch for '{}': invalid watch id returned.", targetPath.string());
        return {};
    }

    SlotMapHandle handle;
    {
        std::scoped_lock lock(data_->mutex);
        handle = data_->watches.emplace(FileWatchEntry{watchId, state});
        data_->watchIdToHandle[watchId] = handle;
    }
    return handle;
}

bool FileWatchManager::unwatch(FileWatchHandle handle)
{
    if (!handle) return false;

    efsw::WatchID watchId = kInvalidWatchId;
    std::shared_ptr<FileWatchCallbackState> state;

    {
        std::scoped_lock lock(data_->mutex);
        if (!data_->watches.isValid(handle)) return false;

        auto &entry = data_->watches[handle];
        watchId = entry.watchId;
        state = entry.state;
        if (watchId != kInvalidWatchId) {
            data_->watchIdToHandle.erase(watchId);
        }
        data_->watches.release(handle);
    }

    if (state) state->active.store(false, std::memory_order_relaxed);
    if (watchId != kInvalidWatchId) {
        data_->watcher->removeWatch(watchId);
    }
    return true;
}

} // namespace Cory
