/* *
 * kern import resolution – shared implementation for main and REPL.
 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#include "import_resolution.hpp"
#include "platform/env_compat.hpp"
#include "platform/kern_env.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include "compiler/ast.hpp"
#include "vm/builtins.hpp"
#include "stdlib_modules.hpp"
#include "process/process_module.hpp"
#include "system/input_module.hpp"
#include "system/vision_module.hpp"
#include "system/render_module.hpp"
#include "builtin_module_registry.hpp"
#include "system/runtime_services.hpp"
#include "errors/errors.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#include "g2d/g2d.h"
#include "g3d/g3d.h"
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <utility>

namespace kern {

namespace {
namespace fs = std::filesystem;

static void addBundledStdlibRootIfPresent(std::vector<fs::path>& roots) {
    std::error_code ec;
    if (auto kr = tryResolveKernRoot()) {
        if (!isResolvedToolchainRoot(*kr)) return;
        fs::path libKern = *kr / "lib" / "kern";
        if (fs::is_directory(libKern, ec) && !ec) roots.push_back(*kr);
    }
}

/// Per-process import cache and related state (no loose globals).
class ImportResolver {
public:
    std::unordered_map<std::string, ValuePtr> moduleCache;
    std::unordered_set<std::string> loading;
    VM* cacheOwner = nullptr;
    std::shared_ptr<RuntimeServices> runtimeServices = std::make_shared<RuntimeServices>();
    EmbeddedModuleProvider embeddedProvider = nullptr;
};

static ImportResolver& getImportResolver() {
    thread_local ImportResolver instance;
    return instance;
}

[[maybe_unused]] static const auto& g_builtinModuleCatalog = get_builtin_modules();

static std::string normalizeKey(const std::string& path) {
    std::string out = path;
    for (char& c : out) {
        if (c == '\\') c = '/';
    }
    return out;
}

[[noreturn]] static Value importFailure(const std::string& msg, VMErrorCode code) {
    throw VMError(msg, 0, 0, 8, static_cast<int>(code));
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

static std::string regexEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '\\': case '^': case '$': case '.': case '|': case '?':
            case '*': case '+': case '(': case ')': case '[': case '{':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

/** GitHub-style package keys for kargo: owner/repo or github.com/owner/repo (lowercase). */
static std::string kargoPackagePathKey(const std::string& path) {
    std::string p;
    p.reserve(path.size());
    for (char c : path) {
        if (c >= 'A' && c <= 'Z')
            p.push_back(static_cast<char>(c + 32));
        else
            p.push_back(c);
    }
    static const char pref[] = "github.com/";
    const size_t plen = sizeof(pref) - 1;
    if (p.size() > plen && p.compare(0, plen, pref) == 0) p = p.substr(plen);
    if (p.empty() || p.find("..") != std::string::npos) return "";
    const std::size_t slash = p.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= p.size()) return "";
    if (p.find('/', slash + 1) != std::string::npos) return "";
    auto okSeg = [](const std::string& s) {
        if (s.empty()) return false;
        for (char ch : s) {
            const auto u = static_cast<unsigned char>(ch);
            if (!(std::isalnum(u) || ch == '-' || ch == '_' || ch == '.')) return false;
        }
        return true;
    };
    const std::string a = p.substr(0, slash);
    const std::string b = p.substr(slash + 1);
    if (!okSeg(a) || !okSeg(b)) return "";
    return p;
}

static std::string resolveInstalledPackageMain(const std::string& packageName, std::string* errOut = nullptr) {
    std::error_code ec;
    std::error_code ecPath;
    fs::path pathsFile = resolvePackagePathsJsonFile();
    if (!fs::exists(pathsFile, ecPath)) {
        if (errOut) *errOut = "config/package-paths.json not found (set KERN_HOME or use kargo install)";
        return "";
    }
    std::ifstream f(pathsFile, std::ios::in | std::ios::binary);
    if (!f) {
        if (errOut) *errOut = "cannot read config/package-paths.json";
        return "";
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string json = buf.str();
    const std::string escaped = regexEscape(packageName);
    std::regex mainRe("\"" + escaped + "\"\\s*:\\s*\\{[\\s\\S]*?\"main\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    if (!std::regex_search(json, m, mainRe) || m.size() < 2) {
        if (errOut) *errOut = "package not installed in config/package-paths.json";
        return "";
    }
    fs::path mainPath(m[1].str());
    std::error_code ecCan;
    fs::path can;
    if (mainPath.is_absolute()) {
        can = fs::weakly_canonical(mainPath, ecCan);
        if (ecCan || !fs::exists(can)) {
            if (errOut) *errOut = "installed package main path does not exist";
            return "";
        }
    } else {
        if (pathContainsTraversal(mainPath)) {
            if (errOut) *errOut = "invalid package main path in package-paths.json";
            return "";
        }
        auto kr = tryResolveKernRoot();
        if (!kr) {
            if (errOut) *errOut = "cannot resolve Kern root for package-paths.json entry";
            return "";
        }
        can = fs::weakly_canonical(*kr / mainPath, ecCan);
        if (ecCan || !fs::exists(can)) {
            if (errOut) *errOut = "installed package main path does not exist";
            return "";
        }
        if (!isSubpathOf(can, *kr)) {
            if (errOut) *errOut = "package main path escapes Kern root";
            return "";
        }
    }
    return can.string();
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
    const char* lib = kernGetEnv("KERN_LIB");
    if (lib && lib[0]) {
        roots.emplace_back(lib);
    } else {
        addBundledStdlibRootIfPresent(roots);
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
    // Do not strip exports just because a name matches a builtin. A .kn file may define `def require`
    // (or any other name) that shadows the builtin in globals; that definition must appear on the
    // module map or imports see a false API (callable in-host but missing on the module object).
    // Only omit names that must never be re-exported from file modules.
    static const std::unordered_set<std::string> kNeverExportFromFileModule = {
        "__import",
    };

    const auto after = vm.getGlobalsSnapshot();
    for (const auto& kv : after) {
        const std::string& name = kv.first;
        if (kNeverExportFromFileModule.find(name) != kNeverExportFromFileModule.end()) continue;
        auto itBefore = before.find(name);
        if (itBefore == before.end() || itBefore->second != kv.second) {
            exports[name] = kv.second ? kv.second : std::make_shared<Value>(Value::nil());
        }
    }
    return std::make_shared<Value>(Value::fromMap(std::move(exports)));
}

static bool compileAndRunImportedSource(VM* v, const std::string& displayPath, const std::string& source,
                                       const std::string& cacheKey, const std::string& resolvedKey,
                                       ValuePtr& outModule) {
    try {
        const auto beforeGlobals = v->getGlobalsSnapshot();
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        CodeGenerator gen;
        Bytecode code = gen.generate(std::move(program));
        v->runSubScript(code, gen.getConstants(), gen.getValueConstants(), displayPath);
        outModule = makeModuleFromGlobalDelta(*v, beforeGlobals);
        getImportResolver().moduleCache[resolvedKey] = outModule;
        getImportResolver().moduleCache[cacheKey] = outModule;
        getImportResolver().loading.erase(resolvedKey);
        return true;
    } catch (const LexerError& e) {
        ErrorReporterImportScope scope(g_errorReporter, displayPath, source);
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, e.what(),
            lexerCompileErrorHint(), "IMP-LEX", importLexDetail(displayPath));
    } catch (const ParserError& e) {
        std::string msg(e.what());
        ErrorReporterImportScope scope(g_errorReporter, displayPath, source);
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, msg,
            parserCompileErrorHint(msg), "IMP-PARSE", importParseDetail(displayPath, msg));
    } catch (const CodegenError& e) {
        ErrorReporterImportScope scope(g_errorReporter, displayPath, source);
        g_errorReporter.reportCompileError(ErrorCategory::Other, e.line, 0,
            std::string("import codegen failed: ") + e.what(),
            "The compiler hit an unsupported AST/operator case while lowering to bytecode.",
            "IMP-CODEGEN",
            internalFailureDetail("import code generation (`" + displayPath + "`)"));
    } catch (const VMError& e) {
        ErrorReporterImportScope scope(g_errorReporter, displayPath, source);
        std::vector<StackFrame> stack;
        for (const auto& f : v->getCallStackSlice())
            stack.push_back({f.functionName, f.filePath, f.line, f.column});
        std::string hint(vmRuntimeErrorHint(e.category, e.code));
        g_errorReporter.reportRuntimeError(vmErrorCategory(e.category), e.line, e.column, e.what(), stack, hint,
            vmErrorCodeString(e.category, e.code),
            std::string(vmRuntimeErrorDetail(e.category, e.code)) + "\n" + importVmDetail(displayPath),
            e.lineEnd, e.columnEnd);
    } catch (const std::exception& e) {
        ErrorReporterImportScope scope(g_errorReporter, displayPath, source);
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("import failed: ") + e.what(),
            "Unexpected exception during module load.", "IMP-INTERNAL",
            internalFailureDetail("import (`" + displayPath + "`)") + "\n" + importEmbeddedFailureDetail(displayPath));
    } catch (...) {
        ErrorReporterImportScope scope(g_errorReporter, displayPath, source);
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "import failed: unknown exception",
            "Non-typed throw while loading a module.", "IMP-INTERNAL-UNKNOWN",
            importEmbeddedFailureDetail(displayPath));
    }
    getImportResolver().loading.erase(resolvedKey);
    return false;
}

static Value runImportBuiltin(VM* v, std::vector<ValuePtr> args) {
    if (!v || args.empty() || !args[0] || args[0]->type != Value::Type::STRING)
        return Value::nil();
    std::string path = std::get<std::string>(args[0]->data);
    std::string base = path;
    if (base.size() >= 4 && base.compare(base.size() - 4, 4, ".kn") == 0)
        base = base.substr(0, base.size() - 4);

    std::string cacheKey = normalizeKey(base);

    // Imported file modules execute inside the VM's global namespace. Reusing cached file modules
    // across different VM instances breaks because functions resolve globals dynamically.
    if (getImportResolver().cacheOwner != v) {
        getImportResolver().moduleCache.clear();
        getImportResolver().loading.clear();
        getImportResolver().cacheOwner = v;
    }

#ifdef KERN_BUILD_GAME
    if (path == "game" || path == "game.kn" || base == "game") {
        ValuePtr mod = createGameModule(*v);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (path == "g2d" || path == "g2d.kn" || path == "2dgraphics" || path == "2dgraphics.kn" || base == "g2d" || base == "2dgraphics") {
        ValuePtr mod = create2dGraphicsModule(*v);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (path == "g3d" || path == "g3d.kn" || base == "g3d") {
        ValuePtr mod = create3dGraphicsModule(*v);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
#endif
    if (base == "g2d" || base == "2dgraphics") {
        g_errorReporter.reportCompileError(ErrorCategory::ReferenceError, 0, 0,
            "Module 'g2d' (2D graphics) is not available in this binary.",
            importRaylibModuleHint(), "IMP-UNAVAILABLE-G2D", importModuleUnavailableDetail("g2d"));
        return importFailure("BROWSERKIT-UNSUPPORTED-PLATFORM: g2d module unavailable in this binary",
            VMErrorCode::IMPORT_UNSUPPORTED_MODULE);
    }
    if (base == "g3d") {
        g_errorReporter.reportCompileError(ErrorCategory::ReferenceError, 0, 0,
            "Module 'g3d' (3D graphics) is not available in this binary.",
            importRaylibModuleHint(), "IMP-UNAVAILABLE-G3D", importModuleUnavailableDetail("g3d"));
        return importFailure("BROWSERKIT-UNSUPPORTED-PLATFORM: g3d module unavailable in this binary",
            VMErrorCode::IMPORT_UNSUPPORTED_MODULE);
    }
    if (base == "game") {
        g_errorReporter.reportCompileError(ErrorCategory::ReferenceError, 0, 0,
            "Module 'game' is not available in this binary.",
            importRaylibModuleHint(), "IMP-UNAVAILABLE-GAME", importModuleUnavailableDetail("game"));
        return importFailure("BROWSERKIT-UNSUPPORTED-PLATFORM: game module unavailable in this binary",
            VMErrorCode::IMPORT_UNSUPPORTED_MODULE);
    }
    if (base == "process" || base == "kern::process") {
        ValuePtr mod = createProcessModule(*v, getImportResolver().runtimeServices);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (base == "input" || base == "kern::input") {
        ValuePtr mod = createInputModule(*v, getImportResolver().runtimeServices);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (base == "vision" || base == "kern::vision") {
        ValuePtr mod = createVisionModule(*v, getImportResolver().runtimeServices);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (base == "render" || base == "kern::render") {
        ValuePtr mod = createRenderModule(*v, getImportResolver().runtimeServices);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }
    if (isStdlibModuleName(base)) {
        ValuePtr mod = createStdlibModule(*v, base);
        getImportResolver().moduleCache[cacheKey] = mod;
        return mod ? *mod : Value::nil();
    }

    // File/embedded modules can be cached across imports. Built-in/stdlib modules above must be
    // created per-VM so their function values always point at the current VM's builtin table.
    auto cacheIt = getImportResolver().moduleCache.find(cacheKey);
    if (cacheIt != getImportResolver().moduleCache.end()) {
        return cacheIt->second ? *cacheIt->second : Value::nil();
    }

    if (path.find('/') == std::string::npos && path.find('\\') == std::string::npos &&
        path.find('.') == std::string::npos && path.find(':') == std::string::npos) {
        std::string pkgErr;
        std::string pkgMain = resolveInstalledPackageMain(path, &pkgErr);
        if (!pkgMain.empty()) {
            path = pkgMain;
        } else {
            path += ".kn";
        }
    } else {
        const std::string kargoKey = kargoPackagePathKey(path);
        if (!kargoKey.empty()) {
            std::string pkgErr;
            const std::string pkgMain = resolveInstalledPackageMain(kargoKey, &pkgErr);
            if (!pkgMain.empty()) {
                path = pkgMain;
            } else if (path.find('.') == std::string::npos) {
                path += ".kn";
            }
        } else if (path.find('.') == std::string::npos) {
            path += ".kn";
        }
    }

    // embedded module path resolution (used by standalone bundled executables).
    if (kernGetEnv("KERNC_TRACE_IMPORTS")) {
        std::cerr << "[kern-embed] import request=" << path << " provider=" << (getImportResolver().embeddedProvider ? "set" : "null") << std::endl;
    }
    if (getImportResolver().embeddedProvider) {
        std::string embeddedSource;
        std::string logicalPath;
        if (getImportResolver().embeddedProvider(path, embeddedSource, &logicalPath)) {
            std::string resolvedKey = normalizeKey(logicalPath.empty() ? path : logicalPath);
            auto resolvedIt = getImportResolver().moduleCache.find(resolvedKey);
            if (resolvedIt != getImportResolver().moduleCache.end()) {
                getImportResolver().moduleCache[cacheKey] = resolvedIt->second;
                return resolvedIt->second ? *resolvedIt->second : Value::nil();
            }
            if (getImportResolver().loading.find(resolvedKey) != getImportResolver().loading.end()) {
                g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0,
                    "Cyclic import detected: " + resolvedKey,
                    "Refactor modules to remove the dependency cycle.", "IMP-CYCLE",
                    importWhileLoadingDetail(resolvedKey));
                return importFailure("IMP-CYCLE: cyclic import detected: " + resolvedKey, VMErrorCode::IMPORT_CYCLE);
            }
            getImportResolver().loading.insert(resolvedKey);
            std::string displayPath = logicalPath.empty() ? resolvedKey : logicalPath;
            ValuePtr fileModule;
            if (compileAndRunImportedSource(v, displayPath, embeddedSource, cacheKey, resolvedKey, fileModule))
                return fileModule ? *fileModule : Value::nil();
            return importFailure("IMP-INTERNAL: embedded module import failed: " + displayPath, VMErrorCode::IMPORT_INTERNAL);
        }
    }

    std::string resolveErr;
    std::string resolved = resolveImportPath(path, &resolveErr);
    if (resolved.empty()) {
        g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
            "Could not resolve module import: " + path,
            importResolveFailureHint(), "IMP-RESOLVE",
            importResolveFailureDetail() + "\nReason: " + resolveErr);
        VMErrorCode code = VMErrorCode::IMPORT_NOT_FOUND;
        if (resolveErr == "empty module path" ||
            resolveErr == "path traversal is not allowed" ||
            resolveErr == "absolute import path is outside allowed roots") {
            code = VMErrorCode::IMPORT_INVALID_PATH;
        }
        return importFailure("IMP-RESOLVE: could not resolve module import: " + path, code);
    }
    std::string resolvedKey = normalizeKey(resolved);
    auto resolvedIt = getImportResolver().moduleCache.find(resolvedKey);
    if (resolvedIt != getImportResolver().moduleCache.end()) {
        getImportResolver().moduleCache[cacheKey] = resolvedIt->second;
        return resolvedIt->second ? *resolvedIt->second : Value::nil();
    }
    if (getImportResolver().loading.find(resolvedKey) != getImportResolver().loading.end()) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0,
            "Cyclic import detected: " + resolved,
            "Refactor modules to remove the dependency cycle.", "IMP-CYCLE",
            importWhileLoadingDetail(resolved));
        return importFailure("IMP-CYCLE: cyclic import detected: " + resolved, VMErrorCode::IMPORT_CYCLE);
    }

    getImportResolver().loading.insert(resolvedKey);
    std::ifstream f(resolved, std::ios::in | std::ios::binary);
    if (!f) {
        g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
            "Could not read module file: " + resolved,
            importResolveFailureHint(), "IMP-READ", fileOpenErrorDetail());
        getImportResolver().loading.erase(resolvedKey);
        return importFailure("IMP-READ: could not read module file: " + resolved, VMErrorCode::IMPORT_READ_FAIL);
    }
    std::stringstream buf;
    buf << f.rdbuf();
    std::string source = buf.str();
    ValuePtr fileModule;
    if (compileAndRunImportedSource(v, resolved, source, cacheKey, resolvedKey, fileModule))
        return fileModule ? *fileModule : Value::nil();
    return importFailure("IMP-INTERNAL: module import failed: " + resolved, VMErrorCode::IMPORT_INTERNAL);
}

} // namespace

void registerImportBuiltin(VM& vm) {
    // Each VM owns its own global namespace. Clear the import cache when a new VM installs
    // the import builtin so cached modules never leak between VM instances.
    getImportResolver().moduleCache.clear();
    getImportResolver().loading.clear();
    getImportResolver().cacheOwner = &vm;

    auto importFn = std::make_shared<FunctionObject>();
    importFn->isBuiltin = true;
    importFn->builtinIndex = IMPORT_BUILTIN_INDEX;
    vm.setGlobal("__import", std::make_shared<Value>(Value::fromFunction(importFn)));
    vm.registerBuiltin(IMPORT_BUILTIN_INDEX, [](VM* v, std::vector<ValuePtr> args) { return runImportBuiltin(v, args); });
}

void setEmbeddedModuleProvider(EmbeddedModuleProvider provider) {
    getImportResolver().embeddedProvider = std::move(provider);
    if (kernGetEnv("KERNC_TRACE_IMPORTS")) {
        std::cerr << "[kern-embed] provider installed" << std::endl;
    }
}

void clearEmbeddedModuleProvider() {
    if (kernGetEnv("KERNC_TRACE_IMPORTS")) {
        std::cerr << "[kern-embed] provider cleared" << std::endl;
    }
    getImportResolver().embeddedProvider = nullptr;
}

void replClearImportLoadingAtReplLineStart() {
    getImportResolver().loading.clear();
}

} // namespace kern
