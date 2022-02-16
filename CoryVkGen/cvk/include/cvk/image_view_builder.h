#include "image_view.h"



namespace cvk {
class context;
class image;

class image_view_builder {
  public:
    friend image_view;
    image_view_builder(context &ctx, const image &img);

    [[nodiscard]] image_view_builder &next(const void *pNext) noexcept
    {
        info_.pNext = pNext;
        return *this;
    }

    [[nodiscard]] image_view_builder &flags(VkImageViewCreateFlags flags) noexcept
    {
        info_.flags = flags;
        return *this;
    }

    [[nodiscard]] image_view_builder &image(VkImage image) noexcept
    {
        info_.image = image;
        return *this;
    }

    [[nodiscard]] image_view_builder &view_type(VkImageViewType viewType) noexcept
    {
        info_.viewType = viewType;
        return *this;
    }

    [[nodiscard]] image_view_builder &format(VkFormat format) noexcept
    {
        info_.format = format;
        return *this;
    }

    [[nodiscard]] image_view_builder &components(VkComponentMapping components) noexcept
    {
        info_.components = components;
        return *this;
    }

    [[nodiscard]] image_view_builder &
    subresource_range(VkImageSubresourceRange subresourceRange) noexcept
    {
        info_.subresourceRange = subresourceRange;
        return *this;
    }

    [[nodiscard]] image_view create();

  private:
    context &ctx_;
    VkImageViewCreateInfo info_{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                .components =
                                    VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                       .a = VK_COMPONENT_SWIZZLE_IDENTITY}};
    const cvk::image &image_;
    std::string_view name_;
};
}