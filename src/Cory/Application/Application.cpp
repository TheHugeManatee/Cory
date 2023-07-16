#include <Cory/Application/Application.hpp>

#include <Cory/Application/LayerStack.hpp>
#include <Cory/Renderer/Context.hpp>

namespace Cory {

struct ApplicationPrivate {
    Context ctx;
    LayerStack layers;

    ApplicationPrivate(ContextCreationInfo info)
        : ctx{info}
        , layers{ctx}
    {
    }
};

Application::Application() {}

void Application::init(const ContextCreationInfo &info)
{
    data_ = std::make_unique<ApplicationPrivate>(info);
}

// <editor-fold desc="Accessors">
Context &Application::ctx() { return data_->ctx; }
const Context &Application::ctx() const { return data_->ctx; }
LayerStack &Application::layers() { return data_->layers; }
const LayerStack &Application::layers() const { return data_->layers; }
// </editor-fold>

Application::~Application() = default;

} // namespace Cory