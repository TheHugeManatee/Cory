#include "SlangCompilerTools.hpp"

#include <magic_enum/magic_enum.hpp>
#include <slang.h>

#include <fmt/format.h>
#include <iterator>
#include <utility>

namespace Cory {
namespace detail {

struct DumpWriter {
    std::string buffer;
    int indentWidth{2};
    int indentLevel{0};

    struct IndentScope {
        DumpWriter &writer;
        explicit IndentScope(DumpWriter &writerIn)
            : writer(writerIn)
        {
            ++writer.indentLevel;
        }
        ~IndentScope() { --writer.indentLevel; }
    };
    auto indent() { return IndentScope(*this); }

    template <typename... Args> void append(fmt::format_string<Args...> fmtString, Args &&...args)
    {
        buffer.append("|| ");
        buffer.append(static_cast<size_t>(indentLevel * indentWidth), ' ');
        fmt::format_to(std::back_inserter(buffer), fmtString, std::forward<Args>(args)...);
    }
    template <typename... Args> void line(fmt::format_string<Args...> fmtString, Args &&...args)
    {
        append(std::forward<fmt::format_string<Args...>>(fmtString), std::forward<Args>(args)...);
        buffer.push_back('\n');
    }
    void blank() { buffer.append("|| \n"); }
};

template <typename T> std::string enumLabel(T value)
{
    const auto name = magic_enum::enum_name(value);
    if (!name.empty()) {
        return std::string(name);
    }
    return std::to_string(static_cast<int>(value));
}

void dumpType(slang::TypeReflection *type,
              DumpWriter &out,
              int depthRemaining,
              const SlangCompilerTools::DumpOptions &options);
void dumpTypeLayout(slang::TypeLayoutReflection *typeLayout,
                    DumpWriter &out,
                    int depthRemaining,
                    const SlangCompilerTools::DumpOptions &options);
void dumpVarLayout(slang::VariableLayoutReflection *varLayout,
                   DumpWriter &out,
                   int depthRemaining,
                   const SlangCompilerTools::DumpOptions &options);

void dumpVarLayout(slang::VariableLayoutReflection *varLayout,
                   DumpWriter &out,
                   int depthRemaining,
                   const SlangCompilerTools::DumpOptions &options)
{
    if (!varLayout) {
        out.line("- <null>");
        return;
    }

    out.line("- {}", varLayout->getName() ? varLayout->getName() : "<unnamed>");

    const int catCount = varLayout->getCategoryCount();
    if (catCount > 0) {
        auto indent = out.indent();
        out.line("Categories ({}):", catCount);
        auto categoriesIndent = out.indent();
        for (int ci = 0; ci < catCount; ++ci) {
            const auto cat = varLayout->getCategoryByIndex(ci);
            const size_t offset = varLayout->getOffset(cat);
            out.line("- {} offset={}", enumLabel(cat), offset);
        }
    }

    if (options.includeTypeLayouts && depthRemaining > 0) {
        auto indent = out.indent();
        dumpTypeLayout(varLayout->getTypeLayout(), out, depthRemaining - 1, options);
    }
}

void dumpTypeLayout(slang::TypeLayoutReflection *typeLayout,
                    DumpWriter &out,
                    int depthRemaining,
                    const SlangCompilerTools::DumpOptions &options)
{
    if (!typeLayout) {
        out.line("TypeLayout: <null>");
        return;
    }

    out.line("TypeLayout: {} size={}",
             typeLayout->getName() ? typeLayout->getName() : "<unnamed>",
             typeLayout->getSize());

    if (options.includeTypes && depthRemaining > 0) {
        if (auto *type = typeLayout->getType()) {
            auto indent = out.indent();
            dumpType(type, out, depthRemaining - 1, options);
        }
    }

    const int fieldCount = typeLayout->getFieldCount();
    if (options.includeVariables && fieldCount > 0 && depthRemaining > 0) {
        auto indent = out.indent();
        for (int fi = 0; fi < fieldCount; ++fi) {
            dumpVarLayout(typeLayout->getFieldByIndex(fi), out, depthRemaining - 1, options);
        }
    }
}

void dumpType(slang::TypeReflection *type,
              DumpWriter &out,
              int depthRemaining,
              const SlangCompilerTools::DumpOptions &options)
{
    if (!type) {
        out.line("Type: <null>");
        return;
    }

    out.line("Type: {} kind={}",
             type->getName() ? type->getName() : "<unnamed>",
             enumLabel(type->getKind()));

    if (type->getKind() == slang::TypeReflection::Kind::Struct && depthRemaining > 0) {
        auto indent = out.indent();
        const int fieldCount = type->getFieldCount();
        for (int fi = 0; fi < fieldCount; ++fi) {
            auto field = type->getFieldByIndex(fi);
            out.line("- {}", field->getName() ? field->getName() : "<unnamed>");
        }
    }

    if (depthRemaining > 0) {
        const auto kind = type->getKind();
        const bool hasElementType = (kind == slang::TypeReflection::Kind::ConstantBuffer) ||
                                    (kind == slang::TypeReflection::Kind::ParameterBlock) ||
                                    (kind == slang::TypeReflection::Kind::Resource) ||
                                    (kind == slang::TypeReflection::Kind::ShaderStorageBuffer) ||
                                    (kind == slang::TypeReflection::Kind::TextureBuffer);
        if (hasElementType) {
            if (auto *elementType = type->getElementType()) {
                auto indent = out.indent();
                dumpType(elementType, out, depthRemaining - 1, options);
            }
        }
    }
}

void dumpGlobalParams(slang::TypeLayoutReflection *globals,
                      DumpWriter &out,
                      const SlangCompilerTools::DumpOptions &options)
{
    out.line("Global Parameters:");
    if (!globals) {
        auto indent = out.indent();
        out.line("<none>");
        return;
    }

    auto indent = out.indent();
    dumpTypeLayout(globals, out, options.maxDepth, options);
}
} // namespace detail

std::string SlangCompilerTools::dumpProgramLayout(slang::IComponentType *program,
                                                  int targetIndex,
                                                  const DumpOptions &options)
{
    detail::DumpWriter out;
    if (!program) {
        out.line("Error: program is null");
        return out.buffer;
    }

    auto *layout = program->getLayout(targetIndex);
    if (!layout) {
        out.line("Error: failed to get program layout");
        return out.buffer;
    }

    out.line("=== Slang Reflection Dump ===");
    out.line("Target Index: {}", targetIndex);
    out.blank();

    if (options.includeGlobalParams || options.includeVariables) {
        detail::dumpGlobalParams(layout->getGlobalParamsTypeLayout(), out, options);
        out.blank();
    }

    out.append("=== End Reflection Dump ===");
    return out.buffer;
}

std::string SlangCompilerTools::dumpTypeLayout(slang::TypeLayoutReflection *typeLayout,
                                               const DumpOptions &options)
{
    detail::DumpWriter out;
    detail::dumpTypeLayout(typeLayout, out, options.maxDepth, options);
    return out.buffer;
}

std::string SlangCompilerTools::dumpType(slang::TypeReflection *type, const DumpOptions &options)
{
    detail::DumpWriter out;
    detail::dumpType(type, out, options.maxDepth, options);
    return out.buffer;
}

} // namespace Cory
