#pragma once

#include <Cory/Application/Common.hpp>
#include <Cory/Base/Common.hpp>
#include <Cory/Renderer/Common.hpp>

#include <memory>

namespace Cory {

class Application : NoCopy, NoMove {
  public:
    Application();
    virtual ~Application();

    virtual void run() = 0;

    void init(const ContextCreationInfo &info);

    Context& ctx();
    const Context& ctx() const;

    LayerStack &layers();
    const LayerStack &layers() const;

  private:
    std::unique_ptr<struct ApplicationPrivate> data_;
};

} // namespace Cory
