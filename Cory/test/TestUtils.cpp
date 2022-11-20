#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/Instance.h>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/equal.hpp>

#include <mutex>
#include <vector>

namespace Cory::testing {

Context &getTestContext()
{
    static Context testContext;
    return testContext;
}

struct VulkanTestContextPrivate {
    Context &ctx{getTestContext()};
    std::mutex debugMessagesMtx;
    std::vector<DebugMessageInfo> debugMessages;
    std::vector<int32_t> expectedMessages;
};

VulkanTester::VulkanTester()
    : data_{std::make_unique<VulkanTestContextPrivate>()}
{
    data_->ctx.onVulkanDebugMessageReceived([data = data_.get()](DebugMessageInfo info) {
        if (info.severity == Cory::DebugMessageSeverity::Error) {
            std::lock_guard lck{data->debugMessagesMtx};
            data->debugMessages.push_back(info);
        }
    });
}
VulkanTester::~VulkanTester()
{
    // ensure all commands have finished
    data_->ctx.device()->DeviceWaitIdle(data_->ctx.device());
    // clear vulkan debug callback
    data_->ctx.onVulkanDebugMessageReceived([](auto) {});

    // remove all expected messages
    if (!ranges::equal(data_->debugMessages,
                       data_->expectedMessages,
                       {},
                       &DebugMessageInfo::messageIdNumber)) {

        std::string messages;
        for (const auto &msgInfo : data_->debugMessages) {
            messages += fmt::format("   * {}: {}\n", msgInfo.messageIdNumber, msgInfo.message);
        }

        CO_CORE_ERROR("*** VulkanTester message validation check failed: ***\n"
                      "  Expected error messages IDs: [{}]\n"
                      "  Received {} debug message(s):\n"
                      "{}",
                      fmt::join(data_->expectedMessages, ","),
                      data_->debugMessages.size(),
                      messages);

        FAIL("There were unexpected vulkan validation errors or expected messages did not occur!");
    }
}

Context &VulkanTester::ctx() { return data_->ctx; }

const std::vector<DebugMessageInfo> &VulkanTester::errors() { return data_->debugMessages; }

void VulkanTester::expectMessageId(int32_t messageIdNumber)
{
    data_->expectedMessages.push_back(messageIdNumber);
}

} // namespace Cory::testing

TEST_CASE("VulkanTester")
{
    Cory::testing::VulkanTester t;

    VkDebugUtilsMessengerCallbackDataEXT messageCallbackData{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
        .messageIdNumber = 1337};

    const std::string message{"Test Error message"};
    messageCallbackData.pMessage = message.c_str();
    t.ctx().instance()->SubmitDebugUtilsMessageEXT(t.ctx().instance(),
                                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                                   VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                                   &messageCallbackData);

    REQUIRE(t.errors().size() == 1);
    CHECK(t.errors()[0].messageType == Cory::DebugMessageType::General);
    CHECK(t.errors()[0].severity == Cory::DebugMessageSeverity::Error);
    CHECK(t.errors()[0].messageIdNumber == messageCallbackData.messageIdNumber);
    CHECK(t.errors()[0].message == message);

    t.expectMessageId(messageCallbackData.messageIdNumber);
}