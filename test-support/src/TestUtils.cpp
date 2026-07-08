#include <Cory/Testing/TestUtils.hpp>

#include <Cory/Base/Debugger.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm/equal.hpp>

#include <mutex>
#include <vector>

namespace Cory::testing {

Context &getTestContext()
{
    static Context testContext = [] {
        Context ctx{ContextCreationInfo{.validation = ValidationLayers::Enabled}};
        testContext.setupHeadlessDevice();
        return ctx;
    }();
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
    data_->ctx.onVulkanDebugMessageReceived([data = data_.get()](const DebugMessageInfo &info) {
        if (info.severity == DebugMessageSeverity::Error) {
            std::lock_guard lck{data->debugMessagesMtx};
            data->debugMessages.push_back(info);
            BreakpointIfDebugging();
        }
    });
}
VulkanTester::~VulkanTester()
{
    data_->ctx.device().waitUntilIdle();
    data_->ctx.onVulkanDebugMessageReceived([](auto) {});

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

Context &VulkanTester::ctx()
{
    return data_->ctx;
}

const std::vector<DebugMessageInfo> &VulkanTester::errors() const
{
    return data_->debugMessages;
}

void VulkanTester::expectMessageId(int32_t messageIdNumber)
{
    data_->expectedMessages.push_back(messageIdNumber);
}

} // namespace Cory::testing
