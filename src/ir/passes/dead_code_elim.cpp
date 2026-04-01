#include "ir/passes/passes.hpp"

#include <sstream>

namespace kern {

void runDeadCodeElimination(IRProgram& program) {
    for (auto& mod : program.modules) {
        std::istringstream in(mod.source);
        std::ostringstream out;
        std::string line;
        bool skip = false;
        while (std::getline(in, line)) {
            std::string trimmed = line;
            while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) trimmed.erase(trimmed.begin());
            if (trimmed.rfind("// @enddead", 0) == 0) {
                skip = false;
                continue;
            }
            if (trimmed.rfind("// @dead", 0) == 0) {
                skip = true;
                continue;
            }
            if (!skip) out << line << "\n";
        }
        mod.source = out.str();
    }
}

} // namespace kern
