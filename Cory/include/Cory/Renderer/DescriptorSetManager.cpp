#include "DescriptorSetManager.hpp"

#include <Cory/Base/Log.hpp>
#include <Cory/Renderer/Context.hpp>
#include <Cory/Renderer/ResourceManager.hpp>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Vk/DescriptorPoolCreateInfo.h>
#include <Magnum/Vk/DescriptorSet.h>
#include <Magnum/Vk/DescriptorSetLayoutCreateInfo.h>
#include <Magnum/Vk/DescriptorType.h>

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
};

// defaulted - nothing to be done here
DescriptorSetManager::DescriptorSetManager() = default;

DescriptorSetManager::~DescriptorSetManager()
{
    data_->resourceManager->release(data_->layoutHandle);
}

void DescriptorSetManager::init(Magnum::Vk::Device &device,
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
    data_->descriptorPool =
        Vk::DescriptorPool{device, Vk::DescriptorPoolCreateInfo{instances * 4, bindings}};

    // create descriptor sets
    auto &layout = resourceManager[data_->layoutHandle];
    const auto allocate_set = [&]() { return data_->descriptorPool.allocate(layout); };
    // allocate one descriptor set for each type and frame in flight
    std::generate_n(std::back_inserter(data_->staticDescriptorSets), instances, allocate_set);
    std::generate_n(std::back_inserter(data_->frameDescriptorSets), instances, allocate_set);
    std::generate_n(std::back_inserter(data_->passDescriptorSets), instances, allocate_set);
    std::generate_n(std::back_inserter(data_->userDescriptorSets), instances, allocate_set);
}

void DescriptorSetManager::update(Vk::Device &device,
                                  SetType modifiedType,
                                  gsl::index instanceIndex)
{
    // todo
}

DescriptorSetLayoutHandle DescriptorSetManager::layout() { return data_->layoutHandle; }

uint32_t DescriptorSetManager::instances() const
{
    return gsl::narrow_cast<uint32_t>(data_->staticDescriptorSets.size());
}

Magnum::Vk::DescriptorSet &DescriptorSetManager::get(DescriptorSetManager::SetType type,
                                                     gsl::index setIndex)
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
} // namespace Cory