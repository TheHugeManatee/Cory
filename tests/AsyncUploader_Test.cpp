#include <Cory/Renderer/AsyncUploader.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("AsyncUploader queue policy resolves dedicated transfer usage")
{
    CHECK(Cory::AsyncUploader::hasDedicatedTransferQueue(1, 0));
    CHECK_FALSE(Cory::AsyncUploader::hasDedicatedTransferQueue(0, 0));
}

TEST_CASE("AsyncUploader queue family transfer detection")
{
    CHECK(Cory::AsyncUploader::needsQueueFamilyTransfer(2, 1));
    CHECK_FALSE(Cory::AsyncUploader::needsQueueFamilyTransfer(1, 1));
}

TEST_CASE("AsyncUploader consumer queue family defaults to graphics")
{
    constexpr auto graphicsFamily = uint32_t{3};
    CHECK(Cory::AsyncUploader::resolveConsumerQueueFamily(Cory::AsyncUploader::DefaultQueueFamily,
                                                          graphicsFamily) == graphicsFamily);
    CHECK(Cory::AsyncUploader::resolveConsumerQueueFamily(5, graphicsFamily) == 5);
}

TEST_CASE("UploadTicket default completion behavior")
{
    Cory::AsyncUploader::UploadTicket ticket;
    CHECK(ticket.ready());
    ticket.wait();
    CHECK(ticket.ready());
}
