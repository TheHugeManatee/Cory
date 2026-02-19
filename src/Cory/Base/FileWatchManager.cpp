#include "FileWatchManager.hpp"

#include "Locked.hpp"

#include <Cory/Base/SlotMap.hpp>

#include <efsw/efsw.hpp>

#include "Log.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

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
    std::deque<FileWatchEventType> queuedEvents;
    cppcoro::coroutine_handle<> waitingConsumer{};
    bool consumerAttached{false};
    bool pendingRemoval{false};
};

struct WatchDescriptor {
    FileWatchHandle handle;
    fs::path targetPath;
};

} // namespace

/// The FileWatchListener will receive events from efsw on a separate thread. It will use the
struct FileWatchManager::Private : public efsw::FileWatchListener {
    Private()
        : watcher(std::make_unique<efsw::FileWatcher>())
    {
        auto lockedWatcher = watcher.lock();
        (*lockedWatcher)->watch();
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
        auto data = fileWatchThreadData.lock();

        auto lookup = data->watchIdToDescriptors.find(watchId);
        if (lookup == data->watchIdToDescriptors.end()) return;

        const auto eventType = actionToEvent(action);
        if (eventType == FileWatchEventType::Unknown) return;

        const auto newPath = absoluteNormalized(fs::path(dir) / filename);
        const auto oldPath =
            oldFilename.empty() ? fs::path{} : absoluteNormalized(fs::path(dir) / oldFilename);

        for (const auto &watchDescriptor : lookup->second) {
            bool relevant = newPath == watchDescriptor.targetPath;

            if (!relevant && !oldFilename.empty()) {
                relevant = oldPath == watchDescriptor.targetPath;
            }

            if (!relevant) continue;

            data->pendingEvents.push_back(FileWatchEvent{
                .handle = watchDescriptor.handle,
                .type = eventType,
            });
        }
    }

    void stopAll()
    {
        std::vector<efsw::WatchID> watchIdsToRemove;
        {
            auto data = fileWatchThreadData.lock();
            watchIdsToRemove.reserve(data->watchIdToDescriptors.size());
            for (const auto &[watchId, _] : data->watchIdToDescriptors) {
                if (watchId != kInvalidWatchId) watchIdsToRemove.push_back(watchId);
            }
            data->watchIdToDescriptors.clear();
            data->directoryToWatchId.clear();
            data->pendingEvents.clear();
            watches.clear();
        }

        auto lockedWatcher = watcher.lock();
        for (auto watchId : watchIdsToRemove) {
            (*lockedWatcher)->removeWatch(watchId);
        }
    }

    SlotMap<FileWatchEntry> watches;
    Locked<std::unique_ptr<efsw::FileWatcher>> watcher;

    struct FileWatchThreadData {
        std::unordered_map<efsw::WatchID, std::vector<WatchDescriptor>> watchIdToDescriptors;
        std::unordered_map<fs::path, efsw::WatchID> directoryToWatchId;
        std::vector<FileWatchEvent> pendingEvents;
    };
    Locked<FileWatchThreadData> fileWatchThreadData;
};

FileWatchManager *FileWatchManager::s_instance = nullptr;

void FileWatchManager::Init()
{
    CO_CORE_DEBUG_ASSERT(s_instance == nullptr, "FileWatchManager is already initialized.");
    s_instance = new FileWatchManager();
}

void FileWatchManager::Shutdown()
{
    delete s_instance;
    s_instance = nullptr;
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

FileWatchManager::~FileWatchManager()
{
    // Moved-from?
    if (!data_) return;

    std::vector<SlotMapHandle> handles;
    handles.reserve(data_->watches.size());
    for (auto handle : data_->watches.handles()) {
        handles.push_back(handle);
    }

    for (auto handle : handles) {
        unwatch(handle);
    }
    // After unwatching all remaining handles, make sure all pending events are processed
    processPendingEvents();

    // At this point, all watches should be unwatched and cleaned up.
}

FileWatchManager::FileWatchManager(FileWatchManager &&rhs) noexcept = default;
FileWatchManager &FileWatchManager::operator=(FileWatchManager &&rhs) noexcept = default;

FileWatchHandle FileWatchManager::watch(FileWatch watch)
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

    efsw::WatchID watchId = kInvalidWatchId;
    efsw::WatchID extraWatchIdToRemove = kInvalidWatchId;
    bool needsNewDirectoryWatch = false;
    {
        auto watchThreadData = data_->fileWatchThreadData.lock();

        if (auto existingWatch = watchThreadData->directoryToWatchId.find(directoryPath);
            existingWatch != watchThreadData->directoryToWatchId.end()) {
            watchId = existingWatch->second;
        }
        else needsNewDirectoryWatch = true;
    }

    if (needsNewDirectoryWatch) {
        efsw::WatchID createdWatchId = kInvalidWatchId;
        {
            auto lockedWatcher = data_->watcher.lock();
            createdWatchId = (*lockedWatcher)->addWatch(directoryPath.string(), data_.get(), false);
        }

        if (createdWatchId < 0) {
            CO_CORE_ERROR("Failed to start file watch for {} -- invalid watch id returned.",
                          targetPath.string());
            return {};
        }

        auto watchThreadData = data_->fileWatchThreadData.lock();
        if (auto existingWatch = watchThreadData->directoryToWatchId.find(directoryPath);
            existingWatch != watchThreadData->directoryToWatchId.end()) {
            watchId = existingWatch->second;
            extraWatchIdToRemove = createdWatchId;
        }
        else {
            watchId = createdWatchId;
            watchThreadData->directoryToWatchId[directoryPath] = watchId;
            watchThreadData->watchIdToDescriptors[watchId] = {};
        }
    }

    if (extraWatchIdToRemove != kInvalidWatchId) {
        auto lockedWatcher = data_->watcher.lock();
        (*lockedWatcher)->removeWatch(extraWatchIdToRemove);
    }

    SlotMapHandle handle = data_->watches.emplace(watchId);
    {
        auto watchThreadData = data_->fileWatchThreadData.lock();
        watchThreadData->watchIdToDescriptors[watchId].push_back(WatchDescriptor{
            .handle = handle,
            .targetPath = targetPath,
        });
    }
    return handle;
}

bool FileWatchManager::unwatch(FileWatchHandle handle)
{
    if (!handle) return false;

    efsw::WatchID watchId = kInvalidWatchId;
    bool removeDirectoryWatch = false;
    std::vector<cppcoro::coroutine_handle<>> waiters;
    {
        if (!data_->watches.isValid(handle)) return false;

        auto &entry = data_->watches[handle];
        watchId = entry.watchId;

        if (watchId != kInvalidWatchId) {
            auto watchThreadData = data_->fileWatchThreadData.lock();

            auto descriptorsIt = watchThreadData->watchIdToDescriptors.find(watchId);
            if (descriptorsIt != watchThreadData->watchIdToDescriptors.end()) {
                auto &descriptors = descriptorsIt->second;
                descriptors.erase(std::remove_if(descriptors.begin(),
                                                 descriptors.end(),
                                                 [handle](const WatchDescriptor &descriptor) {
                                                     return descriptor.handle == handle;
                                                 }),
                                  descriptors.end());

                if (descriptors.empty()) {
                    watchThreadData->watchIdToDescriptors.erase(descriptorsIt);
                    for (auto dirIt = watchThreadData->directoryToWatchId.begin();
                         dirIt != watchThreadData->directoryToWatchId.end();
                         ++dirIt) {
                        if (dirIt->second == watchId) {
                            watchThreadData->directoryToWatchId.erase(dirIt);
                            break;
                        }
                    }
                    removeDirectoryWatch = true;
                }
            }

            entry.watchId = kInvalidWatchId;
        }

        if (!entry.consumerAttached) {
            data_->watches.release(handle);
        }
        else {
            entry.pendingRemoval = true;
            const bool wasEmpty = entry.queuedEvents.empty();
            entry.queuedEvents.push_back(FileWatchEventType::WatchEnded);
            if (wasEmpty && entry.waitingConsumer) {
                waiters.push_back(entry.waitingConsumer);
                entry.waitingConsumer = nullptr;
            }
        }
    }

    if (removeDirectoryWatch && watchId != kInvalidWatchId) {
        auto lockedWatcher = data_->watcher.lock();
        (*lockedWatcher)->removeWatch(watchId);
    }

    for (auto waiter : waiters) {
        if (waiter) waiter.resume();
    }

    return true;
}

void FileWatchManager::processPendingEvents()
{
    std::vector<cppcoro::coroutine_handle<>> waiters;
    {
        std::vector<FileWatchEvent> events;
        {
            auto watchThreadData = data_->fileWatchThreadData.lock();
            events.swap(watchThreadData->pendingEvents);
        }

        waiters.reserve(events.size());

        for (const auto &event : events) {
            if (!data_->watches.isValid(event.handle)) continue;

            auto &entry = data_->watches[event.handle];
            const bool wasEmpty = entry.queuedEvents.empty();
            entry.queuedEvents.push_back(event.type);
            // If the entry already had queued events, it's already in our waiters list
            if (wasEmpty && entry.waitingConsumer) {
                waiters.push_back(entry.waitingConsumer);
                entry.waitingConsumer = nullptr;
            }
        }
    }

    for (auto waiter : waiters) {
        if (waiter) waiter.resume();
    }
}

FileWatchManager::NextEventAwaitable FileWatchManager::nextEvent(FileWatchHandle handle)
{
    CO_CORE_ASSERT(data_->watches.isValid(handle),
                   "Attempted to await events on an invalid FileWatchHandle.");
    return NextEventAwaitable{*this, handle};
}

FileWatchManager::NextEventAwaitable::NextEventAwaitable(FileWatchManager &managerRef,
                                                         FileWatchHandle handleToWatch)
    : manager(&managerRef)
    , handle(handleToWatch)
{
    manager->ensureConsumer(handleToWatch);
}

FileWatchManager::NextEventAwaitable::~NextEventAwaitable()
{
    if (suspended) {
        manager->cancelSuspendedConsumer(handle, suspendedHandle);
    }
}

bool FileWatchManager::NextEventAwaitable::await_ready()
{
    cachedEvent = manager->tryConsumeQueuedEvent(handle);
    return cachedEvent.has_value();
}

bool FileWatchManager::NextEventAwaitable::await_suspend(cppcoro::coroutine_handle<> h)
{
    cachedEvent = manager->tryConsumeQueuedEvent(handle);
    if (cachedEvent.has_value()) return false;

    suspendedHandle = h;
    suspended = manager->suspendConsumer(handle, h);
    return suspended;
}

FileWatchEventType FileWatchManager::NextEventAwaitable::await_resume()
{
    if (!cachedEvent.has_value()) {
        cachedEvent = manager->tryConsumeQueuedEvent(handle);
        CO_CORE_ASSERT(cachedEvent.has_value(),
                       "Resumed FileWatchManager::nextEvent without a pending event.");
    }
    suspended = false;
    suspendedHandle = {};
    return *cachedEvent;
}

void FileWatchManager::detach(FileWatchHandle handle)
{
    detachConsumer(handle);
}

void FileWatchManager::detachConsumer(FileWatchHandle handle)
{
    if (!handle) return;
    if (!data_->watches.isValid(handle)) return;

    auto &entry = data_->watches[handle];
    entry.consumerAttached = false;
    entry.waitingConsumer = nullptr;
    if (entry.pendingRemoval) {
        entry.queuedEvents.clear();
        data_->watches.release(handle);
    }
}

void FileWatchManager::ensureConsumer(FileWatchHandle handle)
{
    if (!handle) return;
    CO_CORE_ASSERT(data_->watches.isValid(handle),
                   "Attempted to attach consumer for invalid FileWatchHandle.");
    auto &entry = data_->watches[handle];
    if (!entry.consumerAttached) {
        entry.consumerAttached = true;
    }
}

std::optional<FileWatchEventType> FileWatchManager::tryConsumeQueuedEvent(FileWatchHandle handle)
{
    if (!handle) return std::nullopt;
    if (!data_->watches.isValid(handle)) return std::nullopt;

    auto &entry = data_->watches[handle];
    if (entry.queuedEvents.empty()) return std::nullopt;

    auto event = entry.queuedEvents.front();
    entry.queuedEvents.pop_front();
    finalizeConsumedEvent(handle, event);
    return event;
}

void FileWatchManager::finalizeConsumedEvent(FileWatchHandle handle, FileWatchEventType event)
{
    if (!data_->watches.isValid(handle)) return;

    if (event == FileWatchEventType::WatchEnded) {
        detachConsumer(handle);
    }
}

bool FileWatchManager::suspendConsumer(FileWatchHandle handle,
                                       cppcoro::coroutine_handle<> coroutine)
{
    if (!handle || !coroutine) return false;
    if (!data_->watches.isValid(handle)) return false;

    auto &entry = data_->watches[handle];
    CO_CORE_ASSERT(entry.waitingConsumer == nullptr,
                   "Multiple coroutines attempted to await the same FileWatchHandle.");
    entry.waitingConsumer = coroutine;
    return true;
}

void FileWatchManager::cancelSuspendedConsumer(FileWatchHandle handle,
                                               cppcoro::coroutine_handle<> coroutine)
{
    if (!handle || !coroutine) return;
    if (!data_->watches.isValid(handle)) return;

    auto &entry = data_->watches[handle];
    if (entry.waitingConsumer == coroutine) {
        entry.waitingConsumer = nullptr;
        if (entry.pendingRemoval) {
            entry.queuedEvents.clear();
            data_->watches.release(handle);
        }
    }
}

} // namespace Cory
