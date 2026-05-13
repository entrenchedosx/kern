#include "ir/passes/passes.hpp"

#include <regex>

namespace kern {

void runBasicInlining(IRProgram& program) {
    for (auto& mod : program.modules) {
        // very small source-level inliner: replace inline_const("x", 123) with let x = 123.
        std::regex re(R"re(\binline_const\(\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*,\s*([0-9]+)\s*\))re");
        mod.source = std::regex_replace(mod.source, re, "let $1 = $2");
    }
}

} // namespace kern
