#pragma once

#include <Cory/Renderer/Common.hpp>

#include <Cory/Renderer/Context.hpp>

#include <memory>

namespace Cory::testing {

class VulkanTester {
  public:
    VulkanTester();
    ~VulkanTester();

    Context& ctx();

  private:
    std::unique_ptr<struct VulkanTestContextPrivate> data_;
};

} // namespace Cory::testing