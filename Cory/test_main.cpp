#define DOCTEST_CONFIG_IMPLEMENT

#include <Cory/Log.h>
#include <Cory/vk/test_utils.h>

#include <doctest/doctest.h>

int main(int argc, char **argv)
{
    Cory::Log::Init();
    Cory::Log::SetCoreLevel(spdlog::level::warn);
    cory::vk::test_init();

    doctest::Context context;

    // !!! THIS IS JUST AN EXAMPLE SHOWING HOW DEFAULTS/OVERRIDES ARE SET !!!

    // defaults
    //     context.addFilter("test-case-exclude",
    //                       "*math*");           // exclude test cases with "math" in their name
    //     context.setOption("abort-after", 5);   // stop test execution after 5 failed assertions
    //     context.setOption("order-by", "name"); // sort the test cases by their name

    context.applyCommandLine(argc, argv);

    // overrides
    // context.setOption("no-breaks", true); // don't break in the debugger when assertions fail

    int res = context.run(); // run

    return res;
}