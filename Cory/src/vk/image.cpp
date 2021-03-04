#include "vk/image.h"

#include "vk/graphics_context.h"

namespace cory {
namespace vk {

image_builder::operator image() { return ctx_.create_image(*this); }

image image_builder::create() { return ctx_.create_image(*this); }

} // namespace vk
} // namespace cory