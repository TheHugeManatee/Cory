#include <cvk/context.h>

#include <cvk/FmtEnum.h>
#include <cvk/device_builder.h>
#include <cvk/instance.h>
#include <cvk/log.h>
#include <cvk/queue.h>

#include <vk_mem_alloc.h>

namespace cvk {

namespace {
template <typename ScoringFunctor>
std::optional<uint32_t>
find_best_queue_family(const std::vector<VkQueueFamilyProperties> &queue_family_properties,
                       ScoringFunctor scoring_func)
{
    // if eligible, score is 32 - number of total set bits
    // the thought is the lower the number of set bits,
    // the more "specialized" the family is and therefore more optimal
    std::vector<int> scores;
    std::transform(queue_family_properties.begin(),
                   queue_family_properties.end(),
                   std::back_inserter(scores),
                   scoring_func);
    auto best_it = std::max_element(scores.begin(), scores.end());
    if (*best_it == 0) return {};
    return static_cast<uint32_t>(std::distance(scores.begin(), best_it));
}
} // namespace

// explicit destructor to enable forward-declared unique_ptrs
context_queues::~context_queues() = default;

context::context(instance inst,
                 cvk::surface surface_khr /*= {} */,
                 VkPhysicalDeviceFeatures *requested_features /*= nullptr*/,
                 std::vector<const char *> requested_extensions /*= {}*/,
                 std::vector<const char *> requested_layers /*= {}*/)
    : instance_{std::move(inst)}
    , physical_device_{pick_device()} // if not explicitly supplied, enable all features. :)
    , physical_device_features_{requested_features ? *requested_features
                                                   : physical_device_.features}
    , surface_{std::move(surface_khr)}
    , device_{create_device(std::move(requested_extensions), std::move(requested_layers))}
    , vma_allocator_{init_allocator()}
{
    init_allocator();

    if (surface_.has_value()) { init_swapchain(); }

    //    // determine best depth format
    //    default_depth_stencil_format_ =
    //        find_supported_format(physical_device_info_.device,
    //                              {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
    //                              VK_IMAGE_TILING_OPTIMAL,
    //                              VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    //
    //    if (surface_.has_value())
    //        default_color_format_ = swapchain_->format();
    //    else {
    //        // FIXME: this is the next-best thing that was supported on my system, need to figure
    //        // out the best way to determine a supported format, i.e. by querying the device's
    //        supported
    //        // formats and picking the "best" RGB format
    //        default_color_format_ = VK_FORMAT_B8G8R8A8_SRGB;
    //    }
    //
    //    max_msaa_samples_ = get_max_usable_sample_count(physical_device_info_.properties);
}

cvk::physical_device context::pick_device()
{
    const auto devices = instance_.physical_devices();
    std::optional<cvk::physical_device> pickedDevice;

    // prefer any discrete GPU
    for (const auto &info : devices) {
        if (!pickedDevice && info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            pickedDevice = info;
        }
    }
    // if no discrete available, just use the first device available
    auto picked = pickedDevice.value_or(devices[0]);
    CVK_INFO("Using {}", picked.properties.deviceName);

    return picked;
}

std::set<uint32_t> context::configure_queue_families()
{
    // find the default queue families for graphics, transfer and compute
    // we use a scoring function that ranks eligible families by their amount of
    // specialization: the more other bits they have (i.e. the more other features the
    // family supports) the less attractive they are (because they might overlap with other
    // computations).
    // No idea if this makes sense at all, but I finally found a use for std::popcount! :)
    auto scoring_func = [](VkQueueFlagBits queue_flags) {
        return [=](const VkQueueFamilyProperties &qfp) {
            if (qfp.queueFlags & queue_flags) { return 32 - std::popcount(qfp.queueFlags); }
            return 0;
        };
    };
    const auto &qfi_props = physical_device_.queue_family_properties;
    queues_.graphics_family =
        find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_GRAPHICS_BIT));
    queues_.transfer_family =
        find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_TRANSFER_BIT));
    queues_.compute_family =
        find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_COMPUTE_BIT));

    // if we were passed a surface, try to initialize a present queue for it. no fancy
    // selection logic yet, just pick whatever works.
    if (surface_.has_value()) {
        VkBool32 supports_present;
        for (int qfi = 0; qfi < qfi_props.size(); ++qfi) {
            vkGetPhysicalDeviceSurfaceSupportKHR(
                physical_device_.device, qfi, surface_.get(), &supports_present);
            if (supports_present) { queues_.present_family = qfi; }
        }

        CVK_ASSERT(queues_.present_family.has_value(),
                   "graphics_context: surface was supplied but could not find a present "
                   "queue to support it.");
    }

    // log this out, you never know when this might be interesting..
    CVK_TRACE(R"(Instantiating queue families:)");
    CVK_TRACE(
        "    graphics: {} - {}",
        *queues_.graphics_family,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_.queue_family_properties[*queues_.graphics_family].queueFlags));
    CVK_TRACE(
        "    transfer  {} - {}",
        *queues_.transfer_family,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_.queue_family_properties[*queues_.transfer_family].queueFlags));
    CVK_TRACE(
        "    compute:  {} - {}",
        *queues_.compute_family,
        flag_bits_to_string<VkQueueFlagBits>(
            physical_device_.queue_family_properties[*queues_.compute_family].queueFlags));

    if (surface_.has_value()) {
        CVK_TRACE("    present:  {} - {}",
                  *queues_.present_family,
                  flag_bits_to_string<VkQueueFlagBits>(
                      physical_device_.queue_family_properties[*queues_.present_family]
                          .queueFlags));
    }

    // figure out which queues we need to create - we always expect graphics and transfer queues
    // (i guess there might be compute-only devices but we don't care about those weirdos)
    std::set<uint32_t> queue_families_to_instantiate;
    queue_families_to_instantiate.insert(*queues_.graphics_family);
    queue_families_to_instantiate.insert(*queues_.transfer_family);

    if (queues_.compute_family)
        queue_families_to_instantiate.insert(*queues_.compute_family);
    if (queues_.present_family)
        queue_families_to_instantiate.insert(*queues_.present_family);
    return queue_families_to_instantiate;
}

cvk::device context::create_device(std::vector<const char *> requested_extensions,
                                   std::vector<const char *> requested_layers)
{

    // if we got a surface, make sure we enable the required extension
    if (surface_.has_value()) { requested_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME); }

    // just enable everything! :)
    VkPhysicalDeviceFeatures enabledFeatures = physical_device_.features;
    //    spdlog::info("Available Queue families:");
    //    for (const auto &qfp : physicalDevice_.queue_family_properties) {
    //        spdlog::info("{}", qfp);
    //    }
    std::vector<const char *> extensions{};

    auto queue_families = configure_queue_families();

    // build the device
    auto device = cvk::device_builder(physical_device_)
                      .add_queues(queue_families)
                      .enabled_features(enabledFeatures)
                      .enabled_extension_names(extensions)
                      .create();

    setup_queues(queue_families, device.get());

    return device;
}

void context::setup_queues(const std::set<uint32_t> &queue_families, VkDevice device)
{
    static constexpr uint32_t INVALID_QUEUE_INDEX{0xFFFFFFFF};
    std::unordered_map<uint32_t, cvk::queue *> family_map;

    auto make_queue_name = [&](uint32_t familyIndex) {
        std::vector<std::string> supported;
        auto add_supported = [&](const auto &family_opt, const std::string &name) {
            if (family_opt && familyIndex == *family_opt) { supported.push_back(name); }
        };
        add_supported(queues_.graphics_family, "graphics");
        add_supported(queues_.compute_family, "compute");
        add_supported(queues_.transfer_family, "transfer");
        add_supported(queues_.present_family, "present");
        return join(supported, "|");
    };

    for (uint32_t familyIndex : queue_families) {
        VkQueue vk_queue{};
        vkGetDeviceQueue(device, familyIndex, 0, &vk_queue);
        auto name = make_queue_name(familyIndex);

        queues_.storage.emplace_back(
            std::make_unique<cvk::queue>(*this, name, vk_queue, familyIndex));
        family_map[familyIndex] = queues_.storage.back().get();
    }

    // setup the queue objects
    auto get_queue = [&](auto &family, auto &queue) {
        if (family.has_value()) { queue = family_map[*family]; }
    };
    get_queue(queues_.graphics_family, queues_.graphics);
    get_queue(queues_.compute_family, queues_.compute);
    get_queue(queues_.transfer_family, queues_.transfer);
    get_queue(queues_.present_family, queues_.present);
}

std::shared_ptr<VmaAllocator_T> context::init_allocator()
{
    // create the VMA allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    // TODO: the vulkan version should come from the instance, as the instance/device might not
    // actually support 1.2
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physical_device_.device;
    allocatorInfo.device = device_.get();
    allocatorInfo.instance = instance_.get();

    VmaAllocator vmaAllocator;
    VK_CHECKED_CALL(vmaCreateAllocator(&allocatorInfo, &vmaAllocator),
                    "Could not create VMA allocator object");
    // wrap the allocator with a shared_ptr with a custom deallocator
    return make_shared_resource(vmaAllocator,
                                [](VmaAllocator alloc) { vmaDestroyAllocator(alloc); });
}
void context::init_swapchain()
{
    // TODO
}

} // namespace cvk