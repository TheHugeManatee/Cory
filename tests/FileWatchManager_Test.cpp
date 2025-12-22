#include <Cory/Base/FileWatchManager.hpp>

#include <Cory/Base/Log.hpp>

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

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
} // namespace

TEST_CASE("FileWatchManager: file creation, modification, deletion, and unwatching")
{
    const auto testFilePath = testDirectory() / "test_filewatch.txt";

    FileWatchManager mgr;
    // Clean up before test
    if (fs::exists(testFilePath)) fs::remove(testFilePath);

    std::promise<FileWatchEvent> createdPromise, modifiedPromise, deletedPromise;
    auto createdFuture = createdPromise.get_future();
    auto modifiedFuture = modifiedPromise.get_future();
    auto deletedFuture = deletedPromise.get_future();
    std::atomic<int> callbackCount{0};
    std::atomic<bool> createdSet{false}, modifiedSet{false}, deletedSet{false};
    auto handle = mgr.watch({testFilePath.string()}, [&](FileWatchEvent event) {
        callbackCount++;
        switch (event) {
        case FileWatchEvent::Created:
            if (!createdSet.exchange(true)) createdPromise.set_value(event);
            break;
        case FileWatchEvent::Modified:
            if (!modifiedSet.exchange(true)) modifiedPromise.set_value(event);
            break;
        case FileWatchEvent::Deleted:
            if (!deletedSet.exchange(true)) deletedPromise.set_value(event);
            break;
        default:
            break;
        }
    });

    REQUIRE(handle);

    // Test file creation
    createTestFile(testFilePath);
    auto createdWait = createdFuture.wait_for(std::chrono::seconds(4));
    if (createdWait != std::future_status::ready) FAIL("File creation event was not received in time");
    CHECK(createdFuture.get() == FileWatchEvent::Created);
    callbackCount = 0;

    // Test file modification
    modifyTestFile(testFilePath);
    auto modifiedWait = modifiedFuture.wait_for(std::chrono::seconds(4));
    if (modifiedWait != std::future_status::ready) FAIL("File modification event was not received in time");
    CHECK(modifiedFuture.get() == FileWatchEvent::Modified);
    callbackCount = 0;

    // Test file deletion
    deleteTestFile(testFilePath);
    auto deletedWait = deletedFuture.wait_for(std::chrono::seconds(4));
    if (deletedWait != std::future_status::ready) FAIL("File deletion event was not received in time");
    CHECK(deletedFuture.get() == FileWatchEvent::Deleted);
    callbackCount = 0;

    // Test unwatching
    bool unwatchResult = mgr.unwatch(handle);
    CHECK(unwatchResult == true);
    // Modify file again, should not receive callback
    createTestFile(testFilePath);
    // Wait a short time to ensure no callback is received
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(callbackCount == 0);

    // Clean up
    if (fs::exists(testFilePath)) fs::remove(testFilePath);
}
