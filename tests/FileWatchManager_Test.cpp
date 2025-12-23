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
    using namespace std::chrono_literals;
    const auto testFilePath = testDirectory() / "test_filewatch.txt";
    const auto secondFilePath = testDirectory() / "test_filewatch_2.txt";

    FileWatchManager mgr;
    // Clean up before test
    if (fs::exists(testFilePath)) fs::remove(testFilePath);
    if (fs::exists(secondFilePath)) fs::remove(secondFilePath);

    std::vector<FileWatchEventType> receivedEvents;

    auto handle = mgr.watch({testFilePath.string()},
                            [&](FileWatchEventType event) { receivedEvents.push_back(event); });

    REQUIRE(handle);

    // Test file creation
    createTestFile(testFilePath);
    REQUIRE(receivedEvents.size() == 0);
    std::this_thread::sleep_for(20ms);
    mgr.processPendingEvents();
    REQUIRE(!receivedEvents.empty());
    CHECK(receivedEvents == std::vector{FileWatchEventType::Created, FileWatchEventType::Modified});
    receivedEvents.clear();

    // Test file modification
    modifyTestFile(testFilePath);
    std::this_thread::sleep_for(20ms);
    REQUIRE(receivedEvents.size() == 0);
    mgr.processPendingEvents();
    REQUIRE(receivedEvents.size() == 1);
    CHECK(receivedEvents.back() == FileWatchEventType::Modified);
    receivedEvents.clear();

    // Test file deletion
    deleteTestFile(testFilePath);
    std::this_thread::sleep_for(20ms);
    REQUIRE(receivedEvents.size() == 0);
    mgr.processPendingEvents();
    REQUIRE(receivedEvents.size() == 1);
    CHECK(receivedEvents.back() == FileWatchEventType::Deleted);
    receivedEvents.clear();

    // Test unwatching
    bool unwatchResult = mgr.unwatch(handle);
    CHECK(unwatchResult == true);
    // Modify file again, should not receive callback
    createTestFile(testFilePath);
    REQUIRE(receivedEvents.size() == 0);
    mgr.processPendingEvents();
    REQUIRE(receivedEvents.size() == 0);

    // Clean up
    if (fs::exists(testFilePath)) fs::remove(testFilePath);
    if (fs::exists(secondFilePath)) fs::remove(secondFilePath);
}
