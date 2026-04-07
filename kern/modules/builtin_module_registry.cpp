#include "builtin_module_registry.hpp"

namespace kern {
namespace {

void builtinModuleNoopInit() {}

} // namespace

const std::vector<KernBuiltinModule>& get_builtin_modules() {
#ifdef KERN_BUILD_GAME
    static const std::vector<KernBuiltinModule> kTable = {
        {"input", builtinModuleNoopInit},
        {"vision", builtinModuleNoopInit},
        {"render", builtinModuleNoopInit},
        {"g2d", builtinModuleNoopInit},
        {"g3d", builtinModuleNoopInit},
    };
#else
    static const std::vector<KernBuiltinModule> kTable = {
        {"input", builtinModuleNoopInit},
        {"vision", builtinModuleNoopInit},
        {"render", builtinModuleNoopInit},
    };
#endif
    return kTable;
}

} // namespace kern
