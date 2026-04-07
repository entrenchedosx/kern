#include "ir/typed_ir_builder.hpp"

#include <regex>
#include <sstream>

namespace kern {

namespace {

static std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

static bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

} // namespace

TypedIRModule buildTypedIRModule(const std::string& modulePath, const std::string& source) {
    TypedIRModule m;
    m.path = modulePath;

    TypedIRFunction top;
    top.name = "__module_init__";
    top.returnType = "void";
    top.blocks.push_back({"entry", {}, {}});

    std::regex fnRe(R"re(^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*(?::\s*([A-Za-z_][A-Za-z0-9_\*]*))?)re");
    std::regex importRe(R"re(\bimport\s*(?:\(\s*)?"([^"]+)")re");
    std::regex assignRe(R"re(^\s*(?:let|var|const)?\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+)$)re");
    std::regex callRe(R"re(^\s*([A-Za-z_][A-Za-z0-9_\.]*)\s*\((.*)\)\s*;?\s*$)re");

    std::istringstream iss(source);
    std::string line;
    int lineNo = 0;
    while (std::getline(iss, line)) {
        ++lineNo;
        std::string t = trim(line);
        if (t.empty() || startsWith(t, "//")) continue;

        std::smatch mm;
        if (std::regex_search(t, mm, importRe) && mm.size() >= 2) {
            m.imports.push_back(mm[1].str());
            top.blocks[0].instructions.push_back({TypedIROp::Import, "", mm[1].str(), "", lineNo});
            continue;
        }

        if (std::regex_search(t, mm, fnRe) && mm.size() >= 2) {
            TypedIRFunction fn;
            fn.name = mm[1].str();
            fn.returnType = mm.size() >= 4 && mm[3].matched ? mm[3].str() : "dynamic";
            std::string params = mm.size() >= 3 ? trim(mm[2].str()) : "";
            if (!params.empty()) {
                std::stringstream ss(params);
                std::string p;
                while (std::getline(ss, p, ',')) {
                    p = trim(p);
                    if (p.empty()) continue;
                    size_t colon = p.find(':');
                    if (colon != std::string::npos) p = trim(p.substr(0, colon));
                    fn.params.push_back(p);
                }
            }
            fn.blocks.push_back({"entry", {}, {}});
            m.functions.push_back(std::move(fn));
            continue;
        }

        if (startsWith(t, "return")) {
            top.blocks[0].instructions.push_back({TypedIROp::Return, "", t.substr(6), "", lineNo});
            continue;
        }

        if (std::regex_search(t, mm, assignRe) && mm.size() >= 3) {
            top.blocks[0].instructions.push_back({TypedIROp::Assign, trim(mm[1].str()), trim(mm[2].str()), "", lineNo});
            continue;
        }

        if (std::regex_search(t, mm, callRe) && mm.size() >= 2) {
            top.blocks[0].instructions.push_back({TypedIROp::Call, "", trim(mm[1].str()), trim(mm[2].str()), lineNo});
            continue;
        }

        top.blocks[0].instructions.push_back({TypedIROp::Nop, "", t, "", lineNo});
    }

    // make sure each function has a terminal edge/return-like semantics in CFG.
    for (auto& fn : m.functions) {
        if (fn.blocks.empty()) fn.blocks.push_back({"entry", {}, {}});
        if (fn.blocks[0].instructions.empty() || fn.blocks[0].instructions.back().op != TypedIROp::Return) {
            fn.blocks[0].instructions.push_back({TypedIROp::Return, "", "null", "", 0});
        }
    }

    m.functions.insert(m.functions.begin(), std::move(top));
    return m;
}

void buildTypedIRForProgram(IRProgram& program) {
    program.typedModules.clear();
    for (auto& mod : program.modules) {
        mod.typed = buildTypedIRModule(mod.path, mod.source);
        program.typedModules.push_back(mod.typed);
    }
}

} // namespace kern
