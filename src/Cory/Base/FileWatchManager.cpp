#include "FileWatchManager.hpp"

#include <Cory/Base/SlotMap.hpp>

#include <efsw/efsw.hpp>

#include "Log.hpp"

#include <atomic>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
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

[[nodiscard]] FileWatchEventType actionToEvent(efsw::Action action)
{
    switch (action) {
    case efsw::Actions::Add:
        return FileWatchEventType::Created;
    case efsw::Actions::Delete:
        return FileWatchEventType::Deleted;
    case efsw::Actions::Modified:
        return FileWatchEventType::Modified;
    default:
        return FileWatchEventType::Unknown;
    }
}

struct FileWatchEvent {
    FileWatchHandle handle;
    FileWatchEventType type;
};

struct FileWatchEntry {
    efsw::WatchID watchId{kInvalidWatchId};
    Function<void(FileWatchEventType)> callback;
};

struct WatchDescriptor {
    FileWatchHandle handle;
    fs::path targetPath;
};

} // namespace

struct FileWatchManager::Private : public efsw::FileWatchListener {
    Private()
        : watcher(std::make_unique<efsw::FileWatcher>())
    {
        watcher->watch();
    }

    ~Private() override { stopAll(); }

    void handleFileAction(efsw::WatchID watchId,
                          const std::string &dir,
                          const std::string &filename,
                          efsw::Action action,
                          std::string oldFilename) override
    {
        // When a potential file action is detected, it might not apply to the
        // actual file associated with the watch. So we check if the path matches
        // the target path of the watch before adding it to the pending events.
        std::scoped_lock lock(mutex);

        auto lookup = watchIdToHandle.find(watchId);
        if (lookup == watchIdToHandle.end()) return;

        const auto eventType = actionToEvent(action);
        if (eventType == FileWatchEventType::Unknown) return;

        const auto newPath = absoluteNormalized(fs::path(dir) / filename);
        bool relevant = newPath == lookup->second.targetPath;

        if (!relevant && !oldFilename.empty()) {
            const auto oldPath = absoluteNormalized(fs::path(dir) / oldFilename);
            relevant = oldPath == lookup->second.targetPath;
        }

        if (!relevant) return;

        pendingEvents.push_back(FileWatchEvent{
            .handle = lookup->second.handle,
            .type = eventType,
        });
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
    std::unordered_map<efsw::WatchID, WatchDescriptor> watchIdToHandle;
    std::vector<FileWatchEvent> pendingEvents;
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
    CO_CORE_DEBUG_ASSERT(s_instance != nullptr,
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
                                        Function<void(FileWatchEventType)> callback)
{
    auto targetPath = absoluteNormalized(fs::path{watch.path});
    auto directoryPath = targetPath.parent_path();
    if (directoryPath.empty()) directoryPath = targetPath;
    directoryPath = absoluteNormalized(directoryPath);

    std::error_code ec;
    const bool directoryExists =
        fs::exists(directoryPath, ec) && fs::is_directory(directoryPath, ec);
    if (!directoryExists) {
        CO_CORE_ERROR("Unable to watch '{}': directory '{}' does not exist or is not accessible.",
                      targetPath.string(),
                      directoryPath.string());
        return {};
    }

    efsw::WatchID watchId = data_->watcher->addWatch(directoryPath.string(), data_.get(), false);

    if (watchId < 0) {
        CO_CORE_ERROR("Failed to start file watch for '{}': invalid watch id returned.",
                      targetPath.string());
        return {};
    }

    SlotMapHandle handle;
    {
        std::scoped_lock lock(data_->mutex);
        handle = data_->watches.emplace(FileWatchEntry{
            .watchId = watchId,
            .callback = std::move(callback),
        });

        data_->watchIdToHandle[watchId] = WatchDescriptor{
            .handle = handle,
            .targetPath = targetPath,
        };
    }
    return handle;
}

bool FileWatchManager::unwatch(FileWatchHandle handle)
{
    std::scoped_lock lock(data_->mutex);
    if (!handle) return false;

    efsw::WatchID watchId = kInvalidWatchId;

    if (!data_->watches.isValid(handle)) return false;

    auto &entry = data_->watches[handle];
    watchId = entry.watchId;
    if (watchId != kInvalidWatchId) {
        data_->watchIdToHandle.erase(watchId);
    }
    data_->watches.release(handle);

    if (watchId != kInvalidWatchId) {
        data_->watcher->removeWatch(watchId);
    }
    return true;
}

void FileWatchManager::processPendingEvents()
{
    std::scoped_lock lock(data_->mutex);
    for (const auto &event : data_->pendingEvents) {
        // Watch has been removed in the meantime
        if (!data_->watches.isValid(event.handle)) {
            continue;
        }

        auto &entry = data_->watches[event.handle];
        if (entry.callback) {
            entry.callback(event.type);
        }
    }
    data_->pendingEvents.clear();
}

} // namespace Cory
