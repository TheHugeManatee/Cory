#pragma once

#include <cstdint>
#include <memory>

#include <gsl/gsl>

namespace Cory {

class CpuBuffer {
  public:
    CpuBuffer(size_t size)
        : m_size{size}
        , m_data{std::make_unique<uint8_t[]>(m_size)} {};
    ~CpuBuffer(){};

    uint8_t *data() { return m_data.get(); };
    size_t size() { return m_size; }

    gsl::span<uint8_t> span() { return {data(), size()}; }

  private:
    size_t m_size;
    std::unique_ptr<uint8_t[]> m_data;
};

} // namespace Cory