#include <Cory/Application/ImGuiLayer.hpp>

#include <Cory/Application/Window.hpp>
#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Base/Log.hpp>
#include <Cory/Base/Utils.hpp>
// #include <Cory/Framegraph/CommandList.hpp>
// #include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Base/GlmUtils.hpp>
#include <Cory/Base/Primitives.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/Swapchain.hpp>

#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <KDGpuExample/imgui_renderer.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>

namespace Cory {

struct ImGuiLayer::Private {
    Context *ctx;
    Window *window;
    std::unique_ptr<KDGpuExample::ImGuiRenderer> imguiRenderer;
    ImGuiContext *context;
    i32vec2 windowSize;
};

ImGuiLayer::ImGuiLayer(Window &window)
    : ApplicationLayer("ImGui")
    , data_{std::make_unique<Private>()}
{
    data_->window = &window;
    data_->windowSize = window.dimensions();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    data_->context = ImGui::CreateContext();
}

ImGuiLayer::~ImGuiLayer()
{
    if (data_ != nullptr) {
        CO_CORE_WARN("ImGuiLayer::deinit() should be called before destruction of the layer!");
    }
}

void ImGuiLayer::onAttach(Context &ctx, LayerAttachInfo attachInfo)
{
    data_->ctx = &ctx;
    auto &window = *data_->window;

    data_->imguiRenderer = std::make_unique<KDGpuExample::ImGuiRenderer>(
        &ctx.device(), &ctx.graphicsQueue(), data_->context);
    data_->imguiRenderer->initialize(
        1.0f, window.samples(), window.colorFormat(), window.depthFormat());

    ImGui_ImplGlfw_InitForVulkan(window.getGlfwWindow(), true);

    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // setupCustomColors();
}

void ImGuiLayer::onDetach(Context &ctx)
{
    ImGui_ImplGlfw_Shutdown();
    // free all buffers before destroying the imgui context
    data_->imguiRenderer->cleanup();
    ImGui::DestroyContext(data_->context);
    data_.reset();
}

bool ImGuiLayer::onEvent(Event event)
{
    return std::visit(
        lambda_visitor{
            [](auto event) { return false; },
            [this](const SwapchainResizedEvent &event) {
                data_->windowSize = event.size;
                // data_->framebuffers =
                //     createFramebuffers(*data_->ctx, *data_->window, data_->renderPass);
                data_->imguiRenderer->updateScale(1.0f);
                return false;
            },
            // we just need to prevent lower layers from using the events, actual processing
            // happens in the onUpdate() method
            [](const ScrollEvent &event) { return ImGui::GetIO().WantCaptureMouse; },
            [](const MouseButtonEvent &event) { return ImGui::GetIO().WantCaptureMouse; },
            [](const MouseMovedEvent &event) { return ImGui::GetIO().WantCaptureMouse; },
        },
        event);
}

void ImGuiLayer::onUpdate(const LogicUpdateContext &updateCtx)
{
    // Nothing more to do here

    // Set frame time and display size.
    ImGuiIO &io = ImGui::GetIO();
    // io.DeltaTime = engine()->deltaTimeSeconds();
    io.DeltaTime = updateCtx.deltaTime;
    io.DisplaySize = glmu::to<ImVec2>(glm::vec2{data_->windowSize});

    ImGui::SetCurrentContext(data_->context);
    ImGui::NewFrame();
}
//
// RenderTaskDeclaration<LayerPassOutputs> ImGuiLayer::renderTask(Cory::RenderTaskBuilder builder,
//                                                                LayerPassOutputs previousLayer)
// {
//     auto [writtenColorHandle, colorInfo] =
//         builder.readWrite(previousLayer.color, Cory::Sync::AccessType::ColorAttachmentWrite);
//
//     co_yield LayerPassOutputs{.color = writtenColorHandle, .depth = previousLayer.depth};
//     Cory::RenderInput renderApi = co_await builder.finishDeclaration();
//
//     Context &ctx = *renderApi.ctx;
//     FrameContext &frameCtx = *renderApi.frameCtx;
//
//     // note - currently, we're letting imgui handle the final resolve and transition to
//     // present_layout
//     // recordFrameCommands(ctx, frameCtx.index, renderApi.cmd->handle());
// }

void ImGuiLayer::recordFrameCommands(KDGpu::RenderPassCommandRecorder *recorder,
                                     KDGpu::Extent2D extent,
                                     uint32_t inFlightIndex,
                                     KDGpu::RenderPass *currentRenderPass,
                                     int lastSubpassIndex)
{
    ImGui::Render();
    if (data_->imguiRenderer->updateGeometryBuffers(inFlightIndex)) {
        data_->imguiRenderer->recordCommands(
            recorder, extent, inFlightIndex, currentRenderPass, lastSubpassIndex);
    }
}

void ImGuiLayer::setupCustomColors()
{
    ImVec4 *colors = ImGui::GetStyle().Colors;
    // clang-format off
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.00f); // <
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.35f, 0.58f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 0.00f); // <
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    // clang-format on
}

} // namespace Cory
