#include <Cory/Framegraph/Framegraph.hpp>

#include <range/v3/action/unique.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/reverse.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

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
    for (RenderPassInfo &info : renderPasses_) {
        info.coroHandle.destroy();
    }
}

void Framegraph::execute()
{
    std::vector<RenderPassHandle> passesToExecute = compile();

    for (const auto &handle : passesToExecute) {
        const RenderPassInfo &rpInfo = renderPasses_[handle];
        auto &coroHandle = rpInfo.coroHandle;
        CO_CORE_INFO("Executing rendering commands for {}", rpInfo.name);
        if (!coroHandle.done()) { coroHandle.resume(); }
        CO_CORE_ASSERT(
            coroHandle.done(),
            "Render pass definition seems to have more unnecessary synchronization points!");
    }
}
TextureHandle Framegraph::declareInput(std::string_view name)
{
    Texture tex{
        .name = std::string{name},
        // todo fill these with more parameters?
        //  also create an external resource in the resource manager?
    };
    auto handle = resources_.registerExternal(tex);
    externalInputs_.push_back(handle);
    return TextureHandle{.name = tex.name,
                         // todo other fields
                         .external = true,
                         .rsrcHandle = handle};
}

void Framegraph::declareOutput(TextureHandle handle)
{
    // todo
    outputs_.push_back(handle.rsrcHandle);
}

std::vector<RenderPassHandle> Framegraph::compile()
{
    // TODO this is where we would optimize and figure out the dependencies
    CO_CORE_INFO("Framegraph optimization not implemented.");

    auto passes = resolve(outputs_);
    dump();
    return passes;
}

cppcoro::task<TextureHandle> Builder::read(TextureHandle &handle)
{
    inputs.push_back(handle);

    co_return TextureHandle{.size = handle.size,
                            .format = handle.format,
                            .layout = handle.layout,
                            .rsrcHandle = handle.rsrcHandle};
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
    outputs.push_back({handle, PassOutputKind::Create});
    co_return handle;
}

cppcoro::task<MutableTextureHandle> Builder::write(TextureHandle handle)
{
    // framegraph_.graph_.recordWrites(passHandle_, handle);
    outputs.push_back({handle, PassOutputKind::Write});
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
    RenderPassHandle passHandle =
        framegraph_.finishPassDeclaration(RenderPassInfo{.name = passName_,
                                                         .inputs = std::move(inputs),
                                                         .outputs = std::move(outputs),
                                                         .coroHandle = {},
                                                         .executionPriority = -1});
    return RenderPassExecutionAwaiter{passHandle, framegraph_};
}

RenderInput RenderPassExecutionAwaiter::await_resume() const noexcept { return fg.renderInput(); }

void RenderPassExecutionAwaiter::await_suspend(
    cppcoro::coroutine_handle<> coroHandle) const noexcept
{
    fg.enqueueRenderPass(passHandle, coroHandle);
}

void Framegraph::dump()
{
    std::string out{"digraph G {\nrankdir=LR;\nnode [fontsize=12,fontname=\"Courier New\"]\n"};

    std::unordered_map<TransientTextureHandle, TextureHandle> textures;
    std::set<TransientTextureHandle> realizedTextures{externalInputs_.cbegin(),
                                                      externalInputs_.cend()};

    for (const TransientTextureHandle &externalResource : externalInputs_) {
        realizedTextures.insert(externalResource);
    }

    for (const auto &passInfo : renderPasses_) {
        const std::string passCol = (!passInfo.coroHandle)            ? "red"
                                    : passInfo.executionPriority >= 0 ? "black"
                                                                      : "gray";
        fmt::format_to(std::back_inserter(out),
                       "  \"{0}\" [shape=ellipse,color={1},fontcolor={1}]\n",
                       passInfo.name,
                       passCol);

        for (const auto &inputHandle : passInfo.inputs) {
            fmt::format_to(
                std::back_inserter(out), "  \"{}\" -> \"{}\" \n", inputHandle.name, passInfo.name);
            textures[inputHandle.rsrcHandle] = inputHandle;
        }

        for (const auto &[outputHandle, kind] : passInfo.outputs) {
            const std::string color = kind == PassOutputKind::Create ? "darkgreen" : "black";
            const std::string label = kind == PassOutputKind::Create ? "creates" : "";
            fmt::format_to(std::back_inserter(out),
                           "  \"{}\" -> \"{}\" [style=dashed,color={},label=\"{}\"]\n",
                           passInfo.name,
                           outputHandle.name,
                           color,
                           label);
            textures[outputHandle.rsrcHandle] = outputHandle;
            if (passInfo.executionPriority >= 0) {
                realizedTextures.insert(outputHandle.rsrcHandle);
            }
        }
    }

    for (const auto &[rsrcHandle, texHandle] : textures) {
        const std::string color = ranges::contains(externalInputs_, rsrcHandle)    ? "blue"
                                  : ranges::contains(realizedTextures, rsrcHandle) ? "black"
                                                                                   : "gray";
        const std::string label = fmt::format("{}{}\\n[{}x{}x{} {},{}]",
                                              texHandle.name,
                                              texHandle.external ? " (ext)" : "",
                                              texHandle.size.x,
                                              texHandle.size.y,
                                              texHandle.size.z,
                                              texHandle.format,
                                              texHandle.layout);
        const float penWidth =
            ranges::contains(outputs_, rsrcHandle) || ranges::contains(externalInputs_, rsrcHandle)
                ? 3.0f
                : 1.0f;
        fmt::format_to(
            std::back_inserter(out),
            "  \"{0}\" [shape=rectangle,label=\"{1}\",color={2},fontcolor={2},penwidth={3}]\n",
            texHandle.name,
            label,
            color,
            penWidth);
    }

    out += "}\n";
    CO_CORE_INFO(out);
}

std::vector<RenderPassHandle>
Framegraph::resolve(const std::vector<TransientTextureHandle> &requestedResources)
{
    // counter to assign render passes an increasing execution priority - passes
    // with higher priority should be executed earlier
    int32_t executionPrio{-1};

    // first, reorder the information into a more convenient graph representation
    // essentially, in- and out-edges
    std::unordered_map<TransientTextureHandle, RenderPassHandle> resourceToPass;
    std::unordered_multimap<RenderPassHandle, TransientTextureHandle> passInputs;
    std::unordered_map<TransientTextureHandle, TextureHandle> textures;
    for (const auto &[passHandle, passInfo] : renderPasses_.items()) {
        for (const auto &inputHandle : passInfo.inputs) {
            passInputs.insert({passHandle, inputHandle.rsrcHandle});
            textures[inputHandle.rsrcHandle] = inputHandle;
        }
        for (const auto &[outputHandle, kind] : passInfo.outputs) {
            resourceToPass[outputHandle.rsrcHandle] = passHandle;
            textures[outputHandle.rsrcHandle] = outputHandle;
        }
    }

    // flood-fill the graph starting at the resources requested from the outside
    std::deque<TransientTextureHandle> requiredResources{requestedResources.cbegin(),
                                                         requestedResources.cend()};
    while (!requiredResources.empty()) {
        auto nextResource = requiredResources.front();
        requiredResources.pop_front();

        // if resource is external, we don't have to resolve it
        if (ranges::contains(externalInputs_, nextResource)) { continue; }

        // determine the pass that writes/creates the resource
        auto writingPassIt = resourceToPass.find(nextResource);
        if (writingPassIt == resourceToPass.end()) {
            CO_CORE_ERROR(
                "Could not resolve frame dependency graph: resource '{}' ({}) is not created "
                "by any render pass",
                textures[nextResource].name,
                nextResource);
            return {};
        }

        RenderPassHandle writingPass = writingPassIt->second;
        CO_CORE_DEBUG(
            "Resolving resource {}: created/written by render pass {}", nextResource, writingPass);
        renderPasses_[writingPass].executionPriority = ++executionPrio;

        // enqueue the inputs of the pass for resolve
        auto rng = passInputs.equal_range(writingPass);
        ranges::transform(
            rng.first, rng.second, std::back_inserter(requiredResources), [&](const auto &it) {
                CO_CORE_DEBUG("Requesting input resource for {}: {}", writingPass, it.second);
                return it.second;
            });
    }

    auto items = renderPasses_.items();
    auto passesToExecute =
        items | ranges::views::transform([](const auto &it) {
            return std::make_pair(RenderPassHandle{it.first}, it.second.executionPriority);
        }) |
        ranges::views::filter([](const auto &it) { return it.second >= 0; }) |
        ranges::to<std::vector>;

    std::ranges::sort(passesToExecute, {}, [](const auto &it) { return it.second; });

    CO_APP_DEBUG("Render pass order after resolve:");
    for (const auto &[handle, prio] : passesToExecute) {
        CO_APP_DEBUG("  [{}] {}", prio, renderPasses_[handle].name);
    }

    return passesToExecute | ranges::views::transform([](const auto &it) { return it.first; }) |
           ranges::to<std::vector<RenderPassHandle>>;
}

} // namespace Cory::Framegraph
