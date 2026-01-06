#pragma once

#include <string>

namespace slang {
class IComponentType;
class TypeLayoutReflection;
class TypeReflection;
} // namespace slang

namespace Cory {

class SlangCompilerTools {
  public:
    struct DumpOptions {
        bool includeGlobalParams{true};
        bool includeVariables{true};
        bool includeTypeLayouts{true};
        bool includeTypes{true};
        int maxDepth{6};
    };
    static std::string dumpProgramLayout(slang::IComponentType *program,
                                         int targetIndex = 0,
                                         const DumpOptions &options = {});
    static std::string dumpTypeLayout(slang::TypeLayoutReflection *typeLayout,
                                      const DumpOptions &options = {});
    static std::string dumpType(slang::TypeReflection *type, const DumpOptions &options = {});
};

} // namespace Cory
