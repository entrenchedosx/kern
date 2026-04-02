#include "scanner/builtin_registry_check.hpp"
#include "vm/builtins.hpp"

#include <unordered_map>

namespace kern {

void emitBuiltinRegistryDiagnostics(VM& vm, ErrorReporter& rep) {
    const std::vector<std::string>& names = getBuiltinNames();
    std::unordered_map<std::string, int> firstIndex;
    for (size_t i = 0; i < names.size(); ++i) {
        const std::string& n = names[i];
        if (n.empty()) {
            rep.reportCompileError(ErrorCategory::Other, 0, 0,
                "Empty builtin name at index " + std::to_string(i),
                "Remove the stray entry or assign a valid identifier in getBuiltinNames().",
                "SCAN-REG-EMPTY",
                "Builtin names must be non-empty; empty entries break stable index assignment.");
            continue;
        }
        auto it = firstIndex.find(n);
        if (it != firstIndex.end()) {
            rep.reportCompileError(ErrorCategory::Other, 0, 0,
                "Duplicate builtin name \"" + n + "\" at indices " + std::to_string(it->second) + " and " +
                    std::to_string(static_cast<int>(i)),
                "Builtin registration is append-only; each name must appear once.",
                "SCAN-REG-DUP",
                "Duplicate names make builtin indices ambiguous for tooling and the VM.");
        } else {
            firstIndex[n] = static_cast<int>(i);
        }
    }

    for (size_t i = 0; i < names.size(); ++i) {
        if (!vm.builtinSlotFilled(i)) {
            rep.reportCompileError(ErrorCategory::Other, 0, 0,
                "Builtin index " + std::to_string(i) + " (\"" + names[i] + "\") has no VM registration",
                "Ensure registerAllBuiltins registers one handler per getBuiltinNames() entry in order.",
                "SCAN-REG-MISS",
                "Each name in getBuiltinNames() must have a matching makeBuiltin registration at the same index.");
        }
    }
}

} // namespace kern
