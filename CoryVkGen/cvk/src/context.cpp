#include <cvk/context.h>

#include <cvk/FmtEnum.h>
#include <cvk/FmtStruct.h>
#include <cvk/device_builder.h>
#include <cvk/instance.h>
#include <cvk/log.h>
#include <cvk/queue.h>
#include <cvk/swapchain.h>
#include <cvk/swapchain_builder.h>
#include <cvk/util/container.h>

#include <fmt/ranges.h>
#include <glm.h>
#include <vk_mem_alloc.h>

#include <limits>

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

context::~context() = default;

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

    auto queue_families = configure_queue_families();

    // build the device
    auto device = cvk::device_builder(physical_device_)
                      .add_queues(queue_families)
                      .enabled_features(enabledFeatures)
                      .enabled_extension_names(requested_extensions)
                      .enabled_layer_names(requested_layers)
                      .create();

    setup_queues(queue_families, device.get());

    return device;
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
    queues_.compute_family = find_best_queue_family(qfi_props, scoring_func(VK_QUEUE_COMPUTE_BIT));

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
    CVK_TRACE("    graphics: {} - {}",
              *queues_.graphics_family,
              flag_bits_to_string<VkQueueFlagBits>(
                  physical_device_.queue_family_properties[*queues_.graphics_family].queueFlags));
    CVK_TRACE("    transfer  {} - {}",
              *queues_.transfer_family,
              flag_bits_to_string<VkQueueFlagBits>(
                  physical_device_.queue_family_properties[*queues_.transfer_family].queueFlags));
    CVK_TRACE("    compute:  {} - {}",
              *queues_.compute_family,
              flag_bits_to_string<VkQueueFlagBits>(
                  physical_device_.queue_family_properties[*queues_.compute_family].queueFlags));

    if (surface_.has_value()) {
        CVK_TRACE(
            "    present:  {} - {}",
            *queues_.present_family,
            flag_bits_to_string<VkQueueFlagBits>(
                physical_device_.queue_family_properties[*queues_.present_family].queueFlags));
    }

    // figure out which queues we need to create - we always expect graphics and transfer queues
    // (i guess there might be compute-only devices but we don't care about those weirdos)
    std::set<uint32_t> queue_families_to_instantiate;
    queue_families_to_instantiate.insert(*queues_.graphics_family);
    queue_families_to_instantiate.insert(*queues_.transfer_family);

    if (queues_.compute_family) queue_families_to_instantiate.insert(*queues_.compute_family);
    if (queues_.present_family) queue_families_to_instantiate.insert(*queues_.present_family);
    return queue_families_to_instantiate;
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
    // get the capabilities of the swapchain
    auto swapchain_support = query_swap_chain_support();

    // currently VkPresentModeKHR enum is not formattable :(
    // CVK_INFO("swapchain supported present modes: {}", swapchain_support.present_modes);
    for (const auto &surfaceFmt : swapchain_support.formats)
        CVK_DEBUG("swapchain supported format: {}, {}", surfaceFmt.format, surfaceFmt.colorSpace);
    // CO_APP_INFO("Swap chain supports formats: {}", fmt::join(formats, ","));

    // get the present mode - ideally MAILBOX, but FIFO shall always be available as a fallback
    auto present_mode = util::find_or(
        swapchain_support.present_modes, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR);

    // get the surface format - BGRA8 with nonlinear sRGB is the preferred option
    auto surface_format = util::find_or(
        swapchain_support.formats,
        [](const auto &fmt) {
            return fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
                   fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        },
        swapchain_support.formats[0]);

    // determine the swapchain extent
    glm::uvec2 swapchain_extent;
    // if it's at limits, extent can vary. otherwise we need to set manually
    if (swapchain_support.capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        swapchain_extent = glm::uvec2{swapchain_support.capabilities.currentExtent.width,
                                      swapchain_support.capabilities.currentExtent.height};
    }
    else {
        swapchain_extent = glm::uvec2{800, 600};

        // NOTE: this is where we would ideally query the window manager for the window size.
        //       however, the current API does not allow passing such a size, so we have to fall
        //       back to a fixed window size, which might not look great and might not work at
        //       all on some platforms

        CVK_WARN("Surface did not supply a preferred swapchain extent. Falling back to a "
                 "default resolution of {}x{}. This can lead to unexpected results.",
                 swapchain_extent.x,
                 swapchain_extent.y);

        // technically, we should also check the capabilities.min/maxImageExtent dimensions here
    }

    // we add a "spare" one to achieve at least triple-buffering (typically), but of respect the max
    // image count given by the swapchain
    auto num_swapchain_images = swapchain_support.capabilities.minImageCount + 1;
    // respect maxImageCount (0 means unlimited)
    if (swapchain_support.capabilities.maxImageCount > 0) {
        num_swapchain_images =
            std::min(num_swapchain_images, swapchain_support.capabilities.maxImageCount);
    }

    // we partially rely on the defaults defined by the swapchain_builder here
    auto swch_builder = swapchain_builder(*this)
                            .surface(surface_.get())
                            .min_image_count(num_swapchain_images)
                            .present_mode(present_mode)
                            .image_format(surface_format.format)
                            .image_color_space(surface_format.colorSpace)
                            .image_extent(swapchain_extent)
                            .pre_transform(swapchain_support.capabilities.currentTransform)
                            .max_frames_in_flight(2);

    // if the graphics and present queues are different, the swap
    // chain images have to be shareable
    if (queues_.graphics_family != queues_.present_family) {
        swch_builder.image_sharing_mode(VK_SHARING_MODE_CONCURRENT)
            .queue_family_indices(
                {queues_.graphics_family.value(), queues_.present_family.value()});
        CVK_WARN("Graphics and present queues are not the same. Needed to enable "
                 "VK_SHARING_MODE_CONCURRENT which is suboptimal.");
    }
    else {
        // otherwise, exclusive has better performance
        swch_builder.image_sharing_mode(VK_SHARING_MODE_EXCLUSIVE).queue_family_indices({});
    }

    swapchain_ = swch_builder.create();
}

swap_chain_support context::query_swap_chain_support()
{
    const auto device = physical_device_.device;
    const auto surface = surface_.get();

    swap_chain_support details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    details.formats =
        vk_enumerate<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, device, surface);
    details.present_modes =
        vk_enumerate<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, device, surface);

    return details;
}

cvk::fence context::create_fence(VkFenceCreateFlags flags /*= {}*/)
{
    // fixme: we probably want a pool here!
    VkFenceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = flags};

    VkFence created_fence;
    VK_CHECKED_CALL(vkCreateFence(device_.get(), &create_info, nullptr, &created_fence),
                    "failed to create a fence object");

    auto vk_resource_ptr = make_shared_resource(
        created_fence, [dev = device_.get()](VkFence f) { vkDestroyFence(dev, f, nullptr); });

    return {*this, vk_resource_ptr};
}

cvk::semaphore context::create_semaphore(VkSemaphoreCreateFlags flags /*= {}*/)
{
    VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                      .flags = flags};

    VkSemaphore created_semaphore;
    VK_CHECKED_CALL(vkCreateSemaphore(device_.get(), &create_info, nullptr, &created_semaphore),
                    "failed to create a semaphore object");
    auto vk_resource_ptr =
        make_shared_resource(created_semaphore, [dev = device_.get()](VkSemaphore f) {
            vkDestroySemaphore(dev, f, nullptr);
        });
    return cvk::semaphore{std::move(vk_resource_ptr)};
}

} // namespace cvk