/* *
 * kern import resolution – shared implementation for main and REPL.
 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include "import_resolution.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include "compiler/ast.hpp"
#include "vm/builtins.hpp"
#include "stdlib_modules.hpp"
#include "process/process_module.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#include "modules/g2d/g2d.h"
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace kern {

namespace {
namespace fs = std::filesystem;

struct ImportState {
    std::unordered_map<std::string, ValuePtr> moduleCache;
    std::unordered_set<std::string> loading;
};

static ImportState g_importState;

static std::string normalizeKey(const std::string& path) {
    std::string out = path;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    return out;
}

static bool pathContainsTraversal(const fs::path& p) {
    for (const auto& part : p) {
        if (part == "..") return true;
    }
    return false;
}

static bool isSubpathOf(const fs::path& candidate, const fs::path& root) {
    std::error_code ec1;
    std::error_code ec2;
    fs::path c = fs::weakly_canonical(candidate, ec1);
    fs::path r = fs::weakly_canonical(root, ec2);
    if (ec1 || ec2) return false;
    auto cIt = c.begin();
    auto rIt = r.begin();
    while (rIt != r.end() && cIt != c.end()) {
        if (*rIt != *cIt) return false;
        ++rIt;
        ++cIt;
    }
    return rIt == r.end();
}

/* *
 * resolve file path using deterministic order:
 * 1) current working directory
 * 2) kERN_LIB (if set)
 * rejects traversal and absolute paths outside allowed roots.
 */
static std::string resolveImportPath(const std::string& importPath, std::string* errOut = nullptr) {
    if (importPath.empty()) {
        if (errOut) *errOut = "empty module path";
        return "";
    }
    fs::path raw(importPath);
    if (pathContainsTraversal(raw)) {
        if (errOut) *errOut = "path traversal is not allowed";
        return "";
    }

    std::vector<fs::path> roots;
    std::error_code ec;
    fs::path cwd = fs::current_path(ec);
    if (!ec) roots.push_back(cwd);
    const char* lib = std::getenv("KERN_LIB");
    if (lib && lib[0]) {
        roots.emplace_back(lib);
    }

    // absolute paths are permitted only if under an allowed root.
    if (raw.is_absolute()) {
        for (const auto& root : roots) {
            if (isSubpathOf(raw, root) && fs::exists(raw)) {
                return fs::weakly_canonical(raw).string();
            }
        }
        if (errOut) *errOut = "absolute import path is outside allowed roots";
        return "";
    }

    for (const auto& root : roots) {
        fs::path candidate = root / raw;
        if (!fs::exists(candidate)) continue;
        if (!isSubpathOf(candidate, root)) continue;
        std::error_code ecCan;
        fs::path can = fs::weakly_canonical(candidate, ecCan);
        if (ecCan) continue;
        return can.string();
    }

    if (errOut) *errOut = "file not found in CWD or KERN_LIB";
    return "";
}

static ValuePtr makeModuleFromGlobalDelta(VM& vm, const std::unordered_map<std::string, ValuePtr>& before) {
    std::unordered_map<std::string, ValuePtr> exports;
    std::unordered_set<std::string> builtinNames;
    insertAllBuiltinNamesForAnalysis(builtinNames);
    builtinNames.insert("__import");

    const auto after = vm.getGlobalsSnapshot();
    for (const auto& kv : after) {
        const std::string& name = kv.first;
        if (builtinNames.find(name) != builtinNames.end()) continue;
        auto itBefore = before.find(name);
        if (itBefore == before.end() || itBefore->second != kv.second) {
            exports[name] = kv.second ? kv.second : std::make_shared<Value>(Value::nil());
        }
    }
    return std::make_shared<Value>(Value::fromMap(std::move(exports)));
}

static Value runImportBuiltin(VM* v, std::vector<ValuePtr> args) {
    if (!v || args.empty() || !args[0] || args[0]->type != Value::Type::STRING)
        return Value::nil();
    std::string path = std::get<std::string>(args[0]->data);
    std::string base = path;
    if (base.size() >= 4 && base.compare(base.size() - 4, 4, ".kn") == 0)
        base = base.substr(0, base.size() - 4);

    // fast cache by requested module key.
    std::string cacheKey = normalizeKey(base);
    auto cacheIt = g_importState.moduleCache.find(cacheKey);
    if (cacheIt != g_importState.moduleCache.end()) {
        return cacheIt->second ? *cacheIt->second : Value::nil();
    }

#ifdef KERN_BUILD_GAME
    if (path == "game" || path == "game.kn" || base == "game") {
        ValuePtr mod = createGameModule(*v);
        g_importState.moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (path == "g2d" || path == "g2d.kn" || path == "2dgraphics" || path == "2dgraphics.kn" || base == "g2d" || base == "2dgraphics") {
        ValuePtr mod = create2dGraphicsModule(*v);
        g_importState.moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
#endif
    if (base == "g2d" || base == "2dgraphics") {
        std::cerr << "import: 'g2d' (2D graphics) is not available in this build." << std::endl;
        std::cerr << "  To enable: install Raylib (e.g. vcpkg install raylib), then reconfigure and rebuild." << std::endl;
        return Value::nil();
    }
    if (base == "game") {
        std::cerr << "import: 'game' is not available. Rebuild with Raylib." << std::endl;
        return Value::nil();
    }
    if (base == "process") {
        ValuePtr mod = createProcessModule(*v);
        g_importState.moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (isStdlibModuleName(base)) {
        ValuePtr mod = createStdlibModule(*v, base);
        g_importState.moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }

    if (path.find('.') == std::string::npos)
        path += ".kn";
    std::string resolveErr;
    std::string resolved = resolveImportPath(path, &resolveErr);
    if (resolved.empty()) {
        std::cerr << "import: failed to resolve '" << path << "': " << resolveErr << std::endl;
        return Value::nil();
    }
    std::string resolvedKey = normalizeKey(resolved);
    auto resolvedIt = g_importState.moduleCache.find(resolvedKey);
    if (resolvedIt != g_importState.moduleCache.end()) {
        g_importState.moduleCache[cacheKey] = resolvedIt->second;
        return resolvedIt->second ? *resolvedIt->second : Value::nil();
    }
    if (g_importState.loading.find(resolvedKey) != g_importState.loading.end()) {
        std::cerr << "import: cyclic module load detected: " << resolved << std::endl;
        return Value::nil();
    }

    g_importState.loading.insert(resolvedKey);
    std::ifstream f(resolved, std::ios::in | std::ios::binary);
    if (!f) {
        g_importState.loading.erase(resolvedKey);
        return Value::nil();
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string source = buf.str();
    try {
        const auto beforeGlobals = v->getGlobalsSnapshot();
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        CodeGenerator gen;
        Bytecode code = gen.generate(std::move(program));
        v->runSubScript(code, gen.getConstants(), gen.getValueConstants());
        ValuePtr fileModule = makeModuleFromGlobalDelta(*v, beforeGlobals);
        g_importState.moduleCache[resolvedKey] = fileModule;
        g_importState.moduleCache[cacheKey] = fileModule;
        g_importState.loading.erase(resolvedKey);
        return fileModule ? *fileModule : Value::nil();
    } catch (const LexerError& e) {
        std::cerr << "import: " << resolved << ": " << e.what() << " (line " << e.line << ")" << std::endl;
    } catch (const ParserError& e) {
        std::cerr << "import: " << resolved << ": " << e.what() << std::endl;
    } catch (const VMError& e) {
        std::cerr << "import: " << resolved << " runtime error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "import: " << resolved << ": " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "import: " << resolved << ": unknown error" << std::endl;
    }
    g_importState.loading.erase(resolvedKey);
    return Value::nil();
}

} // namespace

void registerImportBuiltin(VM& vm) {
    auto importFn = std::make_shared<FunctionObject>();
    importFn->isBuiltin = true;
    importFn->builtinIndex = IMPORT_BUILTIN_INDEX;
    vm.setGlobal("__import", std::make_shared<Value>(Value::fromFunction(importFn)));
    vm.registerBuiltin(IMPORT_BUILTIN_INDEX, [](VM* v, std::vector<ValuePtr> args) { return runImportBuiltin(v, args); });
}

} // namespace kern
