#include "SlangCompiler.hpp"

#include <Cory/Base/Log.hpp>

#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <Cory/Base/ResourceLocator.hpp>
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string_view>
#include <utility>

namespace Cory {
namespace detail {


/**
 * @brief Base class to support the slang COM interfaces.
 * 
 * Slang COM interfaces require reference counting and querying for interfaces:
 *  - addRef and release to add and remove ref counts, deleting itself when the last ref is released.
 *  - queryInterface to get pointers to supported interfaces (equivalent to dynamic_cast)
 */
class SlangObject {
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

/// @brief Memory blob is just an arbitrary length buffer
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

/// @brief File system that uses Cory's resource locator to find and load included shader files.
/// Can later be extended to support virtual file systems like cmrc etc, to package shaders into the binary.
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

/// Global static Slang session to be shared among all SlangCompiler instances
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
    initSession();
}

SlangCompiler::~SlangCompiler() {}

void SlangCompiler::initSession()
{
    globalSession_ = detail::getGlobalSession();
    if (!globalSession_) {
        CO_CORE_ERROR("Failed to acquire Slang global session");
        return;
    }

    fileSystem_.attach(new detail::CoryResourceFileSystem());

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    spirvProfile_ = globalSession_->findProfile("spirv_1_5");
    targetDesc.profile = spirvProfile_;

    slang::SessionDesc sessionDesc = {};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
    sessionDesc.fileSystem = fileSystem_.get();

    std::array options = {
        slang::CompilerOptionEntry{slang::CompilerOptionName::EmitSpirvDirectly,
                                   {slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}},
    };
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(options.size());

    SlangResult result = globalSession_->createSession(sessionDesc, session_.writeRef());
    if (SLANG_FAILED(result)) {
        CO_CORE_ERROR("Failed to create Slang session (error code {})", result);
        session_.setNull();
    }
}

SlangCompiler::CompilationResult
SlangCompiler::compileShader(const ShaderSource &source, std::string_view entryPoint, bool optimize)
{
    if (!session_) {
        return makeError("Slang session is not initialized");
    }

    Slang::ComPtr<slang::ICompileRequest> request;
    SlangResult createResult = session_->createCompileRequest(request.writeRef());
    if (SLANG_FAILED(createResult)) {
        return makeError("Failed to create Slang compile request");
    }

    request->setCodeGenTarget(SLANG_SPIRV);
    if (spirvProfile_ != SLANG_PROFILE_UNKNOWN) {
        request->setTargetProfile(0, spirvProfile_);
    }
    request->setMatrixLayoutMode(SLANG_MATRIX_LAYOUT_COLUMN_MAJOR);
    request->setDebugInfoLevel(SLANG_DEBUG_INFO_LEVEL_NONE);
    request->setOptimizationLevel(optimize ? SLANG_OPTIMIZATION_LEVEL_HIGH
                                           : SLANG_OPTIMIZATION_LEVEL_DEFAULT);

    const std::string moduleName = source.filePath().filename().string();
    if (!moduleName.empty()) {
        request->setDefaultModuleName(moduleName.c_str());
    }
    const auto stage = toSlangStage(source.type());
    if (stage == SlangStage::SLANG_STAGE_NONE) {
        return makeError("Unsupported shader stage for Slang compilation");
    }

    auto detectLanguage = [&source]() {
        auto ext = source.filePath().extension();
        if (ext == ".slang" || ext == ".hlsl") {
            return SLANG_SOURCE_LANGUAGE_SLANG;
        }
        return SLANG_SOURCE_LANGUAGE_SLANG;
    };

    const char *translationUnitName = moduleName.empty() ? nullptr : moduleName.c_str();
    const int translationUnit = request->addTranslationUnit(detectLanguage(), translationUnitName);

    for (const auto &[name, value] : source.defines()) {
        request->addTranslationUnitPreprocessorDefine(translationUnit, name.c_str(), value.c_str());
    }

    const auto fullPath = source.filePath().string();
    request->addTranslationUnitSourceString(translationUnit,
                                            fullPath.empty() ? moduleName.c_str()
                                                             : fullPath.c_str(),
                                            source.source().c_str());

    const std::string entryPointName(entryPoint);
    const int entryPointIndex =
        request->addEntryPoint(translationUnit, entryPointName.c_str(), stage);

    SlangResult compileResult = request->compile();
    if (SLANG_FAILED(compileResult)) {
        Slang::ComPtr<slang::IBlob> diagnostics;
        request->getDiagnosticOutputBlob(diagnostics.writeRef());
        if (diagnostics) {
            return makeError(
                std::string(static_cast<const char *>(diagnostics->getBufferPointer())));
        }
        return makeError("Slang compilation failed");
    }

    Slang::ComPtr<slang::IBlob> spirvCode;
    SlangResult codeResult =
        request->getEntryPointCodeBlob(entryPointIndex, 0, spirvCode.writeRef());
    if (SLANG_FAILED(codeResult) || !spirvCode) {
        return makeError("Failed to retrieve SPIR-V from Slang compile request");
    }

    SpirvByteCode result;
    result.resize(spirvCode->getBufferSize() / sizeof(uint32_t));
    std::memcpy(result.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());

    return result;
}

SlangStage SlangCompiler::toSlangStage(Gpu::ShaderStageFlagBits stage) const
{
    using enum Gpu::ShaderStageFlagBits;
    switch (stage) {
    case VertexBit:
        return SLANG_STAGE_VERTEX;
    case GeometryBit:
        return SLANG_STAGE_GEOMETRY;
    case FragmentBit:
        return SLANG_STAGE_FRAGMENT;
    case ComputeBit:
        return SLANG_STAGE_COMPUTE;
    default:
        return SLANG_STAGE_NONE;
    }
}

SlangCompiler::CompilationResult SlangCompiler::makeError(std::string message) const
{
    return std::unexpected(std::move(message));
}
} // namespace Cory
