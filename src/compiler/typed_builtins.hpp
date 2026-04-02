/* *
 * Narrow return-type map for --strict-types semantic checks (Phase 2 roadmap).
 * Append-only: add new rows; do not rename keys (stable for diagnostics).
 * Not a full type system — only simple `let x: T = name(...)` / assignment checks.
 */
#ifndef KERN_COMPILER_TYPED_BUILTINS_HPP
#define KERN_COMPILER_TYPED_BUILTINS_HPP

#include <string>
#include <unordered_map>

namespace kern {

inline const std::unordered_map<std::string, std::string>& builtinStrictReturnTypes() {
    static const std::unordered_map<std::string, std::string> k = {
        {"sqrt", "float"},
        {"sin", "float"},
        {"cos", "float"},
        {"tan", "float"},
        {"floor", "float"},
        {"ceil", "float"},
        {"round", "float"},
        {"round_to", "float"},
        {"abs", "float"},
        {"min", "float"},
        {"max", "float"},
        {"pow", "float"},
        {"log", "float"},
        {"atan2", "float"},
        {"sign", "float"},
        {"clamp", "float"},
        {"lerp", "float"},
        {"deg_to_rad", "float"},
        {"rad_to_deg", "float"},
        {"int", "int"},
        {"float", "float"},
        {"str", "string"},
        {"bool", "bool"},
        {"len", "int"},
        {"chr", "string"},
        {"ord", "int"},
        {"hex", "string"},
        {"bin", "string"},
    };
    return k;
}

} // namespace kern

#endif
