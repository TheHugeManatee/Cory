#pragma once

//namespace cory {
//namespace vk {
//
//template <typename Resource> class resource_pool {
//  public:
//    static Resource get();
//    static void recycle(Resource &&);
//};
//
//} // namespace vk
//} // namespace cory
//
//#ifndef DOCTEST_CONFIG_DISABLE
//#include "test_utils.h"
//#include <doctest/doctest.h>
//
//TEST_CASE("pool management")
//{
//    using namespace cory::vk;
//    // pool tester class - simply counts the ids
//    static int tester_count{};
//    struct pool_tester {
//        pool_tester()
//            : id{tester_count++}
//        {
//        }
//        int id;
//    };
//    CHECK(tester_count ==0);
//    pool_tester p1 = resource_pool<pool_tester>::get();
//    CHECK(tester_count == 1);
//    pool_tester p2 = resource_pool<pool_tester>::get();
//
//    CHECK(tester_count == 2);
//
//    resource_pool<pool_tester>::recycle(std::move(p1));
//
//    CHECK(tester_count == 2);
//
//    // get another - should yield the same as p1
//    pool_tester p3 = resource_pool<pool_tester>::get();
//
//    CHECK(tester_count == 2);
//}

//#endif