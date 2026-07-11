#pragma once

#include <cstdint>
#include <filesystem>

namespace Cory {

struct HeadlessOutputOptions {
    std::filesystem::path outputPath{};

    [[nodiscard]] bool requested() const { return !outputPath.empty(); }

    void forceHeadlessAndFinite(bool &headless, uint64_t &framesToRender) const
    {
        if (!requested()) {
            return;
        }
        headless = true;
        if (framesToRender == 0) {
            framesToRender = 1;
        }
    }
};

} // namespace Cory
