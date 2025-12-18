#pragma once

#include "Shader.hpp"

#include <expected>
#include <slang-com-ptr.h>

namespace Cory {
/// This class is a wrapper around the Slang compiler to compile shaders during runtime.
class SlangCompiler {
  public:
    SlangCompiler();
    ~SlangCompiler();

    using SpirvByteCode = std::vector<uint32_t>;
    using CompilationError = std::string;
    using CompilationResult = std::expected<SpirvByteCode, CompilationError>;

    [[nodiscard]] CompilationResult compileShader(const ShaderSource &source);

  private:
    Slang::ComPtr<slang::ISession> session_;
};
} // namespace Cory