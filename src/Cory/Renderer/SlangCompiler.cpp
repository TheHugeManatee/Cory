#include "SlangCompiler.hpp"

#include <Cory/Base/Log.hpp>

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <mutex>

namespace Cory {
namespace detail {
static slang::IGlobalSession *getGlobalSession()
{
    static Slang::ComPtr<slang::IGlobalSession> globalSession;
    static std::once_flag once_flag;
    std::call_once(once_flag, []() { createGlobalSession(globalSession.writeRef()); });
    return globalSession.get();
}

} // namespace detail

SlangCompiler::SlangCompiler()
{
    auto *globalSession = detail::getGlobalSession();

    // 2. Create Session
    slang::SessionDesc sessionDesc = {};
    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile("spirv_1_5");

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    std::array<slang::CompilerOptionEntry, 1> options = {
        {slang::CompilerOptionName::EmitSpirvDirectly,
         {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}}};
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = options.size();

    globalSession->createSession(sessionDesc, session_.writeRef());
}

SlangCompiler::~SlangCompiler() {}

SlangCompiler::CompilationResult SlangCompiler::compileShader(const ShaderSource &source)
{
    // Load module
    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slangModule = session_->loadModuleFromSourceString(
            source.filePath().filename().string().c_str(),
            source.filePath().string().c_str(),
            source.source().c_str(),
            diagnosticsBlob.writeRef()); // Optional diagnostic container

        if (!slangModule) {
            if (diagnosticsBlob != nullptr) {
                return std::unexpected(
                    std::string{(const char *)diagnosticsBlob->getBufferPointer()});
            }
            return std::unexpected("Could not load Slang module from source");
        }
    }

    // Query Entry Points
    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());
        if (!entryPoint) {
            CO_CORE_ERROR("Error getting entry point");
            return {};
        }
    }

    // Compose Modules + Entry Points
    std::array<slang::IComponentType *, 2> componentTypes = {slangModule, entryPoint};

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = session_->createCompositeComponentType(componentTypes.data(),
                                                                    componentTypes.size(),
                                                                    composedProgram.writeRef(),
                                                                    diagnosticsBlob.writeRef());
        if (SLANG_FAILED(result)) {
            if (diagnosticsBlob != nullptr) {
                return std::unexpected(
                    std::string{(const char *)diagnosticsBlob->getBufferPointer()});
            }
            return std::unexpected("Could not compose Slang component type");
        }
    }

    // Link
    Slang::ComPtr<slang::IComponentType> linkedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result =
            composedProgram->link(linkedProgram.writeRef(), diagnosticsBlob.writeRef());
        if (SLANG_FAILED(result)) {
            if (diagnosticsBlob != nullptr) {
                return std::unexpected(
                    std::string{(const char *)diagnosticsBlob->getBufferPointer()});
            }
            return std::unexpected("Could not link Slang component type");
        }
    }

    // Get Target Kernel Code
    Slang::ComPtr<slang::IBlob> spirvCode;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = linkedProgram->getEntryPointCode(
            0, 0, spirvCode.writeRef(), diagnosticsBlob.writeRef());

        if (SLANG_FAILED(result)) {
            if (diagnosticsBlob != nullptr) {
                return std::unexpected(
                    std::string{(const char *)diagnosticsBlob->getBufferPointer()});
            }
            return std::unexpected("Could not get SPIR-V code from Slang entry point");
        }
    }

    // Return result
    SpirvByteCode result;
    result.resize(spirvCode->getBufferSize() / sizeof(uint32_t));
    memcpy(result.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());

    return result;
}
} // namespace Cory