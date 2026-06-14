#include <Cory/Base/FileWatchManager.hpp>

#include <Cory/Base/Log.hpp>
#include <Cory/Coro/Coro.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace Cory;
namespace fs = std::filesystem;

namespace {
fs::path testDirectory()
{
    auto base = fs::temp_directory_path() / "cory_filewatch_tests";
    fs::create_directories(base);
    return base;
}

void createTestFile(const fs::path &path)
{
    CO_CORE_INFO("Creating test file: {}", path.string());
    std::ofstream ofs(path);
    ofs << "initial content";
}

void modifyTestFile(const fs::path &path)
{
    CO_CORE_INFO("Modifying test file: {}", path.string());
    std::ofstream ofs(path, std::ios::app);
    ofs << "\nmodified";
}

void deleteTestFile(const fs::path &path)
{
    CO_CORE_INFO("Deleting test file: {}", path.string());
    std::error_code ec;
    fs::remove(path, ec);
}

struct AwaitableConsumer {
    AwaitableConsumer(FileWatchManager &manager, FileWatchHandle watchHandle)
        : mgr(manager)
        , handle(watchHandle)
        , consumerTask{consume()}
    {
    }

    bool waitForEvents(std::size_t count,
                       std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (events.size() < count) {
            mgr.processPendingEvents();
            if (events.size() >= count) return true;
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    std::vector<FileWatchEventType> takeEvents()
    {
        auto copy = events;
        events.clear();
        return copy;
    }

    bool waitUntilFinished(std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!finished) {
            mgr.processPendingEvents();
            if (finished) return true;
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    FileWatchManager &mgr;
    FileWatchHandle handle;
    std::vector<FileWatchEventType> events;
    bool finished{false};

  private:
    auto consume() -> EagerJob
    {
        while (true) {
            auto event = co_await mgr.nextEvent(handle);
            events.push_back(event);
            if (event == FileWatchEventType::WatchEnded) break;
        }
        finished = true;
    }
    EagerJob consumerTask;
};

std::vector<FileWatchEventType> compressModifiedEvents(std::vector<FileWatchEventType> events)
{
    std::vector<FileWatchEventType> compressed;
    compressed.reserve(events.size());
    for (auto event : events) {
        if (!compressed.empty() && event == FileWatchEventType::Modified &&
            compressed.back() == FileWatchEventType::Modified) {
            continue;
        }
        compressed.push_back(event);
    }
    return compressed;
}
} // namespace

TEST_CASE("FileWatchManager: file creation, modification, deletion, and unwatching")
{
    using namespace std::chrono_literals;
    const auto testFilePath = testDirectory() / "test_filewatch.txt";
    const auto secondFilePath = testDirectory() / "test_filewatch_2.txt";

    FileWatchManager mgr;
    if (fs::exists(testFilePath)) fs::remove(testFilePath);
    if (fs::exists(secondFilePath)) fs::remove(secondFilePath);

    const auto handle = mgr.watch({testFilePath.string()});
    REQUIRE(handle);
    AwaitableConsumer consumer{mgr, handle};

    createTestFile(testFilePath);
    REQUIRE(consumer.waitForEvents(2));
    CHECK(compressModifiedEvents(consumer.takeEvents()) ==
          std::vector{FileWatchEventType::Created, FileWatchEventType::Modified});

    modifyTestFile(testFilePath);
    REQUIRE(consumer.waitForEvents(1));
    CHECK(compressModifiedEvents(consumer.takeEvents()) ==
          std::vector{FileWatchEventType::Modified});

    deleteTestFile(testFilePath);
    REQUIRE(consumer.waitForEvents(1));
    CHECK(consumer.takeEvents() == std::vector{FileWatchEventType::Deleted});

    CHECK(mgr.unwatch(handle));
    REQUIRE(consumer.waitForEvents(1));
    CHECK(consumer.takeEvents() == std::vector{FileWatchEventType::WatchEnded});
    CHECK(consumer.waitUntilFinished());

    if (fs::exists(testFilePath)) fs::remove(testFilePath);
    if (fs::exists(secondFilePath)) fs::remove(secondFilePath);
}

TEST_CASE("FileWatchManager: consumer can exit before sentinel is consumed")
{
    using namespace std::chrono_literals;
    const auto path = testDirectory() / "test_filewatch_early_exit.txt";
    if (fs::exists(path)) fs::remove(path);

    FileWatchManager mgr;
    const auto handle = mgr.watch({path.string()});
    REQUIRE(handle);

    bool receivedFirstEvent = false;
    auto consumer = [&]() -> EagerJob {
        auto event = co_await mgr.nextEvent(handle);
        (void)event;
        receivedFirstEvent = true;
        mgr.detach(handle);
    }();

    createTestFile(path);
    std::this_thread::sleep_for(50ms);

    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!receivedFirstEvent) {
        mgr.processPendingEvents();
        if (receivedFirstEvent) break;
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(1ms);
    }

    CHECK(mgr.unwatch(handle));
    mgr.processPendingEvents(); // Should safely drain pending events without an active consumer.

    if (fs::exists(path)) fs::remove(path);
}

TEST_CASE("FileWatchManager: sentinel is emitted even without file activity")
{
    FileWatchManager mgr;
    const auto path = testDirectory() / "test_filewatch_sentinel.txt";
    if (fs::exists(path)) fs::remove(path);

    const auto handle = mgr.watch({path.string()});
    REQUIRE(handle);
    AwaitableConsumer consumer{mgr, handle};

    CHECK(mgr.unwatch(handle));
    REQUIRE(consumer.waitForEvents(1));
    CHECK(consumer.takeEvents() == std::vector{FileWatchEventType::WatchEnded});
    CHECK(consumer.waitUntilFinished());
}

TEST_CASE("FileWatchManager: can watch multiple files in the same directory")
{
    using namespace std::chrono_literals;
    const auto fileAPath = testDirectory() / "test_filewatch_multi_a.txt";
    const auto fileBPath = testDirectory() / "test_filewatch_multi_b.txt";
    if (fs::exists(fileAPath)) fs::remove(fileAPath);
    if (fs::exists(fileBPath)) fs::remove(fileBPath);

    FileWatchManager mgr;
    const auto handleA = mgr.watch({fileAPath.string()});
    const auto handleB = mgr.watch({fileBPath.string()});
    REQUIRE(handleA);
    REQUIRE(handleB);

    AwaitableConsumer consumerA{mgr, handleA};
    AwaitableConsumer consumerB{mgr, handleB};

    createTestFile(fileAPath);
    REQUIRE(consumerA.waitForEvents(2));
    CHECK(compressModifiedEvents(consumerA.takeEvents()) ==
          std::vector{FileWatchEventType::Created, FileWatchEventType::Modified});

    std::this_thread::sleep_for(50ms);
    mgr.processPendingEvents();
    CHECK(consumerB.takeEvents().empty());

    createTestFile(fileBPath);
    REQUIRE(consumerB.waitForEvents(2));
    CHECK(compressModifiedEvents(consumerB.takeEvents()) ==
          std::vector{FileWatchEventType::Created, FileWatchEventType::Modified});

    CHECK(mgr.unwatch(handleA));
    CHECK(mgr.unwatch(handleB));
    REQUIRE(consumerA.waitForEvents(1));
    REQUIRE(consumerB.waitForEvents(1));
    CHECK(consumerA.takeEvents() == std::vector{FileWatchEventType::WatchEnded});
    CHECK(consumerB.takeEvents() == std::vector{FileWatchEventType::WatchEnded});
    CHECK(consumerA.waitUntilFinished());
    CHECK(consumerB.waitUntilFinished());

    if (fs::exists(fileAPath)) fs::remove(fileAPath);
    if (fs::exists(fileBPath)) fs::remove(fileBPath);
}
