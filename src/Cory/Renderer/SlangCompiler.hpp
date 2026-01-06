#pragma once

#include "Shader.hpp"

#include <slang-com-ptr.h>
#include <slang.h>

#include <expected>
#include <string_view>

namespace Cory {
/// This class is a wrapper around the Slang compiler to compile shaders during runtime.
class SlangCompiler {
  public:
    SlangCompiler();
    ~SlangCompiler();

    /// @brief Compile a shader source into SPIR-V bytecode.
    /// @param source The slang shader source to compile.
    /// @param entryPoint The entry point function name.
    /// @param optimize Whether to optimize the shader code.
    /// @return The compiled SPIR-V bytecode or an error message.
    [[nodiscard]] CompilationResult compileShader(const ShaderSource &source,
                                                  std::string_view entryPoint,
                                                  bool optimize);

  private:
    void initSession();
    SlangStage toSlangStage(Gpu::ShaderStageFlagBits stage) const;

    Slang::ComPtr<slang::ISession> session_;
    slang::IGlobalSession *globalSession_{nullptr};
    SlangProfileID spirvProfile_{SLANG_PROFILE_UNKNOWN};
    Slang::ComPtr<ISlangFileSystem> fileSystem_;
};
} // namespace Cory
