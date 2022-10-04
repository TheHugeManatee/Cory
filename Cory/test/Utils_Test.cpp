#include <catch2/catch.hpp>

#include <Cory/Base/Utils.hpp>
#include <Cory/RenderCore/CpuBuffer.hpp>

TEST_CASE("formatBytes", "[Cory/Utils]")
{
    CHECK(Cory::formatBytes(42) == "42 B");
    CHECK(Cory::formatBytes(1024) == "1 KiB");
    CHECK(Cory::formatBytes(1024 * 1024) == "1 MiB");
    CHECK(Cory::formatBytes(512 * 1024 * 1024) == "512 MiB");
    CHECK(Cory::formatBytes(1024 * 1024 * 1024) == "1 GiB");
    CHECK(Cory::formatBytes(1536 * 1024 * 1024) == "1.50 GiB");
}
