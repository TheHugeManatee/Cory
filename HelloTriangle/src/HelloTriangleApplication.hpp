#pragma once

#include <Cory/UI/Application.hpp>
#include <Cory/UI/Window.hpp>

class HelloTriangleApplication : public Cory::Application {
  public:
    HelloTriangleApplication();

    void run() override;

  private:
    Cory::Window window_;
};
