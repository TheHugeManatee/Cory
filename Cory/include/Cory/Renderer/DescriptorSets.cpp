#include "DescriptorSets.hpp"

#include "Cory/Framegraph/TextureManager.hpp"
#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>
#include <Cory/Renderer/UniformBufferObject.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Vk/CommandBuffer.h>
#include <Magnum/Vk/DescriptorPoolCreateInfo.h>
#include <Magnum/Vk/DescriptorSet.h>
#include <Magnum/Vk/DescriptorSetLayoutCreateInfo.h>
#include <Magnum/Vk/DescriptorType.h>
#include <Magnum/Vk/Device.h>
#include <Magnum/Vk/ImageView.h>
#include <Magnum/Vk/PipelineLayout.h>
#include <Magnum/Vk/Sampler.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include <tuple>
#include <vector>

namespace Cory {

namespace Vk = Magnum::Vk;

struct DescriptorSetManagerPrivate {
    Vk::Device *device;
    ResourceManager *resourceManager;
    DescriptorSetLayoutHandle layoutHandle;
    Magnum::Vk::DescriptorPool descriptorPool{Corrade::NoCreate};

    std::vector<Vk::DescriptorSet> staticDescriptorSets;
    std::vector<Vk::DescriptorSet> frameDescriptorSets;
    std::vector<Vk::DescriptorSet> passDescriptorSets;
    std::vector<Vk::DescriptorSet> userDescriptorSets;

    std::vector<VkWriteDescriptorSet> storedWrites;
    std::vector<VkDescriptorBufferInfo> storedWriteBufferInfos;
    std::vector<std::vector<VkDescriptorImageInfo>> storedWriteImageInfos;
};

// defaulted - nothing to be done here
DescriptorSets::DescriptorSets() = default;

DescriptorSets::~DescriptorSets() { data_->resourceManager->release(data_->layoutHandle); }

void DescriptorSets::init(Magnum::Vk::Device &device,
                          ResourceManager &resourceManager,
                          Magnum::Vk::DescriptorSetLayoutCreateInfo defaultLayout,
                          uint32_t instances)
{
    CO_CORE_ASSERT(data_ == nullptr, "Object already initialized!");

    data_ = std::make_unique<DescriptorSetManagerPrivate>();
    data_->device = &device;
    data_->resourceManager = &resourceManager;
    data_->layoutHandle =
        data_->resourceManager->createDescriptorLayout("Default Layout", defaultLayout);

    // determine necessary max values and create descriptor pool
    std::vector<std::pair<Vk::DescriptorType, uint32_t>> bindings{defaultLayout->bindingCount};
    for (int bindingIdx = 0; bindingIdx < defaultLayout->bindingCount; ++bindingIdx) {
        bindings[bindingIdx].first =
            static_cast<Vk::DescriptorType>(defaultLayout->pBindings[bindingIdx].descriptorType);
        bindings[bindingIdx].second =
            defaultLayout->pBindings[bindingIdx].descriptorCount * instances;
    }
    data_->descriptorPool = Vk::DescriptorPool{
        device,
        Vk::DescriptorPoolCreateInfo{
            instances * 4, bindings, Vk::DescriptorPoolCreateInfo::Flag::UpdateAfterBind}};

    // create descriptor sets
    auto &layout = resourceManager[data_->layoutHandle];
    const auto allocate_set = [&](std::string_view name, gsl::index i) {
        auto set = data_->descriptorPool.allocate(layout);
        nameVulkanObject(*data_->device, set, fmt::format("DESC_{} [{}]", name, i));
        return set;
    };
    // allocate one descriptor set for each type and frame in flight
    for (gsl::index i = 0; i < instances; ++i) {
        data_->staticDescriptorSets.push_back(allocate_set("Static", i));
        data_->frameDescriptorSets.push_back(allocate_set("Frame", i));
        data_->passDescriptorSets.push_back(allocate_set("Pass", i));
        data_->userDescriptorSets.push_back(allocate_set("User", i));
    }
}

DescriptorSetLayoutHandle DescriptorSets::layout() { return data_->layoutHandle; }

uint32_t DescriptorSets::instances() const
{
    return gsl::narrow_cast<uint32_t>(data_->staticDescriptorSets.size());
}

Magnum::Vk::DescriptorSet &DescriptorSets::get(DescriptorSets::SetType type, gsl::index setIndex)
{
    CO_CORE_ASSERT(setIndex < instances(), "Set index out of bounds");
    switch (type) {
    case SetType::Static:
        return data_->staticDescriptorSets[setIndex];
    case SetType::Frame:
        return data_->frameDescriptorSets[setIndex];
    case SetType::Pass:
        return data_->passDescriptorSets[setIndex];
    case SetType::User:
        return data_->userDescriptorSets[setIndex];
    }
    throw std::invalid_argument("Invalid SetType specified");
}

DescriptorSets &DescriptorSets::write(DescriptorSets::SetType type,
                                      gsl::index instanceIndex,
                                      const UniformBufferObjectBase &ubo)
{
    auto &set = get(Cory::DescriptorSets::SetType::Static, instanceIndex);

    data_->storedWriteBufferInfos.push_back(ubo.descriptorInfo(instanceIndex));

    data_->storedWrites.emplace_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set,
        .dstBinding = static_cast<uint32_t>(BindPoints::UniformBufferObject),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &data_->storedWriteBufferInfos.back(),
    });

    return *this;
}

DescriptorSets &DescriptorSets::write(SetType type,
                                      gsl::index instanceIndex,
                                      gsl::span<VkImageLayout> layouts,
                                      gsl::span<ImageViewHandle> images,
                                      gsl::span<SamplerHandle> samplers)
{
    auto &set = get(SetType::Static, instanceIndex);

    auto &resources = *data_->resourceManager;

    const std::vector<VkDescriptorImageInfo> imageInfos =
        ranges::views::zip(samplers, images, layouts) |
        ranges::views::transform([&resources](const auto &e) {
            return VkDescriptorImageInfo{.sampler = resources[std::get<0>(e)],
                                         .imageView = resources[std::get<1>(e)],
                                         .imageLayout = std::get<2>(e)};
        }) |
        ranges::to<std::vector>;

    data_->storedWriteImageInfos.push_back(imageInfos);

    data_->storedWrites.emplace_back(
        VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = set,
                             .dstBinding = static_cast<uint32_t>(BindPoints::CombinedImageSampler),
                             .descriptorCount = 1,
                             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             .pImageInfo = data_->storedWriteImageInfos.back().data()});

    return *this;
}

void DescriptorSets::flushWrites()
{
    auto &device = *data_->device;

    device->UpdateDescriptorSets(device,
                                 static_cast<uint32_t>(data_->storedWrites.size()),
                                 data_->storedWrites.data(),
                                 0,
                                 nullptr);

    // clear all information about stored writes so the next write starts cleanly
    data_->storedWrites.clear();
    data_->storedWriteBufferInfos.clear();
    data_->storedWriteImageInfos.clear();
}

void DescriptorSets::bind(Magnum::Vk::CommandBuffer &cmd,
                          gsl::index instanceIndex,
                          Magnum::Vk::PipelineLayout &pipelineLayout)
{
    auto &device = *data_->device;

    const std::vector sets{data_->staticDescriptorSets[instanceIndex].handle(),
                           data_->frameDescriptorSets[instanceIndex].handle(),
                           data_->passDescriptorSets[instanceIndex].handle(),
                           data_->userDescriptorSets[instanceIndex].handle()};

    device->CmdBindDescriptorSets(cmd,
                                  VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipelineLayout,
                                  0,
                                  gsl::narrow<uint32_t>(sets.size()),
                                  sets.data(),
                                  0,
                                  nullptr);
}

} // namespace Cory