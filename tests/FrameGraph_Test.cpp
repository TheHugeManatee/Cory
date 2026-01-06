#include <catch2/catch_test_macros.hpp>

#include "TestUtils.hpp"

#include <Cory/Base/FmtUtils.hpp>
#include <Cory/Framegraph/Framegraph.hpp>
#include <Cory/Framegraph/RenderTaskBuilder.hpp>
#include <Cory/Framegraph/RenderTaskDeclaration.hpp>
#include <Cory/Renderer/FrameContext.hpp>
#include <Cory/Renderer/ShaderManager.hpp>

#include <KDGpu/texture_options.h>
#include <cppcoro/fmap.hpp>
#include <range/v3/algorithm/contains.hpp>

#include <gsl/gsl>

#include <unordered_map>
#include <unordered_set>

using namespace Cory;

namespace passes {

struct DepthPassOutputs {
    TransientTextureHandle depthTexture;
    TransientBufferHandle depthBuffer;
};

RenderTaskDeclaration<DepthPassOutputs>
depthPass(Context &ctx, RenderTaskBuilder builder, glm::u32vec3 size)
{
    auto depth = builder.create("TEX_depth",
                                size,
                                TextureFormat::D24_UNORM_S8_UINT,
                                Sync::AccessType::DepthStencilAttachmentWrite);
    auto depthBuffer = builder.create("BUF_depth",
                                      256u,
                                      Gpu::BufferUsageFlagBits::StorageBufferBit,
                                      Sync::AccessType::AnyShaderWrite);
    DepthPassOutputs outputs{depth, depthBuffer};

    static ShaderHandle vertexShader = ctx.shaders().createShader(
        R"slang(
struct VSInput {
    float3 position;
    float3 texCoord;
    float4 color;
};

struct VSOutput {
    float4 position : SV_Position;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0);
    return output;
})slang",
        Gpu::ShaderStageFlagBits::VertexBit,
        "depth.vert");

    static ShaderHandle fragmentShader = ctx.shaders().createShader(
        R"slang(
struct VSOutput {
    float4 position : SV_Position;
};
struct PSOutput {
    float4 outColor : SV_Target0;
};
PSOutput main(VSOutput input) {
    PSOutput output;
    output.outColor = float4(1.0, 0.0, 0.0, 1.0);
    return output;
})slang",
        Gpu::ShaderStageFlagBits::FragmentBit,
        "depth.frag");

    auto depthPass = builder.declareRenderPass(RenderPassDeclaration{
        .name = "PASS_Depth",
        .shaders = {vertexShader, fragmentShader},
        .attachments = {},
        .depthAttachment =
            DepthStencilAttachment{
                depth,
                KDGpu::AttachmentLoadOperation::Clear,
                KDGpu::AttachmentStoreOperation::Store,
                1.0f,
            },
    });

    RenderInput render = co_await builder.finishDeclaration(outputs);
    CO_CORE_ASSERT(render.cmd != nullptr, "Uh-oh");
    auto recorder = depthPass.begin(render);
    CO_APP_INFO("[DepthPrepass] render commands executing");
    depthPass.end(std::move(recorder));
}

struct DepthDebugOut {
    TransientTextureHandle debugColor;
};
RenderTaskDeclaration<DepthDebugOut> depthDebug(Framegraph &graph,
                                                TransientTextureHandle depthInput)
{
    RenderTaskBuilder builder = graph.declareTask("TASK_DepthDebug");

    auto depthInfo = builder.read(
        depthInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto depthVis = builder.create("TEX_depthDebugVis",
                                   depthInfo.size,
                                   TextureFormat::R8G8B8A8_SRGB,
                                   Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(DepthDebugOut{depthVis});

    CO_APP_INFO("[DepthDebug] Pass render commands are executed");
}

struct NormalDebugOut {
    TransientTextureHandle debugColor;
};
RenderTaskDeclaration<NormalDebugOut> normalDebug(Framegraph &graph,
                                                  TransientTextureHandle normalInput)
{
    RenderTaskBuilder builder = graph.declareTask("TASK_NormalDebug");

    auto normalInfo = builder.read(
        normalInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto normalVis = builder.create("TEX_normalDebugVis",
                                    normalInfo.size,
                                    TextureFormat::R8G8B8A8_SRGB,
                                    Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(NormalDebugOut{normalVis});

    CO_APP_INFO("[NormalDebug] Pass render commands are executed");
}

struct DebugOut {
    TransientTextureHandle debugColor;
};
RenderTaskDeclaration<DebugOut> debugGeneral(Framegraph &graph,
                                             std::vector<TransientTextureHandle> debugTextures,
                                             gsl::index debugViewIndex)
{
    RenderTaskBuilder builder = graph.declareTask("TASK_GeneralDebug");

    auto &textureToDebug = debugTextures[debugViewIndex];
    auto dbgInfo = builder.read(
        textureToDebug, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto depthVis = builder.create("TEX_debugVis",
                                   dbgInfo.size,
                                   TextureFormat::R8G8B8A8_SRGB,
                                   Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(DebugOut{depthVis});

    CO_APP_INFO("[Debug] Pass render commands are executed");
}

struct MainOut {
    TransientTextureHandle color;
    TransientTextureHandle normal;
};
RenderTaskDeclaration<MainOut> mainPass(RenderTaskBuilder builder,
                                        TransientTextureHandle colorInput,
                                        TransientTextureHandle normalInput,
                                        TransientTextureHandle depthInput,
                                        TransientBufferHandle bufferInput)
{
    auto depthInfo = builder.read(depthInput, Sync::AccessType::DepthStencilAttachmentRead);
    auto bufferRW = builder.readWrite(bufferInput, Sync::AccessType::AnyShaderWrite);

    auto colorOut =
        colorInput
            ? builder.readWrite(colorInput, Cory::Sync::AccessType::ColorAttachmentReadWrite).first
            : builder.create("TEX_color",
                             depthInfo.size,
                             TextureFormat::R8G8B8A8_SRGB,
                             Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto normalOut = [&]() {
        if (normalInput) {
            return builder.readWrite(normalInput, Cory::Sync::AccessType::ColorAttachmentReadWrite)
                .first;
        }
        return builder.create("TEX_normal",
                              depthInfo.size,
                              TextureFormat::R8G8B8A8_UNORM,
                              Sync::AccessType::ColorAttachmentWrite);
    }();

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(MainOut{colorOut, normalOut});

    CO_APP_INFO("{} Pass render commands are executed", builder.name());
}

struct PostProcessOut {
    TransientTextureHandle color;
};
RenderTaskDeclaration<PostProcessOut> postProcess(RenderTaskBuilder builder,
                                                  TransientTextureHandle currentColorInput,
                                                  TransientTextureHandle previousColorInput)
{
    auto curColorInfo = builder.read(
        currentColorInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto prevColorInfo = builder.read(
        previousColorInput, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);

    auto color = builder.create("TEX_postprocess",
                                curColorInfo.size,
                                TextureFormat::R8G8B8A8_SRGB,
                                Sync::AccessType::ColorAttachmentWrite);

    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(PostProcessOut{color});

    CO_APP_INFO("[Postprocess] Pass render commands are executed");
}

struct TempResourceOut {
    TransientTextureHandle color;
    TransientBufferHandle scratch;
};
RenderTaskDeclaration<TempResourceOut> tempResourcePass(RenderTaskBuilder builder,
                                                        glm::u32vec3 size)
{
    auto color = builder.create("TEX_tempColor",
                                size,
                                TextureFormat::R8G8B8A8_SRGB,
                                Sync::AccessType::ColorAttachmentWrite);
    auto scratch = builder.create("BUF_tempScratch",
                                  512u,
                                  Gpu::BufferUsageFlagBits::StorageBufferBit,
                                  Sync::AccessType::AnyShaderWrite);

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(TempResourceOut{color, scratch});
    CO_APP_INFO("[TempResource] Pass render commands are executed");
}

struct ToyScatterOut {
    TransientBufferHandle keys;
    TransientBufferHandle indices;
};

RenderTaskDeclaration<TransientBufferHandle> toyInit(RenderTaskBuilder builder,
                                                     TransientBufferHandle indices)
{
    auto [writtenIndices, info] = builder.write(indices, Sync::AccessType::HostWrite);
    (void)info;
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(writtenIndices);
}

RenderTaskDeclaration<TransientBufferHandle> toyPreprocess(RenderTaskBuilder builder,
                                                           TransientBufferHandle keys)
{
    auto [writtenKeys, info] = builder.write(keys, Sync::AccessType::ComputeShaderWrite);
    (void)info;
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(writtenKeys);
}

RenderTaskDeclaration<TransientBufferHandle> toyHistogram(RenderTaskBuilder builder,
                                                          TransientBufferHandle keys,
                                                          TransientBufferHandle indices,
                                                          TransientBufferHandle histograms)
{
    builder.read(keys, Sync::AccessType::ComputeShaderReadOther);
    builder.read(indices, Sync::AccessType::ComputeShaderReadOther);
    auto [writtenHistograms, info] =
        builder.write(histograms, Sync::AccessType::ComputeShaderWrite);
    (void)info;
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(writtenHistograms);
}

RenderTaskDeclaration<TransientBufferHandle> toyScan(RenderTaskBuilder builder,
                                                     TransientBufferHandle histograms)
{
    auto [writtenHistograms, info] =
        builder.readWrite(histograms, Sync::AccessType::ComputeShaderWrite);
    (void)info;
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(writtenHistograms);
}

RenderTaskDeclaration<ToyScatterOut> toyScatter(RenderTaskBuilder builder,
                                                TransientBufferHandle keysIn,
                                                TransientBufferHandle indicesIn,
                                                TransientBufferHandle keysOut,
                                                TransientBufferHandle indicesOut,
                                                TransientBufferHandle histograms)
{
    builder.read(keysIn, Sync::AccessType::ComputeShaderReadOther);
    builder.read(indicesIn, Sync::AccessType::ComputeShaderReadOther);
    builder.read(histograms, Sync::AccessType::ComputeShaderReadOther);
    auto [writtenKeys, keysInfo] = builder.write(keysOut, Sync::AccessType::ComputeShaderWrite);
    auto [writtenIndices, indicesInfo] =
        builder.write(indicesOut, Sync::AccessType::ComputeShaderWrite);
    (void)keysInfo;
    (void)indicesInfo;

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(ToyScatterOut{writtenKeys, writtenIndices});
}

struct ToyRadixOut {
    TransientBufferHandle indices;
};

RenderTaskDeclaration<ToyRadixOut> toyRadix(RenderTaskBuilder builder)
{
    auto keysA = builder.create("BUF_ToyKeysA",
                                256u,
                                Gpu::BufferUsageFlagBits::StorageBufferBit,
                                Sync::AccessType::ComputeShaderWrite);
    auto keysB = builder.create("BUF_ToyKeysB",
                                256u,
                                Gpu::BufferUsageFlagBits::StorageBufferBit,
                                Sync::AccessType::ComputeShaderWrite);
    auto indicesA = builder.create("BUF_ToyIndicesA",
                                   256u,
                                   Gpu::BufferUsageFlagBits::StorageBufferBit,
                                   Sync::AccessType::HostWrite,
                                   Gpu::MemoryUsage::CpuToGpu);
    auto indicesB = builder.create("BUF_ToyIndicesB",
                                   256u,
                                   Gpu::BufferUsageFlagBits::StorageBufferBit,
                                   Sync::AccessType::ComputeShaderWrite);
    auto histograms = builder.create("BUF_ToyHistograms",
                                     16u * sizeof(uint32_t),
                                     Gpu::BufferUsageFlagBits::StorageBufferBit,
                                     Sync::AccessType::ComputeShaderWrite);

    auto initTask = toyInit(builder.subtask("Init"), indicesA);
    auto preprocessTask = toyPreprocess(builder.subtask("Preprocess"), keysA);

    auto keysIn = preprocessTask.output();
    auto keysOut = keysB;
    auto indicesIn = initTask.output();
    auto indicesOut = indicesB;
    auto histogramsHandle = histograms;

    for (uint32_t pass = 0; pass < 2; ++pass) {
        auto histogramTask =
            toyHistogram(builder.subtask(std::string("Histogram_") + std::to_string(pass)),
                         keysIn,
                         indicesIn,
                         histogramsHandle);
        auto scanTask = toyScan(builder.subtask(std::string("Scan_") + std::to_string(pass)),
                                histogramTask.output());
        auto scatterTask =
            toyScatter(builder.subtask(std::string("Scatter_") + std::to_string(pass)),
                       keysIn,
                       indicesIn,
                       keysOut,
                       indicesOut,
                       scanTask.output());

        auto scatterOut = scatterTask.output();
        keysOut = keysIn;
        indicesOut = indicesIn;
        keysIn = scatterOut.keys;
        indicesIn = scatterOut.indices;
        histogramsHandle = scanTask.output();
    }

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(ToyRadixOut{indicesIn});
}

RenderTaskDeclaration<TransientTextureHandle> toySink(RenderTaskBuilder builder,
                                                      TransientBufferHandle indices)
{
    builder.read(indices, Sync::AccessType::ComputeShaderReadOther);
    auto color = builder.create("TEX_ToySink",
                                {1, 1, 1},
                                TextureFormat::R8G8B8A8_SRGB,
                                Sync::AccessType::ColorAttachmentWrite);
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(color);
}

struct SharedBufferOut {
    TransientBufferHandle buffer;
};
RenderTaskDeclaration<SharedBufferOut> sharedBufferProducer(RenderTaskBuilder builder)
{
    auto buffer = builder.create("BUF_Shared",
                                 256u,
                                 Gpu::BufferUsageFlagBits::StorageBufferBit,
                                 Sync::AccessType::AnyShaderWrite);
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(SharedBufferOut{
        buffer,
    });
}

struct BufferConsumerOut {
    TransientTextureHandle color;
};
RenderTaskDeclaration<BufferConsumerOut> bufferConsumer(RenderTaskBuilder builder,
                                                        std::string_view name,
                                                        TransientBufferHandle sharedBuffer,
                                                        glm::u32vec3 size)
{
    builder.read(sharedBuffer, Sync::AccessType::ComputeShaderReadOther);
    auto color = builder.create(std::string{name},
                                size,
                                TextureFormat::R8G8B8A8_SRGB,
                                Sync::AccessType::ColorAttachmentWrite);
    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(BufferConsumerOut{color});
}

struct CombineOut {
    TransientTextureHandle color;
};
RenderTaskDeclaration<CombineOut> combineConsumers(RenderTaskBuilder builder,
                                                   TransientTextureHandle inputA,
                                                   TransientTextureHandle inputB,
                                                   glm::u32vec3 size)
{
    builder.read(inputA, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    builder.read(inputB, Sync::AccessType::FragmentShaderReadSampledImageOrUniformTexelBuffer);
    auto color = builder.create(
        "TEX_Combine", size, TextureFormat::R8G8B8A8_SRGB, Sync::AccessType::ColorAttachmentWrite);
    [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(CombineOut{color});
}
} // namespace passes

struct GraphInput {
    Texture texture;
    TextureView textureView;
    TransientTextureHandle handle;
};
GraphInput createGraphInput(Gpu::Device &device, Framegraph &graph)
{
    GraphInput input;
    // Create a 2D sRGB color texture to use as a color attachment and for sampling
    input.texture = device.createTexture(Gpu::TextureOptions{
        .label = "TEX_previousFrameColor (IMG)",
        .type = Gpu::TextureType::TextureType2D,
        .format = TextureFormat::R8G8B8A8_SRGB,
        .extent = {1024, 768, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = Gpu::SampleCountFlagBits::Samples1Bit,
        .usage =
            Gpu::TextureUsageFlagBits::ColorAttachmentBit | Gpu::TextureUsageFlagBits::SampledBit,
        .memoryUsage = Gpu::MemoryUsage::GpuOnly,
    });

    // Create a default 2D view of the texture (keeps same format)
    input.textureView = input.texture.createView(Gpu::TextureViewOptions{
        .label = "TEX_previousFrameColor (VIEW)",
        .viewType = Gpu::ViewType::ViewType2D,
        .format = TextureFormat::R8G8B8A8_SRGB,
    });

    input.handle = graph.declareInput(
        {"TEX_previousFrameColor", glm::u32vec3{1024, 768, 1}, TextureFormat::R8G8B8A8_SRGB},
        Sync::AccessType::None,
        input.texture,
        input.textureView);
    return input;
}

TEST_CASE("Framegraph API", "[Cory/Framegraph]")
{
    testing::VulkanTester t;

    Framegraph graph(t.ctx(), 0);

    auto &device = t.ctx().device();

    auto prevFrame = createGraphInput(device, graph);

    auto depthPass = passes::depthPass(t.ctx(), graph.declareTask("PASS_DepthPre"), {800, 600, 1});
    auto depthTex = depthPass.output().depthTexture;
    auto mainPass = passes::mainPass(graph.declareTask("PASS_Main"),
                                     NullHandle,
                                     NullHandle,
                                     depthTex,
                                     depthPass.output().depthBuffer);

    auto addMainPass = passes::mainPass(graph.declareTask("PASS_Main_Lines"),
                                        mainPass.output().color,
                                        mainPass.output().normal,
                                        depthTex,
                                        depthPass.output().depthBuffer);

    auto depthDebugPass = passes::depthDebug(graph, depthPass.output().depthTexture);
    auto normalDebugPass = passes::normalDebug(graph, mainPass.output().normal);
    auto debugCombinePass = passes::debugGeneral(
        graph, {depthDebugPass.output().debugColor, normalDebugPass.output().debugColor}, 0);

    auto postProcess = passes::postProcess(
        graph.declareTask("TASK_Postprocess"), addMainPass.output().color, prevFrame.handle);

    // provoke the coroutines to run so we have some stuff to cull in the graph - otherwise
    // they won't even, that's how neat using coroutines is :)
    depthDebugPass.output();
    normalDebugPass.output();
    debugCombinePass.output();

    auto postprocessOut = postProcess.output();

    auto [resultInfo, resultState] = graph.declareOutput(postprocessOut.color);

    CO_APP_INFO("Final output is a color texture of {}", resultInfo.size);

    CommandRecorder recorder = device.createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 2,
        .swapchainImageIndex = 1,
        .frameNumber = 42,
        // rest not needed for the framegraph
        .commandBuffer = std::move(recorder),
    };
    auto g = graph.record(frameCtx);
    CHECK(!g.buffers.empty());
    CHECK(!g.bufferTransitions.empty());
    CO_APP_INFO(graph.dump(g));
}

TEST_CASE("Framegraph allocates temp resources for scheduled tasks", "[Cory/Framegraph]")
{
    testing::VulkanTester t;
    Framegraph graph(t.ctx(), 0);

    auto pass = passes::tempResourcePass(graph.declareTask("PASS_TempResource"), {128, 128, 1});
    auto [outputInfo, outputState] =
        graph.declareOutput(pass.output().color, Sync::AccessType::ColorAttachmentWrite);
    CHECK(outputInfo.size.x == 128);
    CHECK(outputState.status == TextureMemoryStatus::Virtual);

    CommandRecorder recorder = t.ctx().device().createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphTempResourceTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 0,
        .swapchainImageIndex = 0,
        .frameNumber = 1,
        .commandBuffer = std::move(recorder),
    };

    auto execInfo = graph.record(frameCtx);
    const auto scratchHandle = FramegraphBufferHandle{pass.output().scratch};

    CHECK(ranges::contains(execInfo.buffers, scratchHandle));
    // CHECK(graph.resources().state(scratchHandle).status == BufferMemoryStatus::Allocated);
}

namespace passes {
struct MainSubtaskOut {
    TransientTextureHandle color;
};

RenderTaskDeclaration<MainSubtaskOut> mainPassWithSubpasses(RenderTaskBuilder builder,
                                                            TransientTextureHandle colorInput)
{

    auto sub1 = [](RenderTaskBuilder builder) -> RenderTaskDeclaration<TransientBufferHandle> {
        auto buffer = builder.create("BUF_subtask1",
                                     128u,
                                     Gpu::BufferUsageFlagBits::StorageBufferBit,
                                     Sync::AccessType::AnyShaderWrite);

        [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(buffer);

        CO_CORE_INFO("Subpass 1 running, created buffer handle {}", buffer.buffer());
    }(builder.subtask("InstanceBuffer"));

    auto sub2 =
        [](RenderTaskBuilder builder,
           TransientBufferHandle instanceBuffer) -> RenderTaskDeclaration<TransientTextureHandle> {
        const auto &bi = builder.read(instanceBuffer, Sync::AccessType::ComputeShaderReadOther);
        auto texture = builder.create("TEX_subtask2",
                                      {256, 256, 1},
                                      TextureFormat::R8G8B8A8_SRGB,
                                      Sync::AccessType::ColorAttachmentWrite);

        [[maybe_unused]] RenderInput render = co_await builder.finishDeclaration(texture);

        CO_CORE_INFO("Subpass 2 running on instance buffer handle {}, renders to {}",
                     instanceBuffer.buffer(),
                     texture.texture());
    }(builder.subtask("Texture"), sub1.output());

    auto subOut = sub2.output();
    auto blendIn = builder.read(subOut, Sync::AccessType::FragmentShaderReadOther);
    auto colorOutput =
        builder.readWrite(colorInput, Sync::AccessType::ColorAttachmentReadWrite).first;

    [[maybe_unused]] RenderInput render =
        co_await builder.finishDeclaration(MainSubtaskOut{colorOutput});

    CO_APP_INFO("{} Pass render commands are executed", builder.name());
}
} // namespace passes

TEST_CASE("Framegraph subtask definition", "[Cory/Framegraph]")
{
    testing::VulkanTester t;
    Framegraph graph(t.ctx(), 0);

    auto prevFrame = createGraphInput(t.ctx().device(), graph);

    auto mainPass = passes::mainPassWithSubpasses(graph.declareTask("PASS_Main"), prevFrame.handle);

    CommandRecorder recorder = t.ctx().device().createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphTempSubtaskTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 0,
        .swapchainImageIndex = 0,
        .frameNumber = 1,
        .commandBuffer = std::move(recorder),
    };

    auto [outputInfo, outputState] =
        graph.declareOutput(mainPass.output().color, Sync::AccessType::ColorAttachmentWrite);

    auto execInfo = graph.record(frameCtx);

    CHECK(execInfo.tasks.size() == 3); // main pass + 2 subtasks
    // unique textures: external input/output share the same handle + subtask texture
    CHECK(execInfo.resources.size() == 2);
    CHECK(execInfo.buffers.size() == 1);
}

TEST_CASE("Framegraph resolve avoids repeated buffer visits", "[Cory/Framegraph]")
{
    testing::VulkanTester t;
    Framegraph graph(t.ctx(), 0);

    auto shared = passes::sharedBufferProducer(graph.declareTask("PASS_SharedBuffer"));
    auto consumerA = passes::bufferConsumer(graph.declareTask("PASS_ConsumerA"),
                                            "TEX_ConsumerA",
                                            shared.output().buffer,
                                            {128, 128, 1});
    auto consumerB = passes::bufferConsumer(graph.declareTask("PASS_ConsumerB"),
                                            "TEX_ConsumerB",
                                            shared.output().buffer,
                                            {128, 128, 1});
    auto combine = passes::combineConsumers(graph.declareTask("PASS_Combine"),
                                            consumerA.output().color,
                                            consumerB.output().color,
                                            {128, 128, 1});

    auto [outputInfo, outputState] =
        graph.declareOutput(combine.output().color, Sync::AccessType::ColorAttachmentWrite);
    (void)outputInfo;
    (void)outputState;

    CommandRecorder recorder = t.ctx().device().createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphResolveVisitedTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 0,
        .swapchainImageIndex = 0,
        .frameNumber = 1,
        .commandBuffer = std::move(recorder),
    };

    auto execInfo = graph.record(frameCtx);

    std::unordered_set<FramegraphBufferHandle> uniqueBuffers;
    for (const auto &bufferHandle : execInfo.buffers) {
        uniqueBuffers.insert(bufferHandle);
    }
    CHECK(uniqueBuffers.size() == execInfo.buffers.size());
    CHECK(execInfo.buffers.size() == 1);
}

TEST_CASE("Framegraph resolves toy radix ordering", "[Cory/Framegraph]")
{
    testing::VulkanTester t;
    Framegraph graph(t.ctx(), 0);

    auto radixTask = passes::toyRadix(graph.declareTask("TASK_ToyRadix"));
    auto sinkTask = passes::toySink(graph.declareTask("TASK_ToySink"), radixTask.output().indices);

    auto [outputInfo, outputState] =
        graph.declareOutput(sinkTask.output(), Sync::AccessType::ColorAttachmentWrite);
    (void)outputInfo;
    (void)outputState;

    CommandRecorder recorder = t.ctx().device().createCommandRecorder(Gpu::CommandRecorderOptions{
        .label = "CMD_FramegraphToyRadixResolveTest",
        .queue = t.ctx().graphicsQueue().handle(),
        .level = Gpu::CommandBufferLevel::Primary,
    });

    FrameContext frameCtx{
        .inFlightIndex = 0,
        .swapchainImageIndex = 0,
        .frameNumber = 1,
        .commandBuffer = std::move(recorder),
    };

    auto execInfo = graph.record(frameCtx);

    std::unordered_map<RenderTaskHandle, std::string> names;
    for (const auto &[handle, info] : graph.renderTasks()) {
        names.emplace(handle, info.name);
    }

    auto taskIndex = [&](std::string_view name) {
        for (size_t i = 0; i < execInfo.tasks.size(); ++i) {
            if (names[execInfo.tasks[i]] == name) {
                return i;
            }
        }
        FAIL("Task not found");
        return execInfo.tasks.size();
    };

    CHECK(taskIndex("TASK_ToyRadix::Init") < taskIndex("TASK_ToyRadix::Histogram_0"));
    CHECK(taskIndex("TASK_ToyRadix::Preprocess") < taskIndex("TASK_ToyRadix::Histogram_0"));
    CHECK(taskIndex("TASK_ToyRadix::Histogram_0") < taskIndex("TASK_ToyRadix::Scan_0"));
    CHECK(taskIndex("TASK_ToyRadix::Scan_0") < taskIndex("TASK_ToyRadix::Scatter_0"));
    CHECK(taskIndex("TASK_ToyRadix::Scatter_0") < taskIndex("TASK_ToyRadix::Histogram_1"));
    CHECK(taskIndex("TASK_ToyRadix::Histogram_1") < taskIndex("TASK_ToyRadix::Scan_1"));
    CHECK(taskIndex("TASK_ToyRadix::Scan_1") < taskIndex("TASK_ToyRadix::Scatter_1"));
    CHECK(taskIndex("TASK_ToyRadix::Scatter_1") < taskIndex("TASK_ToySink"));
}
