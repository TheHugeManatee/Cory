#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/FutureFrameQueue.hpp>

#include <string>

using namespace Cory;

TEST_CASE("FutureFrameQueue")
{
    FutureFrameQueue<int, std::string> queue;

    CHECK(queue.size() == 0);

    queue.enqueueFor(1, "one");
    queue.enqueueFor(1, "One");
    queue.enqueueFor(1, "oNe");
    queue.enqueueFor(1, "one");
    queue.enqueueFor(2, "two");
    queue.enqueueFor(3, "three");
    queue.enqueueFor(99, "ninety-nine");
    CHECK(queue.size() == 7);

    std::vector<std::string> processed = queue.dequeueUntil(1);
    REQUIRE(processed.size() == 4);
    CHECK(processed[0] == "one");
    CHECK(processed[1] == "One");
    CHECK(processed[2] == "oNe");
    CHECK(processed[3] == "one");

    processed = queue.dequeueUntil(3);
    REQUIRE(processed.size() == 2);
    CHECK(processed[0] == "two");
    CHECK(processed[1] == "three");

    processed = queue.dequeueUntil(10);
    REQUIRE(processed.size() == 0);

    processed = queue.dequeueAll();
    CHECK(queue.size() == 0);
    REQUIRE(processed.size() == 1);
    CHECK(processed[0] == "ninety-nine");
}