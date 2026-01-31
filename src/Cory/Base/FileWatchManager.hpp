#pragma once

#include "Common.hpp"

#include <cppcoro/coroutine.hpp>

#include <memory>
#include <optional>
#include <string>

namespace Cory {

struct FileWatch {
    std::string path;
};

/// The different events that we can detect
enum class FileWatchEventType {
    Unknown,
    Created,
    Deleted,
    Modified,
    Renamed,
    WatchEnded ///< Sentinel emitted when a watch is shut down.
};

/// File watch manager that allows starting and stopping file watch events
class FileWatchManager : NoCopy {
  public:
    static void Init();
    static void Shutdown();
    static FileWatchManager &instance();

    FileWatchManager();
    ~FileWatchManager();

    // movable
    FileWatchManager(FileWatchManager &&rhs) noexcept;
    FileWatchManager &operator=(FileWatchManager &&rhs) noexcept;

    /// Start watching a file for changes
    /// The callback will be called on a separate thread.
    FileWatchHandle watch(FileWatch watch);

    /// Stop watching
    bool unwatch(FileWatchHandle handle);

    /// Process all pending events - this will call any suspended coroutines waiting for events
    void processPendingEvents();

    /// Await the next file watch event for the given handle. Only a single consumer is supported
    /// per handle. If the watch is unwatched, the returned event will be
    /// FileWatchEventType::WatchEnded.
    struct NextEventAwaitable;
    NextEventAwaitable nextEvent(FileWatchHandle handle);

    /// Detach the consumer associated with the given handle. Call this when you no longer
    /// intend to await events for the handle but have not unwatched it.
    void detach(FileWatchHandle handle);

  private:
    void ensureConsumer(FileWatchHandle handle);
    std::optional<FileWatchEventType> tryConsumeQueuedEvent(FileWatchHandle handle);
    void finalizeConsumedEvent(FileWatchHandle handle, FileWatchEventType event);
    bool suspendConsumer(FileWatchHandle handle, cppcoro::coroutine_handle<> coroutine);
    void cancelSuspendedConsumer(FileWatchHandle handle, cppcoro::coroutine_handle<> coroutine);
    void detachConsumer(FileWatchHandle handle);

    static FileWatchManager *s_instance;

    struct Private;
    std::unique_ptr<Private> data_;

  public:
    struct NextEventAwaitable {
        NextEventAwaitable(FileWatchManager &manager, FileWatchHandle handle);
        ~NextEventAwaitable();

        bool await_ready();
        bool await_suspend(cppcoro::coroutine_handle<> h);
        FileWatchEventType await_resume();

      private:
        FileWatchManager *manager;
        FileWatchHandle handle;
        std::optional<FileWatchEventType> cachedEvent;
        bool suspended{false};
        cppcoro::coroutine_handle<> suspendedHandle;
    };
};
} // namespace Cory
