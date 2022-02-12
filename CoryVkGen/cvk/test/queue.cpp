#include <cvk/queue.h>

#include <cvk/context.h>

#include "test_utils.h"

#include <doctest/doctest.h>

TEST_SUITE_BEGIN("cvk::queue");

TEST_CASE("basic queue usage")
{
    cvk::context ctx = cvkt::test_context();

    // record and submit directly to a queue
//    cvk::queue &compute_q = ctx.compute_queue();
//    auto cmdbuf1 = compute_q.record([](command_buffer &) {});
//    cmdbuf1.submit();

    // record through the context
//    auto cmdbuf2 = ctx.record(
//        [](command_buffer &buf) {
//            // buf.blit_image({}, {}, {}, {}, {}, {}, {});
//            // buf.copy_buffer({}, {}, {}, {});
//        },
//        ctx.compute_queue());
//    auto executed = cmdbuf2.submit();

    //// variant 2 - directly on queue
    // auto buffer2 = ctx.graphics_queue().record([](command_buffer &buf) {
    //    buf.blit_image({}, {}, {}, {}, {}, {}, {});
    //    buf.copy_buffer({}, {}, {}, {});
    //});
    // ctx.graphics_queue().submit(buffer2);

    //// variant 3 - overloads
    // auto buffer3 = ctx.record_graphics(...);

    //// variant 4 - template
    // ctx.record<graphics_queue>()...;

    // wait for the GPU to finish (processing an empty command buffer, har har)
    //executed.get();
}

TEST_SUITE_END();