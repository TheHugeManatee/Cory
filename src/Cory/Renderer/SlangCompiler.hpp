#pragma once

#include "Shader.hpp"

#include <slang-com-ptr.h>
#include <slang.h>

#include <expected>
#include <string>

namespace Cory {
/// This class is a wrapper around the Slang compiler to compile shaders during runtime.
class SlangCompiler {
  public:
    SlangCompiler();
    ~SlangCompiler();

    using SpirvByteCode = std::vector<uint32_t>;
    using CompilationError = std::string;
    using CompilationResult = std::expected<SpirvByteCode, CompilationError>;

    [[nodiscard]] CompilationResult compileShader(const ShaderSource &source, bool optimize);

  private:
    void initSession();
    SlangStage toSlangStage(Gpu::ShaderStageFlagBits stage) const;
    std::string resolveEntryPoint(const ShaderSource &source) const;
    CompilationResult makeError(std::string message) const;

    Slang::ComPtr<slang::ISession> session_;
    slang::IGlobalSession *globalSession_{nullptr};
    SlangProfileID spirvProfile_{SLANG_PROFILE_UNKNOWN};
    Slang::ComPtr<ISlangFileSystem> fileSystem_;
};
} // namespace Cory
