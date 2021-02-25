#pragma once

#include <Context.h>
#include <cstdint>
#include <vulkan/vulkan.hpp>

#include "Image.h"

namespace Cory {

class FrameGraph;

class FrameGraphTextureDesc {
  uint32_t width{1};
  uint32_t height{1};
  uint32_t depth{1};
  vk::Format format{vk::Format::eR8G8B8Srgb};
  // about the following i'm less sure, but one or more of the following would
  // be required
  vk::ImageLayout initialLayout{vk::ImageLayout::eUndefined};
  vk::ImageLayout finalLayout{vk::ImageLayout::eGeneral};
  vk::AttachmentLoadOp loadOp{vk::AttachmentLoadOp::eDontCare};
  vk::AttachmentStoreOp storeOp{vk::AttachmentStoreOp::eDontCare};
};

class FrameGraphBufferDesc {
  uint64_t size{};
};

struct FrameGraphResource {
  uint64_t id {};
};
struct FrameGraphMutableResource {
  uint64_t id{};
};

class RenderPassResources {
public:
  // access to the actual resource through the handle
  Image &getImage(FrameGraphResource);
};

class RenderPass {
public:
  RenderPass(const std::string &name)
      : name_{name} {};

private:
  std::string name_;
};

/// can only be created by the FrameGraph. Provides access to the resource
/// allocation methods
class RenderPassBuilder {
public:
  friend FrameGraph;

  FrameGraphResource createTexture(const FrameGraphTextureDesc &);
  FrameGraphResource createRenderTarget(const FrameGraphTextureDesc &);

  FrameGraphResource read(FrameGraphResource input);
  FrameGraphMutableResource write(FrameGraphMutableResource output);

private:
  RenderPassBuilder(FrameGraph &fg)
      : frameGraph_{fg}
  {
  }

  FrameGraph &frameGraph_;
  // store all promised resources
};

class FrameGraph {
public:
  friend RenderPassBuilder;

  template <typename RenderPassData, typename SetupFunc, typename ExecuteFunc>
  RenderPass &addRenderPass(const std::string &name, SetupFunc &&setupFunctor,
                            ExecuteFunc &&executeFunctor)
  {
    // safeguard to avoid accidentally catching large structs in the executor function
    static_assert(sizeof(executeFunctor) < 1024);


    RenderPassData data;
    RenderPassBuilder builder{*this};
    setupFunctor(builder, data);

    auto& pass = passes_.emplace_back(name);
    return pass;
  };

private:
    std::vector<RenderPass> passes_;
};

} // namespace Cory