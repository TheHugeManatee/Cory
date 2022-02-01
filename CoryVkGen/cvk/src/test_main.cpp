
#include <cvk/log.h>
#include <cvk/test_utils.h>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int main(int argc, char **argv)
{
    cvk::log::init();
    cvk::log::set_level(spdlog::level::warn);
    cvk::test_init();

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int res = context.run(); 

    return res;
}