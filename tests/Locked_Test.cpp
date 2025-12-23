#include <catch2/catch_test_macros.hpp>

#include <Cory/Base/Locked.hpp>

struct MyData {
    int value;
};

TEST_CASE("Locked<T>")
{
    Cory::Locked<MyData> lockedData{MyData{42}};
    {
        auto data = lockedData.lock();
        REQUIRE(data->value == 42);
        data->value = 100;
    }
    {
        auto data = lockedData.lock();
        REQUIRE(data->value == 100);
    }

    const Cory::Locked<MyData> constLockedData{MyData{55}};
    {
        auto data = constLockedData.lock();
        REQUIRE(data->value == 55);
        static_assert(std::is_same_v<decltype(*data), const MyData &>,
                      "Data access should be const");
        // data->value = 200; // should fail to compile if uncommented
    }
}