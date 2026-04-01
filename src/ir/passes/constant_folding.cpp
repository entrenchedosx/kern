#include "ir/passes/passes.hpp"

#include <regex>

namespace kern {

void runConstantFolding(IRProgram& program) {
    std::regex re(R"(\blet\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([0-9]+)\s*([\+\-\*\/])\s*([0-9]+))");
    for (auto& mod : program.modules) {
        std::string out;
        out.reserve(mod.source.size());
        size_t offset = 0;
        for (std::sregex_iterator it(mod.source.begin(), mod.source.end(), re), end; it != end; ++it) {
            const auto& m = *it;
            out.append(mod.source.substr(offset, m.position() - offset));
            int64_t lhs = std::stoll(m[2].str());
            int64_t rhs = std::stoll(m[4].str());
            char op = m[3].str()[0];
            int64_t val = 0;
            bool ok = true;
            if (op == '+') val = lhs + rhs;
            else if (op == '-') val = lhs - rhs;
            else if (op == '*') val = lhs * rhs;
            else if (op == '/') {
                if (rhs == 0) ok = false;
                else val = lhs / rhs;
            }
            if (ok) {
                const std::string name = m[1].str();
                mod.foldedConstants[name] = val;
                out += "let " + name + " = " + std::to_string(val);
            } else {
                out += m.str();
            }
            offset = m.position() + m.length();
        }
        out.append(mod.source.substr(offset));
        mod.source.swap(out);
    }
}

} // namespace kern
