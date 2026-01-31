#pragma once

#include <Cory/Base/Common.hpp>
#include <Cory/Base/Log.hpp>

#include <fmt/format.h>

#include <any>
#include <array>
#include <memory>

namespace Cory {

/// a type-erased container to keep vulkan structs alive and link up a pnext chain
template <size_t MAX_CHAIN_SIZE = 10> class PNextChain : NoCopy {
  public:
    PNextChain() = default;
    PNextChain(PNextChain &&) = default;
    PNextChain &operator=(PNextChain &&) = default;

    auto &prepend(auto next_struct)
    {
        CO_CORE_ASSERT(current_ < MAX_CHAIN_SIZE, "PNextChain is full");
        data_[current_] = next_struct;

        auto &next = any_cast<decltype(next_struct) &>(data_[current_]);

        next.pNext = std::exchange(head_, &next);
        current_++;

        return next;
    }

    /// insert something into the storage without appending it into the chain
    auto &insert(auto aux_struct)
    {
        CO_CORE_ASSERT(current_ < MAX_CHAIN_SIZE, "PNextChain is full");
        data_[current_] = aux_struct;

        auto &next = any_cast<decltype(aux_struct) &>(data_[current_]);
        current_++;

        return next;
    }

    [[nodiscard]] void *head() const { return head_; };

    [[nodiscard]] size_t size() const { return current_; };

  private:
    std::array<std::any, MAX_CHAIN_SIZE> data_;
    gsl::index current_{};
    void *head_{};
};

constexpr bool isDepthFormat(Gpu::Format format)
{
    return format == Gpu::Format::D16_UNORM || format == Gpu::Format::X8_D24_UNORM_PACK32 ||
           format == Gpu::Format::D32_SFLOAT || format == Gpu::Format::D16_UNORM_S8_UINT ||
           format == Gpu::Format::D24_UNORM_S8_UINT || format == Gpu::Format::D32_SFLOAT_S8_UINT;
}

} // namespace Cory
