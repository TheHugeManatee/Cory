#pragma once

#include <Cory/UI/Application.hpp>

#include <memory>

class TrianglePipeline;
namespace Cory {
class Window;
}

class HelloTriangleApplication : public Cory::Application {
  public:
    HelloTriangleApplication();
    ~HelloTriangleApplication();

    void run() override;

  private:
    std::unique_ptr<Cory::Window> window_;
    std::unique_ptr<TrianglePipeline> pipeline_;
};
