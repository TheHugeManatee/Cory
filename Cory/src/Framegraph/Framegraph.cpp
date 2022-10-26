#include <Cory/Framegraph/Framegraph.hpp>

#include <range/v3/algorithm/reverse.hpp>
#include <range/v3/algorithm/transform.hpp>

#include <deque>
#include <unordered_map>

namespace Cory::Framegraph {

Builder Framegraph::Framegraph::declarePass(std::string_view name)
{
    //
    return Builder{*this, name};
}

Framegraph::~Framegraph()
{
    // destroy all coroutines
    for (auto &[name, handle] : renderPasses_) {
        handle.destroy();
    }
}

void Framegraph::execute()
{
    std::vector<RenderPassHandle> passesToExecute = compile();

    for (const auto &handle : passesToExecute) {
        auto &coroHandle = renderPasses_[handle];
        CO_CORE_INFO("Executing rendering commands for {}", handle);
        if (!coroHandle.done()) { coroHandle.resume(); }
        CO_CORE_ASSERT(
            coroHandle.done(),
            "Render pass definition seems to have more unnecessary synchronization points!");
    }
}
TextureHandle Framegraph::declareInput(std::string_view name)
{
    TextureHandle handle{
        .name = std::string{name},
        // todo fill these with more parameters?
        //  also create an external resource in the resource manager?
    };
    graph_.recordCreates("[ExternalInput]", handle);
    return handle;
}

void Framegraph::declareOutput(TextureHandle handle)
{
    graph_.recordReads("[ExternalOut]", handle);
    // todo
    outputs_.push_back(handle.rsrcHandle);
}

std::vector<RenderPassHandle> Framegraph::compile()
{
    // TODO this is where we would optimize and figure out the dependencies
    CO_CORE_INFO("Framegraph optimization not implemented.");
    graph_.dump();

    auto passes = graph_.resolve(outputs_);
    return passes;
}

cppcoro::task<TextureHandle> Builder::read(TextureHandle &h)
{
    framegraph_.graph_.recordReads(passHandle_, h);

    co_return TextureHandle{
        .size = h.size, .format = h.format, .layout = h.layout, .rsrcHandle = h.rsrcHandle};
}

cppcoro::task<MutableTextureHandle>
Builder::create(std::string name, glm::u32vec3 size, PixelFormat format, Layout finalLayout)
{

    MutableTextureHandle handle{
        .name = name,
        .size = size,
        .format = format,
        .layout = finalLayout,
        .rsrcHandle = framegraph_.resources_.createTexture(name, size, format, finalLayout)};
    framegraph_.graph_.recordCreates(passHandle_, handle);
    co_return handle;
}

cppcoro::task<MutableTextureHandle> Builder::write(TextureHandle handle)
{
    framegraph_.graph_.recordWrites(passHandle_, handle);
    // todo versioning - MutableTextureHandle should point to a new resource alias (version in
    // slotmap?)
    co_return MutableTextureHandle{.name = handle.name,
                                   .size = handle.size,
                                   .format = handle.format,
                                   .layout = handle.layout,
                                   .rsrcHandle = handle.rsrcHandle};
}

RenderPassExecutionAwaiter Builder::finishDeclaration()
{
    return RenderPassExecutionAwaiter{passHandle_, framegraph_};
}

RenderInput RenderPassExecutionAwaiter::await_resume() const noexcept { return fg.renderInput(); }

void RenderPassExecutionAwaiter::await_suspend(
    cppcoro::coroutine_handle<> coroHandle) const noexcept
{
    fg.enqueueRenderPass(passHandle, coroHandle);
}

void DependencyGraph::dump()
{
    std::string out{"digraph G {\n"};

    std::set<RenderPassHandle> passes;
    std::set<std::string> textures;

    for (const auto &create : creates_) {
        fmt::format_to(
            std::back_inserter(out),
            "  \"{}\" -> \"{}\" [style=dashed,color=darkgreen,label=\"createTexture\"]\n",
            create.pass,
            create.texture.name);
        passes.insert(create.pass);
        textures.insert(create.texture.name);
    }
    for (const auto &read : reads_) {
        fmt::format_to(std::back_inserter(out),
                       "  \"{}\" -> \"{}\" [label=\"read\"]\n",
                       read.texture.name,
                       read.pass);
        passes.insert(read.pass);
        textures.insert(read.texture.name);
    }
    for (const auto &write : writes_) {
        fmt::format_to(std::back_inserter(out),
                       "  \"{}\" -> \"{}\" [label=\"write\"]\n",
                       write.pass,
                       write.texture.name);
        passes.insert(write.pass);
        textures.insert(write.texture.name);
    }
    for (const auto &pass : passes) {
        fmt::format_to(std::back_inserter(out), "  \"{}\" [shape=ellipse]\n", pass);
    }
    for (const auto &texture : textures) {
        fmt::format_to(std::back_inserter(out), "  \"{}\" [shape=rectangle]\n", texture);
    }

    out += "}\n";
    CO_CORE_INFO(out);
}

std::vector<RenderPassHandle>
DependencyGraph::resolve(const std::vector<ResourceHandle> &requestedResources) const
{
    std::unordered_map<ResourceHandle, RenderPassHandle> resourceToPass;
    for (const auto &[pass, texture] : creates_) {
        resourceToPass[texture.rsrcHandle] = pass;
    }
    for (const auto &[pass, texture] : writes_) {
        resourceToPass[texture.rsrcHandle] = pass;
    }
    std::unordered_multimap<RenderPassHandle, ResourceHandle> passInputs;
    for (const auto &[pass, texture] : reads_) {
        passInputs.insert({pass, texture.rsrcHandle});
    }

    std::vector<RenderPassHandle> passExecutionOrder;

    std::deque<ResourceHandle> requiredResources{requestedResources.cbegin(),
                                                 requestedResources.cend()};
    while (!requiredResources.empty()) {
        auto nextResource = requiredResources.front();
        requiredResources.pop_front();

        auto writingPassIt = resourceToPass.find(nextResource);
        if (writingPassIt == resourceToPass.end()) {
            CO_CORE_ERROR("Could not resolve frame dependency graph: resource '{}' is not created "
                          "by any render pass",
                          nextResource);
            return {};
        }

        RenderPassHandle writingPass = writingPassIt->second;
        CO_CORE_DEBUG(
            "Resolving resource {}: created/written by render pass {}", nextResource, writingPass);
        passExecutionOrder.push_back(writingPass);

        auto rng = passInputs.equal_range(writingPass);
        ranges::transform(
            rng.first, rng.second, std::back_inserter(requiredResources), [&](const auto &it) {
                CO_CORE_DEBUG("Requesting input resource for {}: {}", writingPass, it.second);
                return it.second;
            });
    }

    ranges::reverse(passExecutionOrder);
    return passExecutionOrder;
}
} // namespace Cory::Framegraph