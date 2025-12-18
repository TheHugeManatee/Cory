#include "SlangCompiler.hpp"

#include <Cory/Base/Log.hpp>

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <Cory/Base/ResourceLocator.hpp>
#include <atomic>
#include <fstream>
#include <mutex>

namespace Cory {
namespace detail {

class SlangObject : public ISlangUnknown {
  public:
    virtual ~SlangObject() = default;

    // ISlangUnknown
    SLANG_NO_THROW uint32_t SLANG_MCALL addRefInternal() { return ++m_refCount; }
    SLANG_NO_THROW uint32_t SLANG_MCALL releaseInternal()
    {
        uint32_t r = --m_refCount;
        if (r == 0) delete this;
        return r;
    }

    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterfaceInternal(SlangUUID const &uuid,
                                                                  void **outObject)
    {
        if (void *ptr = castAs(uuid)) {
            *outObject = ptr;
            addRef();
            return SLANG_OK;
        }
        *outObject = nullptr;
        return SLANG_E_NO_INTERFACE;
    }

  protected:
    // ISlangCastable (must be implemented by derived classes)
    virtual void *castAs(const SlangUUID &uuid) = 0;

  protected:
    std::atomic<uint32_t> m_refCount{1};
};

class MemoryBlob final : virtual public ISlangBlob, virtual public SlangObject {
  public:
    MemoryBlob(std::vector<uint8_t> &&data)
        : m_data(std::move(data))
    {
    }

    // ISlangUnknown
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return addRefInternal(); }
    SLANG_NO_THROW uint32_t SLANG_MCALL release() override { return releaseInternal(); }
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid,
                                                          void **outObject) override
    {
        return queryInterfaceInternal(uuid, outObject);
    }

    // ISlangCastable
    SLANG_NO_THROW void *castAs(const SlangUUID &uuid) override
    {
        if (uuid == SLANG_UUID_ISlangBlob || uuid == SLANG_UUID_ISlangUnknown) {
            return static_cast<ISlangBlob *>(this);
        }
        return nullptr;
    }

    // ISlangBlob
    SLANG_NO_THROW void const *SLANG_MCALL getBufferPointer() override { return m_data.data(); }

    SLANG_NO_THROW size_t SLANG_MCALL getBufferSize() override { return m_data.size(); }

  private:
    std::vector<uint8_t> m_data;
};

static slang::IGlobalSession *getGlobalSession()
{
    static Slang::ComPtr<slang::IGlobalSession> globalSession;
    static std::once_flag once_flag;
    std::call_once(once_flag, []() { createGlobalSession(globalSession.writeRef()); });
    return globalSession.get();
}

class CoryResourceFileSystem final : public ISlangFileSystem, public SlangObject {
  public:
    explicit CoryResourceFileSystem() {}

    // ISlangUnknown
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return addRefInternal(); }
    SLANG_NO_THROW uint32_t SLANG_MCALL release() override { return releaseInternal(); }
    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const &uuid,
                                                          void **outObject) override
    {
        return queryInterfaceInternal(uuid, outObject);
    }
    // ISlangCastable
    SLANG_NO_THROW void *castAs(const SlangUUID &uuid) override
    {
        if (uuid == SLANG_UUID_ISlangFileSystem || uuid == SLANG_UUID_ISlangUnknown) {
            return static_cast<ISlangFileSystem *>(this);
        }
        return nullptr;
    }

    // ISlangFileSystem
    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(const char *path, ISlangBlob **outBlob) override
    {
        CO_CORE_DEBUG("Slang requested included shader file: {}", path);
        try {
            auto fullPath =
                ResourceLocator::Locate(std::filesystem::path{path}, ResourceType::Shader);
            if (!std::filesystem::exists(fullPath)) {
                return SLANG_E_NOT_FOUND;
            }

            std::ifstream f(fullPath, std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());

            *outBlob = new MemoryBlob(std::move(data));
            return SLANG_OK;
        }
        catch (...) {
            return SLANG_E_NOT_FOUND;
        }
    }
};

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
    sessionDesc.compilerOptionEntryCount = gsl::narrow<uint32_t>(options.size());
    sessionDesc.fileSystem = new detail::CoryResourceFileSystem();

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