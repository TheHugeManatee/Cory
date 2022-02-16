#pragma once

#include <fmt/format.h>

#include <memory>
#include <stdexcept>
#include <vector>

#define VK_CHECKED_CALL(x, err)                                                                    \
    do {                                                                                           \
        if (auto code = (x); code != VK_SUCCESS) {                                                 \
            throw std::runtime_error(fmt::format(#x " failed with {}: {}", code, (err)));          \
        }                                                                                          \
    } while (0)

namespace cvk {
/**
 * @brief utility to shorten the typical enumerate pattern you need in c++
 *
 * For example:
 *
 *      std::vector<VkExtensionProperties> instance_extensions =
 *          vk_enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);
 *
 */
template <typename ReturnT, typename FunctionT, typename... FunctionParameters>
std::vector<ReturnT> vk_enumerate(FunctionT func, FunctionParameters... parameters)
{
    uint32_t count = 0;
    func(parameters..., &count, nullptr);
    std::vector<ReturnT> values{size_t(count)};
    func(parameters..., &count, values.data());
    return values;
}

/**
 * wrapper base class for vulkan objects - holds a shared pointer of the opaque type
 */
template <typename WrappedVkType> class basic_vk_wrapper {
  public:
    using vk_type = WrappedVkType;
    using vk_opaque_type = std::remove_pointer_t<WrappedVkType>;
    using vk_shared_ptr = std::shared_ptr<vk_opaque_type>;

    explicit basic_vk_wrapper(vk_shared_ptr vk_resource_ptr = {})
        : vk_resource_ptr_{std::move(vk_resource_ptr)}
    {
    }

    [[nodiscard]] vk_type get() const { return vk_resource_ptr_.get(); }
    [[nodiscard]] bool has_value() const { return vk_resource_ptr_.get() != nullptr; }

  private:
    vk_shared_ptr vk_resource_ptr_;
};
} // namespace cvk