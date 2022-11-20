#include "TestUtils.hpp"

#include <Cory/Renderer/Context.hpp>

namespace Cory::testing {

struct VulkanTestContextPrivate {
    Context ctx;
};

VulkanTester::VulkanTester()
    : data_{std::make_unique<VulkanTestContextPrivate>()}
{
    data_->ctx.onVulkanDebugMessageReceived([](DebugMessageInfo info) {
        if (info.severity == Cory::DebugMessageSeverity::Warning ||
            info.severity == Cory::DebugMessageSeverity::Error)
            throw std::runtime_error{info.message};
    });
}
VulkanTester::~VulkanTester() = default;
Context &VulkanTester::ctx() { return data_->ctx; }

} // namespace Cory::testing