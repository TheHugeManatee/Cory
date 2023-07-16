#pragma once

#include <Cory/Renderer/Common.hpp>

#include <Cory/Renderer/Context.hpp>

#include <memory>

namespace Cory::testing {

class VulkanTester {
  public:
    VulkanTester();
    ~VulkanTester();

    Context &ctx();

    const std::vector<DebugMessageInfo> &errors();

    // indicate that a test expects a specific vulkan message id
    void expectMessageId(int32_t messageIdNumber);

  private:
    std::unique_ptr<struct VulkanTestContextPrivate> data_;
};

} // namespace Cory::testing