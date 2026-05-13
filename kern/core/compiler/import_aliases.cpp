#include "compiler/import_aliases.hpp"
#include "stdlib_modules.hpp"

#include <cctype>

namespace kern {
namespace {

static std::string normalizeImportKey(std::string s) {
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    if (s.size() >= 4 && s.compare(s.size() - 4, 4, ".kn") == 0) {
        s.resize(s.size() - 4);
    }
    return s;
}

} // namespace

bool isVirtualResolvedImport(const std::string& rawImport) {
    std::string p = normalizeImportKey(rawImport);
    if (p.empty()) return false;

    // stdlib modules (import "math", import "sys", import "std.v1.math", ...)
    if (isStdlibModuleName(p)) return true;

    // graphics / game — resolved at runtime when built with Raylib (still valid Kern).
    if (p == "game" || p == "g2d" || p == "g3d" || p == "2dgraphics") return true;

    // system modules (same names as import_resolution.cpp)
    if (p == "process" || p == "input" || p == "vision" || p == "render") return true;

    // kern:: aliases
    static const char* kPrefix = "kern::";
    const size_t plen = 5;
    if (p.size() > plen && p.compare(0, plen, kPrefix) == 0) {
        std::string rest = p.substr(plen);
        if (rest == "process" || rest == "input" || rest == "vision" || rest == "render") return true;
        if (isStdlibModuleName(rest)) return true;
    }

    return false;
}

bool isIntentionalMissingImportFixture(const std::string& rawImport) {
    std::string p = normalizeImportKey(rawImport);
    return p.find("__does_not_exist__") != std::string::npos;
}

} // namespace kern
