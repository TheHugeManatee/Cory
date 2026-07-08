#include <Cory/Testing/TestUtils.hpp>

#include <catch2/catch_test_macros.hpp>

#include <KDGpu/vulkan/vulkan_resource_manager.h>

TEST_CASE("VulkanTester")
{
    Cory::testing::VulkanTester t;

    VkDebugUtilsMessengerCallbackDataEXT messageCallbackData{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
        .messageIdNumber = 1337};

    const std::string message{"Test Error message"};
    messageCallbackData.pMessage = message.c_str();

    auto ih = t.ctx().instance().handle();
    VkInstance inst = t.ctx().resources().getInstance(ih)->instance;

    auto submitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(
        inst, "vkSubmitDebugUtilsMessageEXT");
    REQUIRE(submitDebugUtilsMessageEXT != nullptr);

    submitDebugUtilsMessageEXT(inst,
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                               VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                               &messageCallbackData);

    t.ctx().device().waitUntilIdle();

    REQUIRE(t.errors().size() == 1);
    CHECK(t.errors()[0].messageType == Cory::DebugMessageType::General);
    CHECK(t.errors()[0].severity == Cory::DebugMessageSeverity::Error);
    CHECK(t.errors()[0].messageIdNumber == messageCallbackData.messageIdNumber);
    CHECK(t.errors()[0].message == message);

    t.expectMessageId(messageCallbackData.messageIdNumber);
}
