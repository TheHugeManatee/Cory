#include <cvk/instance.h>

#include "test_utils.h"

#include <doctest/doctest.h>


TEST_CASE("Creating the test instance is successful")
{
    auto inst = cvkt::test_instance();
    CHECK(inst.get() != nullptr);
    CHECK(cvkt::debug_message_count() == 0);
}