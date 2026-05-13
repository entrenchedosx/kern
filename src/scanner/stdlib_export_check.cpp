#include "scanner/stdlib_export_check.hpp"
#include "stdlib_stdv1_exports.hpp"
#include "vm/builtins.hpp"

#include <unordered_set>

namespace kern {

void emitStdlibExportDiagnostics(ErrorReporter& rep) {
    std::unordered_set<std::string> known;
    for (const auto& n : getBuiltinNames()) known.insert(n);
    for (const auto& n : getBuiltinExtraGlobalNames()) known.insert(n);

    const auto& exports = stdV1NamedExports();
    for (const auto& mod : exports) {
        const std::string& moduleName = mod.first;
        for (const auto& pr : mod.second) {
            const std::string& exportKey = pr.first;
            const std::string& target = pr.second;
            if (known.find(target) == known.end()) {
                rep.reportCompileError(ErrorCategory::Other, 0, 0,
                    "stdlib export \"" + moduleName + "\" key \"" + exportKey + "\" maps to unknown name \"" + target + "\"",
                    "Add the name to getBuiltinNames() or correct the mapping in stdV1NamedExports().",
                    "SCAN-STDLIB-EXPORT",
                    "Cross-layer check: every std.v1 export must resolve to a registered builtin (or extra global alias).");
            }
        }
    }
}

} // namespace kern
