
#include <Cory/Log.h>
#include <Cory/vk/test_utils.h>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int main(int argc, char **argv)
{
    Cory::Log::Init();
    Cory::Log::SetCoreLevel(spdlog::level::warn);
    cory::vk::test_init();

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int res = context.run(); 

    return res;
}