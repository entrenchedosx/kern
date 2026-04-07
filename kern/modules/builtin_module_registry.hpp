#pragma once

#include <vector>

namespace kern {

/** Describes a built-in native module entry (future: dynamic plugins, versioned ABI). */
struct KernBuiltinModule {
    const char* name;
    void (*init)();
};

/** Modules shipped in this binary; `init` is reserved for future one-time setup hooks. */
const std::vector<KernBuiltinModule>& get_builtin_modules();

} // namespace kern
