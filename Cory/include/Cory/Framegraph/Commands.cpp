#include <Cory/Framegraph/Commands.hpp>

namespace Cory::Framegraph {

Commands::Commands(Context &ctx, Magnum::Vk::CommandBuffer &cmdBuffer)
    : ctx_(&ctx)
    , cmdBuffer_{&cmdBuffer}
{
}
} // namespace Cory::Framegraph