#pragma once

#include "Common.hpp"
#include "Function.hpp"

#include <memory>
#include <string>

namespace Cory {

struct FileWatch {
    std::string path;
};

/// The different events that we can detect
enum class FileWatchEvent { Unknown, Created, Deleted, Modified, Renamed };

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
    FileWatchHandle watch(FileWatch watch, Function<void(FileWatchEvent event)> callback);

    /// Stop watching
    bool unwatch(FileWatchHandle handle);

  private:
    static std::unique_ptr<FileWatchManager> s_instance;

    struct Private;
    std::unique_ptr<Private> data_;
};
} // namespace Cory
