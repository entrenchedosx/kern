/* *
 * kern (Kern) - Main entry point
 * compiles and runs .kn files or starts the REPL.
 * modes: kern file.kn | kern --ast file | kern --bytecode file | kern --check file | kern --watch [--check] file | kern watch test [opts] [dir] | kern --scan [paths] | kern --fmt file | kern graph <entry.kn>
 */

#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include "compiler/project_resolver.hpp"
#include "compiler/semantic.hpp"
#include "compiler/ast.hpp"
#include "vm/vm.hpp"
#include "vm/permissions.hpp"
#include "vm/value.hpp"
#include "vm/builtins.hpp"
#include "vm/bytecode.hpp"
#include "errors.hpp"
#include "import_resolution.hpp"
#include "scanner/scan_driver.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#endif
#include <exception>
#include <iostream>
#include <unordered_set>
#include <variant>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <chrono>
#include <regex>
#ifdef _WIN32
#include <windows.h>
#include "platform/win32_associate_kn.hpp"
#endif
#include "platform/env_compat.hpp"

using namespace kern;

static void parseAllowCsv(const std::string& csv, RuntimeGuardPolicy& g) {
    size_t i = 0;
    while (i < csv.size()) {
        while (i < csv.size() && (csv[i] == ' ' || csv[i] == '\t')) ++i;
        size_t j = i;
        while (j < csv.size() && csv[j] != ',') ++j;
        std::string tok = csv.substr(i, j - i);
        while (!tok.empty() && (tok.back() == ' ' || tok.back() == '\t')) tok.pop_back();
        if (!tok.empty()) {
            for (const auto& p : resolvePermissionToken(tok))
                g.grantedPermissions.insert(p);
        }
        i = j + 1;
    }
}

static void dumpStmt(const Stmt* s, int indent) {
    if (!s) return;
    std::string pre(indent, ' ');
    if (auto* fnDecl = dynamic_cast<const FunctionDeclStmt*>(s)) {
        std::cout << pre << "FunctionDecl " << fnDecl->name << "(" << fnDecl->params.size() << " params) L" << fnDecl->line << "\n";
        if (fnDecl->body) dumpStmt(fnDecl->body.get(), indent + 2);
    } else if (auto* classDecl = dynamic_cast<const ClassDeclStmt*>(s)) {
        std::cout << pre << "ClassDecl " << classDecl->name << " L" << classDecl->line << "\n";
        for (const auto& m : classDecl->methods) dumpStmt(m.get(), indent + 2);
    } else if (auto* varDecl = dynamic_cast<const VarDeclStmt*>(s)) {
        std::cout << pre << "VarDecl " << varDecl->name << " L" << varDecl->line << "\n";
    } else if (auto* blockStmt = dynamic_cast<const BlockStmt*>(s)) {
        std::cout << pre << "Block " << blockStmt->statements.size() << " stmts\n";
        for (const auto& c : blockStmt->statements) dumpStmt(c.get(), indent + 2);
    } else if (dynamic_cast<const IfStmt*>(s)) std::cout << pre << "If L" << s->line << "\n";
    else if (dynamic_cast<const ForRangeStmt*>(s)) std::cout << pre << "ForRange L" << s->line << "\n";
    else if (dynamic_cast<const ForInStmt*>(s)) std::cout << pre << "ForIn L" << s->line << "\n";
    else if (dynamic_cast<const WhileStmt*>(s)) std::cout << pre << "While L" << s->line << "\n";
    else if (dynamic_cast<const ReturnStmt*>(s)) std::cout << pre << "Return L" << s->line << "\n";
    else if (dynamic_cast<const TryStmt*>(s)) std::cout << pre << "Try L" << s->line << "\n";
    else if (dynamic_cast<const MatchStmt*>(s)) std::cout << pre << "Match L" << s->line << "\n";
    else std::cout << pre << "Stmt L" << s->line << "\n";
}

static void dumpAst(Program* program) {
    if (!program) return;
    std::cout << "Program\n";
    for (const auto& s : program->statements)
        dumpStmt(s.get(), 2);
}

static void dumpBytecode(const Bytecode& code, const std::vector<std::string>& constants) {
    for (size_t i = 0; i < code.size(); ++i) {
        const auto& inst = code[i];
        std::cout << (i + 1) << "\t" << opcodeName(inst.op) << formatBytecodeOperandSuffix(inst, constants)
                  << "\t; L" << inst.line << " C" << inst.column << "\n";
    }
}

static void dumpBytecodeNormalized(const Bytecode& code, const std::vector<std::string>& constants) {
    for (size_t i = 0; i < code.size(); ++i) {
        const auto& inst = code[i];
        std::cout << (i + 1) << "\t" << opcodeName(inst.op) << formatBytecodeOperandSuffix(inst, constants) << "\n";
    }
}

static std::string readTextFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void stripUtf8Bom(std::string& s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF)
        s.erase(0, 3);
}

// Strip leading #! line when executing a script file (e.g. ./hello.kn with shebang).
static void stripShebangLineForFile(std::string& s, const std::string& filenameForRun) {
    if (filenameForRun.empty()) return;
    if (s.size() >= 2 && s[0] == '#' && s[1] == '!') {
        size_t n = s.find('\n');
        if (n == std::string::npos)
            s.clear();
        else
            s.erase(0, n + 1);
    }
}

static void normalizeKernSourceText(std::string& s, const std::string& filenameForRun) {
    stripUtf8Bom(s);
    stripShebangLineForFile(s, filenameForRun);
}

static std::string pathExtensionLower(const std::filesystem::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

struct ResolvedKnPath {
    std::string path;
    bool warnedNonKn = false;
};

// Resolves script path: exact file, or base name -> base.kn when no extension was given.
static bool resolveKnScriptPath(const std::string& given, ResolvedKnPath& out, std::string& errMsg) {
    namespace fs = std::filesystem;
    fs::path p(given);
    std::error_code ec;
    auto isReg = [&](const fs::path& cand) -> bool {
        std::error_code e2;
        if (!fs::exists(cand, e2) || e2) return false;
        return fs::is_regular_file(cand, e2) && !e2;
    };

    fs::path chosen;
    if (isReg(p)) {
        chosen = p;
    } else if (!p.has_extension()) {
        fs::path withKn = fs::path(given + ".kn");
        if (isReg(withKn)) chosen = std::move(withKn);
    }

    if (chosen.empty()) {
        errMsg = "File not found: " + given;
        if (!p.has_extension()) errMsg += " (also tried " + (given + ".kn") + ")";
        if (given.size() >= 2 && given[0] == '-' && given[1] == '-') {
            errMsg += "\nHint: This looks like a CLI flag, not a script path. The `kern` you ran may be an older "
                      "binary (no `--scan`). Run the toolchain built from this repo, e.g. "
                      ".\\build\\Release\\kern.exe --scan (Windows), or rebuild and put that `kern` on PATH.";
        }
        return false;
    }

    fs::path canon = fs::weakly_canonical(chosen, ec);
    out.path = ec ? chosen.string() : canon.string();
    std::string ext = pathExtensionLower(chosen);
    if (!ext.empty() && ext != ".kn") out.warnedNonKn = true;
    return true;
}

static bool runSource(VM& vm, const std::string& sourceIn, const std::string& filename = "") {
    std::string source = sourceIn;
    normalizeKernSourceText(source, filename);
    g_errorReporter.setSource(source);
    g_errorReporter.setFilename(filename);
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        CodeGenerator gen;
        Bytecode code = gen.generate(std::move(program));
        const std::vector<std::string>& constants = gen.getConstants();
        std::unordered_set<std::string> declaredGlobals;
        insertAllBuiltinNamesForAnalysis(declaredGlobals);
#ifdef KERN_BUILD_GAME
        for (const std::string& n : getGameBuiltinNames()) declaredGlobals.insert(n);
#endif
        declaredGlobals.insert("__import");
        for (const auto& inst : code) {
            if (inst.op != Opcode::STORE_GLOBAL) continue;
            if (inst.operand.index() != 4) continue;
            size_t idx = std::get<size_t>(inst.operand);
            if (idx < constants.size()) declaredGlobals.insert(constants[idx]);
        }
        for (const auto& inst : code) {
            if (inst.op != Opcode::LOAD_GLOBAL) continue;
            if (inst.operand.index() != 4) continue;
            size_t idx = std::get<size_t>(inst.operand);
            if (idx >= constants.size()) continue;
            const std::string& name = constants[idx];
            if (declaredGlobals.find(name) == declaredGlobals.end())
                g_errorReporter.reportWarning(inst.line, 0,
                    "Possible undefined variable '" + name + "'. Did you mean to define it first?",
                    undefinedGlobalLoadWarningHint(), "ANAL-LOAD-GLOBAL", undefinedGlobalLoadWarningDetail());
        }
        vm.setBytecode(code);
        vm.setStringConstants(gen.getConstants());
        vm.setValueConstants(gen.getValueConstants());
        vm.setActiveSourcePath(filename);
        vm.run();
        return true;
    } catch (const LexerError& e) {
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, e.what(), lexerCompileErrorHint(),
            "LEX-TOKENIZE", lexerCompileErrorDetail());
        return false;
    } catch (const ParserError& e) {
        std::string msg(e.what());
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, msg, parserCompileErrorHint(msg),
            "PARSE-SYNTAX", parserCompileErrorDetail(msg));
        return false;
    } catch (const CodegenError& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, e.line, 0,
            std::string("Code generation failed: ") + e.what(),
            "The compiler hit an unsupported AST/operator case while lowering to bytecode.",
            "CODEGEN-UNSUPPORTED",
            internalFailureDetail("code generation"));
        return false;
    } catch (const VMError& e) {
        std::vector<StackFrame> stack;
        for (const auto& f : vm.getCallStackSlice()) {
            stack.push_back({f.functionName, f.filePath, f.line, f.column});
        }
        std::string hint(vmRuntimeErrorHint(e.category, e.code));
        g_errorReporter.reportRuntimeError(vmErrorCategory(e.category), e.line, e.column, e.what(), stack, hint,
            vmErrorCodeString(e.category, e.code), vmRuntimeErrorDetail(e.category, e.code),
            e.lineEnd, e.columnEnd);
        return false;
    } catch (const std::exception& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("Kern stopped: ") + e.what(),
            "This may be an unexpected error from the runtime or compiler; try reducing the program to a minimal case.",
            "KERN-STOP-EXC", internalFailureDetail("script execution (std::exception)"));
        return false;
    } catch (...) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "Kern stopped: unknown exception",
            "This may be an internal error; try simplifying the code or reporting a minimal reproducer.",
            "KERN-STOP-UNKNOWN",
            internalFailureDetail("script execution (non-typed throw)") + "\n"
            "No exception message was available; enable sanitizers or a debugger if this persists.");
        return false;
    }
}

static void setEnvVarIfEmpty(const char* key, const std::string& value) {
    const char* existing = kernGetEnv(key);
    if (existing && existing[0]) return;
    if (value.empty()) return;
#ifdef _WIN32
    _putenv_s(key, value.c_str()); // ensures getenv() sees the new value in-process
#else
    setenv(key, value.c_str(), 1);
#endif
}

// Auto-detect project root so examples that import `lib/kern/...` work even when the user runs
// `kern <absolute/scriptPath.kn>` from a different working directory.
static void maybeAutoSetKernLibFromEntryPath(const std::string& entryPath) {
    namespace fs = std::filesystem;
    fs::path entry(entryPath);
    if (entry.empty()) return;

    // If entryPath is a directory use it, otherwise use its parent directory.
    if (!fs::is_directory(entry)) entry = entry.parent_path();
    if (entry.empty()) return;

    fs::path cur = entry;
    for (int i = 0; i < 12 && !cur.empty(); ++i) {
        // Repo root (the directory containing `lib/kern/`)
        if (fs::exists(cur / "lib" / "kern")) {
            setEnvVarIfEmpty("KERN_LIB", cur.string());
            return;
        }

        // shareable-ide layout: `<root>/shareable-ide/compiler/lib/kern`
        fs::path shareableCompilerRoot = cur / "shareable-ide" / "compiler";
        if (fs::exists(shareableCompilerRoot / "lib" / "kern")) {
            setEnvVarIfEmpty("KERN_LIB", shareableCompilerRoot.string());
            return;
        }

        fs::path parent = cur.parent_path();
        if (parent == cur) break;
        cur = std::move(parent);
    }
}

static void printUsage(const char* prog) {
    std::cout << "Kern " <<
#ifdef KERN_VERSION
        KERN_VERSION
#else
        "1.0.0"
#endif
        << "\n\nUsage:\n"
        << "  " << prog << " [options] [script.kn]\n"
        << "  " << prog << " [options] run <script.kn>   Same as above (explicit run).\n"
        << "  " << prog << "                    Start REPL (no script).\n\n"
        << "Scripts:\n"
        << "  Paths ending in .kn run as Kern source. If the path has no extension, " << prog << " tries <path>.kn.\n"
        << "  A leading #! (e.g. #!/usr/bin/env kern) is ignored when running a file — use chmod +x then ./script.kn on Unix.\n\n"
        << "Options:\n"
        << "  --version, -v          Show version and exit.\n"
        << "  --help, -h            Show this help and exit.\n"
#ifdef _WIN32
        << "  --repair-association  (Windows) Re-apply per-user .kn association, icon, and shell verbs; exit.\n"
#endif
        << "  --check <file>        Compile only; exit 0 if OK.\n"
        << "  --check [--json] [--strict-types] [--skip-lock-verify] <file>\n"
        << "                        Optional flags before or after the path. --json: stdout JSON only (no stderr spam).\n"
        << "                        --strict-types: run semantic strict typing pass (preview).\n"
        << "                        If cwd has kern.json, lockfile must match kern.lock unless --skip-lock-verify.\n"
        << "  --lint <file>         Same as --check (lint/syntax check).\n"
        << "  --fmt <file>          Format script (indent by braces; ignores braces in //, /* */, and strings).\n"
        << "  --ast <file>          Dump AST and exit.\n"
        << "  --bytecode <file>     Dump bytecode and exit.\n"
        << "  --bytecode-normalized <file>  Dump bytecode without source locations (stable for golden tests).\n"
        << "  --vm-error-codes-json Print machine-readable VM error catalog (E### ids) and exit.\n"
        << "  --scan [opts] [paths] Cross-layer scan: builtin registry, stdlib exports, compile + static analysis.\n"
        << "                        Same as standalone `kern-scan`. Use --registry-only, --json, --strict-types, --test.\n\n"
        << "  --watch [--check] [--strict-types] <file>\n"
        << "                        Re-run on save; --check = compile/semantic only (no VM run).\n\n"
        << "Runtime mode flags (can precede any command):\n"
        << "  --trace               Enable VM instruction trace for the next script run or REPL (very noisy).\n"
        << "  --debug / --release   Toggle runtime guard profile (debug default).\n"
        << "  --allow-unsafe        Allow raw memory ops globally (without unsafe block).\n"
        << "  --unsafe              Alias for --allow-unsafe (global unlock for permission-gated builtins).\n"
        << "  --allow=a,b           Pre-grant permission ids (e.g. filesystem.read,network.http,system.exec).\n"
        << "                        Supports groups: fs.readonly, fs.readwrite, net.client, proc.control, env.manage.\n"
        << "  --profile=<name>      Apply capability profile before run (secure|dev|ci).\n"
        << "  --ffi                 Enable ffi_call builtin.\n"
        << "  --ffi-allow <dll>     Add DLL allowlist entry for sandboxed ffi_call.\n"
        << "  --no-sandbox          Disable FFI allowlist sandbox checks.\n\n"
        << "Commands:\n"
        << "  " << prog << " test [options] [path]\n"
        << "                        Run all .kn files under directory recursively (default: tests/coverage).\n"
        << "                        Options: --grep <substr>, --list, --fail-fast (-x), --skip-lock-verify.\n"
        << "                        See: " << prog << " test --help\n"
        << "  " << prog << " watch test [options] [path]\n"
        << "                        Same as test, but re-runs when any matching .kn file's mtime changes.\n"
        << "  " << prog << " docs             Print paths to documentation and optional MkDocs hint.\n"
        << "  " << prog << " build            Show CMake build instructions (toolchain is built with CMake).\n"
        << "  " << prog << " doctor           Print runtime/tooling diagnostics.\n"
        << "  " << prog << " init             Bootstrap kern.json and src/main.kn.\n"
        << "  " << prog << " add <dep>        Add dependency to kern.json.\n"
        << "  " << prog << " remove <dep>     Remove dependency from kern.json.\n"
        << "  " << prog << " install          Refresh lockfile from kern.json (not system install; see ./install.sh).\n"
        << "  " << prog << " verify           Exit 0 if kern.lock matches kern.json dependencies (CI-friendly).\n"
        << "  " << prog << " capability profile [list|show|apply]\n"
        << "                        Inspect/apply secure-default capability profiles.\n"
        << "  " << prog << " graph [opts] <entry.kn>\n"
        << "                        Static .kn import graph from entry (JSON with --json). See docs/FEATURE_PLAN_20.md.\n\n"
        << "Modules: import \"math\"; from \"math\" import sqrt; import(\"path\") (expression form) still works.\n"
        << "Docs: docs/GETTING_STARTED.md\n"
        << "Testing docs: docs/TESTING.md\n";
}

static bool writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << text;
    return static_cast<bool>(f);
}

static std::vector<std::string> parseDependenciesFromManifest(const std::string& s) {
    std::vector<std::string> deps;
    const std::string key = "\"dependencies\"";
    size_t k = s.find(key);
    if (k == std::string::npos) return deps;
    size_t lb = s.find('[', k);
    size_t rb = (lb == std::string::npos) ? std::string::npos : s.find(']', lb);
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb) return deps;
    std::string body = s.substr(lb + 1, rb - lb - 1);
    size_t i = 0;
    while (i < body.size()) {
        while (i < body.size() && body[i] != '"') ++i;
        if (i >= body.size()) break;
        size_t j = i + 1;
        while (j < body.size() && body[j] != '"') ++j;
        if (j >= body.size()) break;
        std::string dep = body.substr(i + 1, j - i - 1);
        if (!dep.empty()) deps.push_back(dep);
        i = j + 1;
    }
    return deps;
}

static std::string buildManifestJson(const std::vector<std::string>& deps) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"name\": \"kern-project\",\n";
    out << "  \"version\": \"1.0.0\",\n";
    out << "  \"entry\": \"src/main.kn\",\n";
    out << "  \"dependencies\": [";
    for (size_t i = 0; i < deps.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << deps[i] << "\"";
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}

static bool upsertDependency(const std::string& manifestPath, const std::string& dep, bool removeDep) {
    std::vector<std::string> deps = parseDependenciesFromManifest(readTextFile(manifestPath));
    std::vector<std::string> next;
    bool found = false;
    for (const auto& d : deps) {
        if (d == dep) {
            found = true;
            if (!removeDep) next.push_back(d);
        } else {
            next.push_back(d);
        }
    }
    if (!removeDep && !found) next.push_back(dep);
    return writeTextFile(manifestPath, buildManifestJson(next));
}

static bool refreshLockfile(const std::string& lockPath, const std::string& manifestPath) {
    std::vector<std::string> deps = parseDependenciesFromManifest(readTextFile(manifestPath));
    std::ostringstream out;
    out << "{\n  \"lockVersion\": 1,\n  \"dependencies\": [";
    for (size_t i = 0; i < deps.size(); ++i) {
        if (i) out << ", ";
        out << "{\"name\":\"" << deps[i] << "\",\"resolved\":\"local:" << deps[i] << "\"}";
    }
    out << "]\n}\n";
    return writeTextFile(lockPath, out.str());
}

static std::vector<std::string> parseLockDependencyNames(const std::string& lockText) {
    std::vector<std::string> out;
    std::regex re(R"re("name"\s*:\s*"([^"]+)")re");
    for (std::sregex_iterator it(lockText.begin(), lockText.end(), re), end; it != end; ++it)
        out.push_back((*it)[1].str());
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// requireKernJson: true for `kern verify` (missing kern.json is an error). false for test/check (no project → skip).
// printSuccessOnOk: only used when requireKernJson (verify command success line).
static int verifyLockfileAgainstManifest(bool requireKernJson, const char* label, bool printSuccessOnOk) {
    namespace fs = std::filesystem;
    if (!fs::exists("kern.json")) {
        if (requireKernJson) {
            std::cerr << label << ": kern.json not found in current directory\n";
            return 1;
        }
        return 0;
    }
    if (!fs::exists("kern.lock")) {
        std::cerr << label << ": kern.lock not found (run `kern install` to generate it)\n";
        return 1;
    }
    std::string man = readTextFile("kern.json");
    std::string lock = readTextFile("kern.lock");
    std::vector<std::string> ma = parseDependenciesFromManifest(man);
    std::sort(ma.begin(), ma.end());
    ma.erase(std::unique(ma.begin(), ma.end()), ma.end());
    std::vector<std::string> lo = parseLockDependencyNames(lock);
    if (ma != lo) {
        std::cerr << label << ": kern.json dependency set does not match kern.lock\n";
        std::cerr << "  kern.json: ";
        for (size_t i = 0; i < ma.size(); ++i) std::cerr << (i ? ", " : "") << ma[i];
        if (ma.empty()) std::cerr << "(empty)";
        std::cerr << "\n  kern.lock: ";
        for (size_t i = 0; i < lo.size(); ++i) std::cerr << (i ? ", " : "") << lo[i];
        if (lo.empty()) std::cerr << "(empty)";
        std::cerr << "\n";
        return 1;
    }
    if (printSuccessOnOk)
        std::cout << "verify: kern.lock matches kern.json\n";
    return 0;
}

static int cmdVerify() {
    return verifyLockfileAgainstManifest(true, "verify", true);
}

static int cmdInit() {
    namespace fs = std::filesystem;
    fs::path cwd = fs::current_path();
    fs::path manifest = cwd / "kern.json";
    fs::path srcDir = cwd / "src";
    fs::path entry = srcDir / "main.kn";
    fs::path lock = cwd / "kern.lock";
    if (!fs::exists(srcDir)) fs::create_directories(srcDir);
    if (!fs::exists(manifest)) {
        if (!writeTextFile(manifest.string(), buildManifestJson({}))) {
            std::cerr << "init: failed to write kern.json\n";
            return 1;
        }
    }
    if (!fs::exists(entry)) {
        if (!writeTextFile(entry.string(), "print(\"Hello from Kern\")\n")) {
            std::cerr << "init: failed to write src/main.kn\n";
            return 1;
        }
    }
    if (!refreshLockfile(lock.string(), manifest.string())) {
        std::cerr << "init: failed to write kern.lock\n";
        return 1;
    }
    std::cout << "Initialized Kern project in " << cwd.string() << "\n";
    return 0;
}

static int cmdDoctor(const char* prog) {
    namespace fs = std::filesystem;
    std::cout << "Kern Doctor\n";
    std::cout << "  executable: " << prog << "\n";
    std::cout << "  cwd: " << fs::current_path().string() << "\n";
    std::cout << "  manifest: " << (fs::exists("kern.json") ? "present" : "missing") << "\n";
    std::cout << "  lockfile: " << (fs::exists("kern.lock") ? "present" : "missing") << "\n";
    std::cout << "  tests/coverage: " << (fs::exists("tests/coverage") ? "present" : "missing") << "\n";
    {
        const fs::path handbook = fs::current_path() / "docs" / "GETTING_STARTED.md";
        std::cout << "  docs: ";
        if (fs::exists(handbook))
            std::cout << handbook.string() << " (found)\n";
        else
            std::cout << "docs/GETTING_STARTED.md not found from cwd (open repo docs/ or set cwd to repo root)\n";
    }
    {
        const fs::path roadmap = fs::current_path() / "docs" / "LANGUAGE_ROADMAP.md";
        std::cout << "  language roadmap: ";
        if (fs::exists(roadmap))
            std::cout << roadmap.string() << " (found)\n";
        else
            std::cout << "docs/LANGUAGE_ROADMAP.md not found from cwd\n";
    }
    {
        const fs::path mem = fs::current_path() / "docs" / "MEMORY_MODEL.md";
        std::cout << "  memory model: ";
        if (fs::exists(mem))
            std::cout << mem.string() << " (found)\n";
        else
            std::cout << "docs/MEMORY_MODEL.md not found from cwd\n";
    }
    {
        const fs::path ec = fs::current_path() / "docs" / "ERROR_CODES.md";
        std::cout << "  error codes: ";
        if (fs::exists(ec))
            std::cout << ec.string() << " (found)\n";
        else
            std::cout << "docs/ERROR_CODES.md not found from cwd\n";
    }
    {
        const fs::path impl = fs::current_path() / "docs" / "IMPLEMENTATION_SUMMARY.md";
        std::cout << "  implementation summary: ";
        if (fs::exists(impl))
            std::cout << impl.string() << " (found)\n";
        else
            std::cout << "docs/IMPLEMENTATION_SUMMARY.md not found from cwd\n";
    }
#ifdef KERN_BUILD_GAME
    std::cout << "  build: KERN_BUILD_GAME enabled (g2d/g3d/game paths compiled into this binary when linked)\n";
#else
    std::cout << "  build: KERN_BUILD_GAME disabled (no Raylib game bundle in this binary)\n";
#endif
    std::cout << "  env: NO_COLOR=1 disables ANSI colors on stderr diagnostics.\n";
    std::cout << "  env: KERN_VM_TRACE=1 enables verbose VM instruction tracing (very noisy).\n";
    std::cout << "  env: KERNC_TRACE_IMPORTS logs embedded import resolution to stderr.\n";
    std::cout << "  tip: kern test [dir] runs .kn files recursively (default tests/coverage).\n";
    std::cout << "  tip: kern verify checks kern.lock matches kern.json dependencies.\n";
    std::cout << "  tip: kern graph <entry.kn> prints static import graph (--json for tools).\n";
    std::cout << "  tip: kern capability profile list shows grouped permission presets.\n";
    return 0;
}

static int cmdDocs(const char* /*prog*/) {
    std::cout << "Kern documentation\n\n"
              << "Handbook: docs/GETTING_STARTED.md\n"
              << "Testing: docs/TESTING.md\n"
              << "Troubleshooting: docs/TROUBLESHOOTING.md\n"
              << "Language sketch: docs/LANGUAGE_SYNTAX.md\n"
              << "Stdlib (std.v1): docs/STDLIB_STD_V1.md\n"
              << "System access vNext: docs/VNEXT_SYSTEM_ACCESS.md\n"
              << "Adoption roadmap: docs/ADOPTION_ROADMAP.md\n"
              << "Feature backlog: docs/FEATURE_PLAN_20.md\n"
              << "Contributing: CONTRIBUTING.md\n"
              << "Code of Conduct: CODE_OF_CONDUCT.md\n\n"
              << "Optional browsable site (requires Python):\n"
              << "  pip install mkdocs\n"
              << "  mkdocs serve    # from repo root; opens http://127.0.0.1:8000\n\n"
              << "Online: https://github.com/entrenchedosx/kern/tree/main/docs\n";
    return 0;
}

static int cmdBuildHint() {
    std::cout << "The Kern toolchain is built with CMake (compiler + VM + CLI).\n\n"
              << "Typical release build (no Raylib / headless):\n"
              << "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DKERN_BUILD_GAME=OFF -DKERN_BUILD_DOC_FRAMEWORK_DEMO=OFF\n"
              << "  cmake --build build --parallel --target kern kernc kern-scan\n\n"
              << "See docs/GETTING_STARTED.md for OS-specific steps (vcpkg, Raylib, portable packaging).\n";
    return 0;
}

static int cmdCapabilityProfile(int argc, char** argv, int argBase, RuntimeGuardPolicy& guards, const char* prog) {
    // Usage: kern capability profile [list|show <name>|apply <name>]
    if (argc <= argBase + 1 || std::string(argv[argBase + 1]) != "profile") {
        std::cerr << "capability: expected `profile` subcommand\n";
        std::cerr << "usage: " << prog << " capability profile [list|show <name>|apply <name>]\n";
        return 1;
    }
    if (argc <= argBase + 2 || std::string(argv[argBase + 2]) == "list") {
        std::cout << "capability profiles:\n";
        for (const auto& kv : capabilityProfiles()) {
            std::cout << "  " << kv.first << "\n";
        }
        return 0;
    }
    const std::string action = argv[argBase + 2];
    if ((action == "show" || action == "apply") && argc > argBase + 3) {
        const std::string name = argv[argBase + 3];
        auto it = capabilityProfiles().find(name);
        if (it == capabilityProfiles().end()) {
            std::cerr << "capability profile not found: " << name << "\n";
            return 1;
        }
        std::vector<std::string> expanded;
        for (const auto& tok : it->second) {
            std::vector<std::string> r = resolvePermissionToken(tok);
            expanded.insert(expanded.end(), r.begin(), r.end());
        }
        std::sort(expanded.begin(), expanded.end());
        expanded.erase(std::unique(expanded.begin(), expanded.end()), expanded.end());
        std::cout << "profile: " << name << "\n";
        std::cout << "  groups: ";
        for (size_t i = 0; i < it->second.size(); ++i) std::cout << (i ? ", " : "") << it->second[i];
        if (it->second.empty()) std::cout << "(none)";
        std::cout << "\n  permissions: ";
        for (size_t i = 0; i < expanded.size(); ++i) std::cout << (i ? ", " : "") << expanded[i];
        if (expanded.empty()) std::cout << "(none)";
        std::cout << "\n";
        if (action == "apply") {
            (void)applyCapabilityProfile(name, guards);
            std::cout << "applied to current runtime guard set\n";
        }
        return 0;
    }
    std::cerr << "usage: " << prog << " capability profile [list|show <name>|apply <name>]\n";
    return 1;
}

struct TestCliOptions {
    std::string grep;
    bool failFast = false;
    bool listOnly = false;
    bool skipLockVerify = false;
};

static std::string graphJsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\\':
            o += "\\\\";
            break;
        case '"':
            o += "\\\"";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                o += buf;
            } else
                o += static_cast<char>(c);
            break;
        }
    }
    return o;
}

/* * Walk upward from entry file dir; if .../lib/kern exists, return that directory's parent (repo root).*/
static std::string detectRepoRootWithLibKern(const std::filesystem::path& entryFile) {
    namespace fs = std::filesystem;
    fs::path dir = entryFile.has_parent_path() ? entryFile.parent_path() : fs::current_path();
    for (int i = 0; i < 16 && !dir.empty(); ++i) {
        if (fs::exists(dir / "lib" / "kern")) return dir.string();
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = std::move(parent);
    }
    return {};
}

static void printGraphUsage(const char* prog) {
    std::cout << "Usage:\n  " << prog << " graph [--json] [--include-path <dir>]... <entry.kn>\n\n"
              << "Statically resolve import(\"...\") / import \"...\" from entry and print the .kn module graph.\n"
              << "  --json              Machine-readable JSON on stdout.\n"
              << "  --include-path      Extra search root (repeatable); also auto-detected when repo contains lib/kern/.\n";
}

// Returns 0 = ok, 1 = error, 2 = help.
static int cmdGraph(int argc, char** argv, int argBase, const char* prog) {
    namespace fs = std::filesystem;
    bool jsonOut = false;
    std::vector<std::string> extraIncludes;
    int i = argBase + 1;
    for (; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            printGraphUsage(prog);
            return 2;
        }
        if (a == "--json") {
            jsonOut = true;
            continue;
        }
        if (a == "--include-path" && i + 1 < argc) {
            extraIncludes.push_back(argv[++i]);
            continue;
        }
        if (!a.empty() && a[0] == '-') {
            std::cerr << "graph: unknown option: " << a << "\n";
            printGraphUsage(prog);
            return 1;
        }
        break;
    }
    if (i >= argc) {
        std::cerr << "graph: missing entry .kn path\n";
        printGraphUsage(prog);
        return 1;
    }

    ResolvedKnPath rs;
    std::string resolveErr;
    if (!resolveKnScriptPath(argv[i], rs, resolveErr)) {
        if (jsonOut)
            std::cout << "{\"ok\":false,\"errors\":[\"" << graphJsonEscape(resolveErr) << "\"]}\n";
        else
            std::cerr << "graph: " << resolveErr << "\n";
        return 1;
    }
    if (rs.warnedNonKn && !jsonOut)
        std::cerr << "warning: not a .kn file; continuing: " << rs.path << "\n";

    fs::path entry(rs.path);
    std::error_code ec;
    entry = fs::weakly_canonical(entry, ec);
    if (ec) entry = fs::path(rs.path);

    ResolveOptions ro;
    ro.projectRoot = entry.has_parent_path() ? entry.parent_path().string() : fs::current_path().string();
    ro.includePaths.push_back(ro.projectRoot);
    std::string repoRoot = detectRepoRootWithLibKern(entry);
    if (!repoRoot.empty()) ro.includePaths.push_back(repoRoot);
    for (const auto& inc : extraIncludes) {
        fs::path p(inc);
        std::error_code ec2;
        fs::path can = fs::weakly_canonical(p, ec2);
        ro.includePaths.push_back(ec2 ? inc : can.string());
    }

    ResolveResult rr = resolveProjectGraph(entry.string(), ro);

    if (!rr.errors.empty()) {
        if (jsonOut) {
            std::cout << "{\"ok\":false,\"entry\":\"" << graphJsonEscape(entry.string()) << "\",\"errors\":[";
            for (size_t e = 0; e < rr.errors.size(); ++e) {
                if (e) std::cout << ',';
                std::cout << "\"" << graphJsonEscape(rr.errors[e]) << "\"";
            }
            std::cout << "]}\n";
        } else {
            std::cerr << "graph: resolve failed:\n";
            for (const auto& err : rr.errors) std::cerr << "  " << err << "\n";
        }
        return 1;
    }

    if (jsonOut) {
        std::cout << "{\"ok\":true,\"entry\":\"" << graphJsonEscape(entry.string()) << "\",\"module_count\":"
                  << rr.modules.size() << ",\"order\":[";
        for (size_t o = 0; o < rr.topologicalOrder.size(); ++o) {
            if (o) std::cout << ',';
            std::cout << "\"" << graphJsonEscape(rr.topologicalOrder[o]) << "\"";
        }
        std::cout << "],\"modules\":[";
        bool firstMod = true;
        std::vector<std::string> keys;
        keys.reserve(rr.modules.size());
        for (const auto& kv : rr.modules) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const std::string& k : keys) {
            const ResolvedModule& m = rr.modules.at(k);
            if (!firstMod) std::cout << ',';
            firstMod = false;
            std::cout << "{\"path\":\"" << graphJsonEscape(m.canonicalPath) << "\",\"dependencies\":[";
            for (size_t d = 0; d < m.dependencies.size(); ++d) {
                if (d) std::cout << ',';
                std::cout << "\"" << graphJsonEscape(m.dependencies[d]) << "\"";
            }
            std::cout << "]}";
        }
        std::cout << "]}\n";
        return 0;
    }

    std::cout << "entry: " << entry.string() << "\n";
    std::cout << "modules: " << rr.modules.size() << "\n";
    std::vector<std::string> sortedKeys;
    sortedKeys.reserve(rr.modules.size());
    for (const auto& kv : rr.modules) sortedKeys.push_back(kv.first);
    std::sort(sortedKeys.begin(), sortedKeys.end());
    for (const std::string& k : sortedKeys) {
        const ResolvedModule& m = rr.modules.at(k);
        std::cout << "\n" << m.canonicalPath << "\n";
        for (const std::string& dep : m.dependencies) std::cout << "  -> " << dep << "\n";
    }
    return 0;
}

static void printTestUsage(const char* prog) {
    std::cout << "Usage:\n  " << prog << " test [options] [dir]\n\n"
              << "Run all .kn files under dir recursively (default: tests/coverage).\n\n"
              << "Options:\n"
              << "  --grep <substr>   Only run tests whose relative path contains the substring.\n"
              << "  --fail-fast, -x   Stop on first unexpected failure.\n"
              << "  --list            List tests that would run (after --grep) and exit.\n"
              << "  --skip-lock-verify Skip kern.json/kern.lock consistency check (not for CI).\n"
              << "  -h, --help        Show this help.\n\n"
              << "Also: " << prog << " watch test [same options] — poll mtimes and re-run this suite.\n";
}

// Returns: 0 = ok, 1 = parse error, 2 = help requested.
static int parseTestCli(int argc, char** argv, int argBase, TestCliOptions& out, std::string& dirPath) {
    dirPath.clear();
    out = TestCliOptions{};
    std::vector<std::string> positionals;
    for (int i = argBase + 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--grep" && i + 1 < argc) {
            out.grep = argv[++i];
            continue;
        }
        if (a == "--fail-fast" || a == "-x") {
            out.failFast = true;
            continue;
        }
        if (a == "--list") {
            out.listOnly = true;
            continue;
        }
        if (a == "--skip-lock-verify") {
            out.skipLockVerify = true;
            continue;
        }
        if (a == "--help" || a == "-h") {
            return 2;
        }
        if (!a.empty() && a[0] == '-') {
            std::cerr << "test: unknown flag: " << a << "\n";
            return 1;
        }
        positionals.push_back(std::move(a));
    }
    if (positionals.size() > 1) {
        std::cerr << "test: only one directory argument allowed\n";
        return 1;
    }
    if (!positionals.empty())
        dirPath = positionals[0];
    return 0;
}

// Brace-based indent for --fmt: only `{` / `}` outside //, /* */, "" / '', and """ / ''' strings affect depth.
static void appendSignificantBracesFromLine(const std::string& line, bool& inBlockComment, bool& inTripleString,
                                            char& tripleQuote, std::string& sig) {
    size_t i = 0;
    while (i < line.size()) {
        if (inTripleString) {
            while (i + 2 < line.size()) {
                if (line[i] == tripleQuote && line[i + 1] == tripleQuote && line[i + 2] == tripleQuote) {
                    i += 3;
                    inTripleString = false;
                    break;
                }
                ++i;
            }
            if (inTripleString)
                return;
            continue;
        }
        if (inBlockComment) {
            const size_t end = line.find("*/", i);
            if (end == std::string::npos)
                return;
            inBlockComment = false;
            i = end + 2;
            continue;
        }
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')
            return;
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '*') {
            inBlockComment = true;
            i += 2;
            continue;
        }
        if ((line[i] == '"' || line[i] == '\'') && i + 2 < line.size() && line[i + 1] == line[i] && line[i + 2] == line[i]) {
            tripleQuote = line[i];
            inTripleString = true;
            i += 3;
            continue;
        }
        if (line[i] == '"' || line[i] == '\'') {
            const char q = line[i++];
            while (i < line.size()) {
                if (line[i] == '\\') {
                    i += 2;
                    continue;
                }
                if (line[i] == q) {
                    ++i;
                    break;
                }
                ++i;
            }
            continue;
        }
        if (line[i] == '{')
            sig += '{';
        else if (line[i] == '}')
            sig += '}';
        ++i;
    }
}

static std::string formatSourceBraceIndentAware(const std::string& source) {
    bool inBlockComment = false;
    bool inTripleString = false;
    char tripleQuote = '"';
    int indent = 0;
    const int indentStep = 2;
    std::string out;
    std::istringstream is(source);
    std::string line;
    while (std::getline(is, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::string sig;
        appendSignificantBracesFromLine(line, inBlockComment, inTripleString, tripleQuote, sig);
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            ++start;
        const std::string stripped = line.substr(start);
        for (char c : sig) {
            if (c == '}' && indent >= indentStep)
                indent -= indentStep;
        }
        if (!stripped.empty()) {
            out += std::string(static_cast<size_t>(indent), ' ');
            out += stripped;
        }
        for (char c : sig) {
            if (c == '{')
                indent += indentStep;
            else if (c == '}' && indent >= indentStep)
                indent -= indentStep;
        }
        out += '\n';
    }
    return out;
}

// 0 = ok (outFiles non-empty), 1 = I/O or path error, 2 = no .kn files (message printed).
static int prepareKnTestFiles(const TestCliOptions& opts, const std::string& dirPath, std::filesystem::path& outRoot,
                              std::vector<std::filesystem::path>& outFiles) {
    namespace fs = std::filesystem;
    outRoot = fs::path(dirPath.empty() ? "tests/coverage" : dirPath);
    if (!fs::exists(outRoot)) {
        std::cerr << "test: path not found: " << outRoot.string() << "\n";
        return 1;
    }
    if (!fs::is_directory(outRoot)) {
        std::cerr << "test: not a directory: " << outRoot.string() << "\n";
        return 1;
    }
    maybeAutoSetKernLibFromEntryPath(outRoot.string());
    outFiles.clear();
    try {
        for (const auto& entry :
             fs::recursive_directory_iterator(outRoot, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".kn") continue;
            outFiles.push_back(entry.path());
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "test: " << e.what() << "\n";
        return 1;
    }
    std::sort(outFiles.begin(), outFiles.end());
    outFiles.erase(std::remove_if(outFiles.begin(), outFiles.end(), [&](const fs::path& path) {
            const std::string rel = path.lexically_relative(outRoot).generic_string();
            return rel == "strict_types_phase2/fail_mismatch.kn" || rel == "fail_mismatch.kn";
        }),
        outFiles.end());
    if (!opts.grep.empty()) {
        std::vector<fs::path> filtered;
        filtered.reserve(outFiles.size());
        for (const auto& path : outFiles) {
            const std::string rel = path.lexically_relative(outRoot).generic_string();
            if (rel.find(opts.grep) != std::string::npos) filtered.push_back(path);
        }
        outFiles = std::move(filtered);
    }
    if (outFiles.empty()) {
        std::cerr << "test: no .kn files"
                 << (opts.grep.empty() ? " found under " : " match --grep under ") << outRoot.string() << "\n";
        return 2;
    }
    return 0;
}

static std::filesystem::file_time_type maxMtimeOfKnFiles(const std::vector<std::filesystem::path>& files) {
    namespace fs = std::filesystem;
    fs::file_time_type best{};
    bool any = false;
    for (const auto& path : files) {
        std::error_code ec;
        const auto t = fs::last_write_time(path, ec);
        if (ec) continue;
        if (!any || t > best) {
            best = t;
            any = true;
        }
    }
    return best;
}

static int runPreparedKnTests(const std::filesystem::path& testRoot, const std::vector<std::filesystem::path>& files,
                              const TestCliOptions& opts) {
    int pass = 0;
    int fail = 0;
    for (const auto& path : files) {
        std::string rel = path.lexically_relative(testRoot).generic_string();
        const bool expectFail =
            (rel.size() >= 9 && rel.find("_xfail.kn") != std::string::npos) ||
            (rel.rfind("test_diag_", 0) == 0) ||
            (rel == "test_module_loading.kn") ||
            (rel == "test_browserkit_import_fail_loud.kn") ||
            (rel == "test_phase2_ffi_typed.kn");
        std::string source = readTextFile(path.string());
        if (source.empty()) {
            ++fail;
            std::cout << "[FAIL] " << rel << " (empty)\n";
            if (opts.failFast) {
                std::cout << "\nStopped (--fail-fast): pass=" << pass << " fail=" << fail << "\n";
                return 1;
            }
            continue;
        }
        VM vm;
        RuntimeGuardPolicy testGuards;
        registerAllStandardPermissions(testGuards);
        vm.setRuntimeGuards(testGuards);
        registerAllBuiltins(vm);
        registerImportBuiltin(vm);
        g_errorReporter.resetCounts();
        bool ok = runSource(vm, source, path.string());
        if (expectFail ? !ok : ok) {
            ++pass;
            std::cout << "[PASS] " << rel << "\n";
        } else {
            ++fail;
            std::cout << "[FAIL] " << rel << "\n";
            if (opts.failFast) {
                std::cout << "\nStopped (--fail-fast): pass=" << pass << " fail=" << fail << "\n";
                return 1;
            }
        }
    }
    std::cout << "\nSummary: pass=" << pass << " fail=" << fail << "\n";
    return fail == 0 ? 0 : 1;
}

static int cmdTest(int argc, char** argv, int argBase, const char* prog) {
    TestCliOptions opts;
    std::string dirPath;
    const int pr = parseTestCli(argc, argv, argBase, opts, dirPath);
    if (pr == 2) {
        printTestUsage(prog);
        return 0;
    }
    if (pr != 0)
        return 1;

    namespace fs = std::filesystem;
    fs::path p;
    std::vector<fs::path> files;
    const int prep = prepareKnTestFiles(opts, dirPath, p, files);
    if (prep == 1 || prep == 2)
        return 1;
    if (opts.listOnly) {
        for (const auto& path : files) {
            std::cout << path.lexically_relative(p).generic_string() << "\n";
        }
        return 0;
    }
    if (!opts.skipLockVerify) {
        if (int lr = verifyLockfileAgainstManifest(false, "test", false); lr != 0)
            return lr;
    }
    return runPreparedKnTests(p, files, opts);
}

static int cmdWatchTest(int argc, char** argv, int testArgBase, const char* prog) {
    TestCliOptions opts;
    std::string dirPath;
    const int pr = parseTestCli(argc, argv, testArgBase, opts, dirPath);
    if (pr == 2) {
        printTestUsage(prog);
        return 0;
    }
    if (pr != 0)
        return 1;

    namespace fs = std::filesystem;
    fs::path p;
    std::vector<fs::path> files;
    const int prep = prepareKnTestFiles(opts, dirPath, p, files);
    if (prep == 1 || prep == 2)
        return 1;
    if (opts.listOnly) {
        for (const auto& path : files) {
            std::cout << path.lexically_relative(p).generic_string() << "\n";
        }
        return 0;
    }
    if (!opts.skipLockVerify) {
        if (int lr = verifyLockfileAgainstManifest(false, "watch test", false); lr != 0)
            return lr;
    }

    std::cout << "Watching .kn tests under " << p.string() << " (Ctrl+C to stop)\n";
    fs::file_time_type lastStamp = maxMtimeOfKnFiles(files);
    while (true) {
        std::cout << "\n--- " << prog << " test (watch) ---\n";
        const int tr = runPreparedKnTests(p, files, opts);
        (void)tr;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            std::vector<fs::path> fresh;
            fs::path root;
            const int again = prepareKnTestFiles(opts, dirPath, root, fresh);
            if (again != 0) {
                std::cerr << "watch test: could not refresh file list\n";
                return 1;
            }
            files = std::move(fresh);
            p = std::move(root);
            const fs::file_time_type now = maxMtimeOfKnFiles(files);
            if (now != lastStamp) {
                lastStamp = now;
                break;
            }
        }
    }
}

static bool checkKernSource(const std::string& source, const std::string& pathResolved, bool strictTypes) {
    g_errorReporter.resetCounts();
    g_errorReporter.setSource(source);
    g_errorReporter.setFilename(pathResolved);
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        CodeGenerator gen;
        (void)gen.generate(std::move(program));
        (void)applySemanticDiagnosticsToReporter(source, pathResolved, strictTypes);
        return g_errorReporter.errorCount() == 0;
    } catch (const LexerError& e) {
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, e.what(), lexerCompileErrorHint(),
            "LEX-TOKENIZE", lexerCompileErrorDetail());
        return false;
    } catch (const ParserError& e) {
        std::string msg(e.what());
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, msg, parserCompileErrorHint(msg),
            "PARSE-SYNTAX", parserCompileErrorDetail(msg));
        return false;
    } catch (const std::exception& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("compile failed: ") + e.what(),
            "This may be an internal compiler error; try simplifying the surrounding code.", "INTERNAL-COMPILE",
            internalFailureDetail("`kern --check` (lex/parse/codegen)"));
        return false;
    } catch (...) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "compile failed: unknown exception",
            "This may be an internal compiler error; try simplifying the surrounding code.", "INTERNAL-COMPILE-UNKNOWN",
            internalFailureDetail("`kern --check` (non-typed throw)"));
        return false;
    }
}

static int cmdWatch(VM& vm, const std::string& scriptPath, bool checkOnly, bool strictTypes) {
    namespace fs = std::filesystem;
    fs::path p(scriptPath);
    if (!fs::exists(p)) {
        std::cerr << "watch: file not found: " << scriptPath << "\n";
        return 1;
    }

    // Ensure runtime module imports work even when `kern --watch` is started from elsewhere.
    maybeAutoSetKernLibFromEntryPath(p.string());

    auto last = fs::last_write_time(p);
    std::cout << "Watching " << p.string();
    if (checkOnly) {
        std::cout << " (compile-only";
        if (strictTypes) std::cout << ", --strict-types";
        std::cout << ")";
    }
    std::cout << " (Ctrl+C to stop)\n";
    while (true) {
        std::string source = readTextFile(p.string());
        normalizeKernSourceText(source, p.string());
        bool ok = checkOnly ? checkKernSource(source, p.string(), strictTypes) : runSource(vm, source, p.string());
        g_errorReporter.printSummary();
        std::cout << "[watch] " << (checkOnly ? "check" : "run") << " -> " << (ok ? "ok" : "failed") << "\n";
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
            auto now = fs::last_write_time(p);
            if (now != last) {
                last = now;
                break;
            }
        }
    }
}

int main(int argc, char** argv) {
#ifdef _WIN32
    if (argc >= 2 && std::strcmp(argv[1], "--repair-association") == 0) {
        kern::win32::repairKnFileAssociation();
        return 0;
    }
    // One-time per-user: .kn → kern.exe, optional kern_logo.ico, Explorer refresh. No-op after
    // %APPDATA%\\kern\\setup_done.flag exists, or if KERN_SKIP_FILE_ASSOCIATION is set.
    kern::win32::maybeRegisterKnFileAssociation();
#endif
    const char* prog = argc >= 1 ? argv[0] : "kern";
    int argBase = 1;
    bool vmTraceCli = false;
    RuntimeGuardPolicy runtimeGuards;
    runtimeGuards.debugMode = true;
    runtimeGuards.allowUnsafe = false;
    runtimeGuards.enforcePointerBounds = true;
    runtimeGuards.ffiEnabled = false;
    runtimeGuards.sandboxEnabled = true;
    {
        const char* kEnf = kernGetEnv("KERN_ENFORCE_PERMISSIONS");
        if (kEnf && (std::strcmp(kEnf, "0") == 0 || std::strcmp(kEnf, "false") == 0))
            runtimeGuards.enforcePermissions = false;
    }
    while (argBase < argc) {
        std::string flag = argv[argBase];
        if (flag == "--debug") {
            runtimeGuards.debugMode = true;
            ++argBase;
            continue;
        }
        if (flag == "--release") {
            runtimeGuards.debugMode = false;
            ++argBase;
            continue;
        }
        if (flag == "--allow-unsafe" || flag == "--unsafe") {
            runtimeGuards.allowUnsafe = true;
            ++argBase;
            continue;
        }
        if (flag.rfind("--allow=", 0) == 0) {
            parseAllowCsv(flag.substr(8), runtimeGuards);
            ++argBase;
            continue;
        }
        if (flag.rfind("--profile=", 0) == 0) {
            std::string prof = flag.substr(10);
            if (!applyCapabilityProfile(prof, runtimeGuards)) {
                std::cerr << "error: unknown capability profile: " << prof << "\n";
                return 1;
            }
            ++argBase;
            continue;
        }
        if (flag == "--deny-unsafe") {
            runtimeGuards.allowUnsafe = false;
            ++argBase;
            continue;
        }
        if (flag == "--ffi") {
            runtimeGuards.ffiEnabled = true;
            ++argBase;
            continue;
        }
        if (flag == "--no-ffi") {
            runtimeGuards.ffiEnabled = false;
            ++argBase;
            continue;
        }
        if (flag == "--no-sandbox") {
            runtimeGuards.sandboxEnabled = false;
            ++argBase;
            continue;
        }
        if (flag == "--sandbox") {
            runtimeGuards.sandboxEnabled = true;
            ++argBase;
            continue;
        }
        if (flag == "--ffi-allow" && argBase + 1 < argc) {
            runtimeGuards.ffiLibraryAllowlist.push_back(argv[argBase + 1]);
            argBase += 2;
            continue;
        }
        if (flag == "--trace") {
            vmTraceCli = true;
            ++argBase;
            continue;
        }
        break;
    }

    // Doc-compat compiler passthrough:
    // Some documentation and tools refer to `kern --config/--target/...` for compiler flows.
    // The standalone compiler binary is `kernc.exe` (launcher for kern-impl). If we see a compiler
    // flag as the first non-runtime-guard argument, forward the full argv to kernc and exit.
    if (argc > argBase) {
        const std::string first = argv[argBase];
        const bool isCompilerEntry =
            first == "--config" || first == "--analyze" || first == "--fix-all" || first == "--undo-fixes" ||
            first == "--pkg-init" || first == "--pkg-lock" || first == "--pkg-validate" || first == "--target";
        if (isCompilerEntry) {
            namespace fs = std::filesystem;
            fs::path exePath;
#ifdef _WIN32
            char buf[MAX_PATH];
            DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            if (n > 0 && n < MAX_PATH) exePath = fs::path(std::string(buf, n));
            else exePath = fs::path(prog);
#else
            exePath = fs::path(prog);
#endif
            fs::path dir = exePath.has_parent_path() ? exePath.parent_path() : fs::current_path();
#ifdef _WIN32
            fs::path kernc = dir / "kernc.exe";
#else
            fs::path kernc = dir / "kernc";
#endif
            std::string kerncPath = kernc.string();
            // If kernc is not next to kern, fall back to PATH lookup.
            if (!fs::exists(kernc)) kerncPath = "kernc";

            std::ostringstream cmd;
            cmd << "\"" << kerncPath << "\"";
            for (int i = argBase; i < argc; ++i) {
                cmd << " \"" << argv[i] << "\"";
            }
#ifdef _WIN32
            // Avoid cmd.exe quoting/lookup issues: spawn directly.
            std::string cmdLine = cmd.str();
            STARTUPINFOA si{};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi{};
            // CreateProcess requires a mutable buffer for the command line.
            std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
            mutableCmd.push_back('\0');
            BOOL ok = CreateProcessA(
                nullptr,
                mutableCmd.data(),
                nullptr, nullptr,
                TRUE,
                0,
                nullptr,
                nullptr,
                &si,
                &pi
            );
            if (!ok) {
                std::cerr << "error: failed to launch kernc for compiler mode passthrough\n";
                return 1;
            }
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 1;
            (void)GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return static_cast<int>(exitCode);
#else
            return std::system(cmd.str().c_str());
#endif
        }
    }
    if (argc > argBase) {
        std::string arg = argv[argBase];
        if (arg == "--version" || arg == "-v") {
            std::string ver = "1.0.0";
#ifdef KERN_VERSION
            ver = KERN_VERSION;
#else
            std::ifstream vf("KERN_VERSION.txt");
            if (vf) {
                std::string line;
                if (std::getline(vf, line)) {
                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || std::isspace(static_cast<unsigned char>(line.back())))) line.pop_back();
                    if (!line.empty()) ver = line;
                }
            }
#endif
            std::cout << "Kern " << ver << "\n";
            std::cout << "bytecode-schema: " << kBytecodeSchemaVersion << "\n";
#ifdef KERN_BUILD_ID
            std::cout << "build: " << KERN_BUILD_ID << "\n";
#endif
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            printUsage(prog);
            return 0;
        }
        if (arg == "--vm-error-codes-json") {
            std::cout << formatVmErrorCatalogJson();
            return 0;
        }
        if (arg == "graph") {
            const int gr = cmdGraph(argc, argv, argBase, prog);
            if (gr == 2) return 0;
            return gr;
        }
        if (arg == "watch") {
            if (argc <= argBase + 1) {
                std::cerr << "watch: use `kern watch test [opts] [dir]` or `kern --watch <file.kn>`\n";
                return 1;
            }
            if (std::string(argv[argBase + 1]) == "test")
                return cmdWatchTest(argc, argv, argBase + 1, prog);
            std::cerr << "watch: unknown subcommand (expected `test`)\n";
            return 1;
        }
        if (arg == "test") {
            return cmdTest(argc, argv, argBase, prog);
        }
        if (arg == "doctor") {
            return cmdDoctor(prog);
        }
        if (arg == "docs") {
            return cmdDocs(prog);
        }
        if (arg == "build") {
            return cmdBuildHint();
        }
        if (arg == "init") {
            return cmdInit();
        }
        if (arg == "add" && argc > argBase + 1) {
            if (!upsertDependency("kern.json", argv[argBase + 1], false)) {
                std::cerr << "add: failed to update kern.json\n";
                return 1;
            }
            (void)refreshLockfile("kern.lock", "kern.json");
            std::cout << "Added dependency: " << argv[argBase + 1] << "\n";
            return 0;
        }
        if (arg == "remove" && argc > argBase + 1) {
            if (!upsertDependency("kern.json", argv[argBase + 1], true)) {
                std::cerr << "remove: failed to update kern.json\n";
                return 1;
            }
            (void)refreshLockfile("kern.lock", "kern.json");
            std::cout << "Removed dependency: " << argv[argBase + 1] << "\n";
            return 0;
        }
        if (arg == "install") {
            if (!refreshLockfile("kern.lock", "kern.json")) {
                std::cerr << "install: failed to refresh kern.lock\n";
                return 1;
            }
            std::cout << "Dependencies locked from kern.json\n";
            return 0;
        }
        if (arg == "verify") {
            return cmdVerify();
        }
        if (arg == "capability") {
            return cmdCapabilityProfile(argc, argv, argBase, runtimeGuards, prog);
        }
    }

    if (argc > argBase && std::string(argv[argBase]) == "--scan") {
        VM scanVm;
        registerAllBuiltins(scanVm);
        registerImportBuiltin(scanVm);
        scanVm.setRuntimeGuards(runtimeGuards);
        if (runtimeGuards.debugMode) {
            scanVm.setStepLimit(5'000'000);
            scanVm.setMaxCallDepth(2048);
            scanVm.setCallbackStepGuard(250'000);
        } else {
            scanVm.setStepLimit(0);
            scanVm.setMaxCallDepth(8192);
            scanVm.setCallbackStepGuard(0);
        }
        std::vector<std::string> scanArgs;
        for (int i = 0; i < argc; ++i) scanArgs.push_back(argv[i]);
        scanVm.setCliArgs(std::move(scanArgs));
        return runScanFromArgv(argc, argv, argBase + 1, scanVm, prog);
    }

    VM vm;
    registerAllBuiltins(vm);
    registerImportBuiltin(vm);
    vm.setRuntimeGuards(runtimeGuards);
    // VM.hpp default is 1024; CLI scripts use higher caps so deep user stacks work in debug/release.
    // Stress tests that need a specific limit should call set_max_call_depth(...) in .kn (see tests/stress).
    if (runtimeGuards.debugMode) {
        vm.setStepLimit(5'000'000);
        vm.setMaxCallDepth(2048);
        vm.setCallbackStepGuard(250'000);
    } else {
        vm.setStepLimit(0);
        vm.setMaxCallDepth(8192);
        vm.setCallbackStepGuard(0);
    }
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
    vm.setCliArgs(std::move(args));
    if (vmTraceCli)
        vm.setVmTraceEnabled(true);

    if (argc > argBase + 1 && std::string(argv[argBase]) == "--ast") {
        ResolvedKnPath rs;
        std::string resolveErr;
        if (!resolveKnScriptPath(argv[argBase + 1], rs, resolveErr)) {
            std::cerr << "error: " << resolveErr << "\n";
            return 1;
        }
        if (rs.warnedNonKn)
            std::cerr << "warning: not a .kn file; continuing: " << rs.path << "\n";
        std::string path = rs.path;
        std::ifstream f(path);
        if (!f) {
            g_errorReporter.setFilename(path);
            g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
                "Could not open file: " + path, "Check that the file exists and is readable.", "FILE-OPEN",
                fileOpenErrorDetail());
            return 1;
        }
        std::stringstream buf;
        buf << f.rdbuf();
        std::string source = buf.str();
        normalizeKernSourceText(source, path);
        g_errorReporter.setSource(source);
        g_errorReporter.setFilename(path);
        try {
            Lexer lexer(source);
            std::vector<Token> tokens = lexer.tokenize();
            Parser parser(std::move(tokens));
            std::unique_ptr<Program> program = parser.parse();
            dumpAst(program.get());
            return 0;
        } catch (const LexerError& e) {
            g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, e.what(), lexerCompileErrorHint(),
                "LEX-TOKENIZE", lexerCompileErrorDetail());
            g_errorReporter.printSummary();
            return 1;
        } catch (const ParserError& e) {
            std::string msg(e.what());
            g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, msg, parserCompileErrorHint(msg),
                "PARSE-SYNTAX", parserCompileErrorDetail(msg));
            g_errorReporter.printSummary();
            return 1;
        } catch (const std::exception& e) {
            g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("AST dump failed: ") + e.what(),
                "This may be an internal compiler error; try simplifying the file.", "INTERNAL-AST",
                internalFailureDetail("`kern --ast` (parse → AST dump)"));
            g_errorReporter.printSummary();
            return 1;
        } catch (...) {
            g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "AST dump failed: unknown exception",
                "This may be an internal compiler error; try simplifying the file.", "INTERNAL-AST-UNKNOWN",
                internalFailureDetail("`kern --ast` (non-typed throw)"));
            g_errorReporter.printSummary();
            return 1;
        }
    }

    if (argc > argBase + 1 && (std::string(argv[argBase]) == "--bytecode" || std::string(argv[argBase]) == "--bytecode-normalized")) {
        const bool normalized = std::string(argv[argBase]) == "--bytecode-normalized";
        ResolvedKnPath rs;
        std::string resolveErr;
        if (!resolveKnScriptPath(argv[argBase + 1], rs, resolveErr)) {
            std::cerr << "error: " << resolveErr << "\n";
            return 1;
        }
        if (rs.warnedNonKn)
            std::cerr << "warning: not a .kn file; continuing: " << rs.path << "\n";
        std::string path = rs.path;
        std::ifstream f(path);
        if (!f) {
            g_errorReporter.setFilename(path);
            g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
                "Could not open file: " + path, "Check that the file exists and is readable.", "FILE-OPEN",
                fileOpenErrorDetail());
            return 1;
        }
        std::stringstream buf;
        buf << f.rdbuf();
        std::string source = buf.str();
        normalizeKernSourceText(source, path);
        g_errorReporter.setSource(source);
        g_errorReporter.setFilename(path);
        try {
            Lexer lexer(source);
            std::vector<Token> tokens = lexer.tokenize();
            Parser parser(std::move(tokens));
            std::unique_ptr<Program> program = parser.parse();
            CodeGenerator gen;
            Bytecode code = gen.generate(std::move(program));
            if (normalized)
                dumpBytecodeNormalized(code, gen.getConstants());
            else
                dumpBytecode(code, gen.getConstants());
            return 0;
        } catch (const LexerError& e) {
            g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, e.what(), lexerCompileErrorHint(),
                "LEX-TOKENIZE", lexerCompileErrorDetail());
            g_errorReporter.printSummary();
            return 1;
        } catch (const ParserError& e) {
            std::string msg(e.what());
            g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, msg, parserCompileErrorHint(msg),
                "PARSE-SYNTAX", parserCompileErrorDetail(msg));
            g_errorReporter.printSummary();
            return 1;
        } catch (const std::exception& e) {
            g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("Bytecode dump failed: ") + e.what(),
                "This may be an internal compiler error; try simplifying the file.", "INTERNAL-BYTECODE",
                internalFailureDetail("`kern --bytecode` (codegen)"));
            g_errorReporter.printSummary();
            return 1;
        } catch (...) {
            g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "Bytecode dump failed: unknown exception",
                "This may be an internal compiler error; try simplifying the file.", "INTERNAL-BYTECODE-UNKNOWN",
                internalFailureDetail("`kern --bytecode` (non-typed throw)"));
            g_errorReporter.printSummary();
            return 1;
        }
    }

    if (argc > argBase + 1 && std::string(argv[argBase]) == "--watch") {
        bool watchCheck = false;
        bool watchStrict = false;
        int wi = argBase + 1;
        for (; wi < argc; ++wi) {
            std::string a = argv[wi];
            if (a == "--check") {
                watchCheck = true;
                continue;
            }
            if (a == "--strict-types") {
                watchStrict = true;
                continue;
            }
            break;
        }
        if (wi >= argc) {
            std::cerr << "error: --watch: missing script path (e.g. " << prog << " --watch [--check] script.kn)\n";
            return 1;
        }
        ResolvedKnPath rs;
        std::string resolveErr;
        if (!resolveKnScriptPath(argv[wi], rs, resolveErr)) {
            std::cerr << "error: " << resolveErr << "\n";
            return 1;
        }
        if (rs.warnedNonKn)
            std::cerr << "warning: not a .kn file; continuing: " << rs.path << "\n";
        return cmdWatch(vm, rs.path, watchCheck, watchStrict);
    }

    // check / --lint: compile only, no run (for CI and IDE)
    if (argc > argBase + 1 && (std::string(argv[argBase]) == "--check" || std::string(argv[argBase]) == "--lint")) {
        bool json = false;
        bool strictTypes = false;
        bool skipLockVerify = false;
        std::string path;
        for (int i = argBase + 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--json") json = true;
            else if (a == "--strict-types") strictTypes = true;
            else if (a == "--skip-lock-verify") skipLockVerify = true;
            else if (!a.empty() && a[0] == '-') {
                std::cerr << "Unknown --check flag: " << a << "\n";
                return 1;
            } else if (path.empty()) path = std::move(a);
            else {
                std::cerr << "Unexpected extra argument for --check: " << a << "\n";
                return 1;
            }
        }
        if (path.empty()) {
            std::cerr << "--check: missing file path\n";
            return 1;
        }
        if (!skipLockVerify) {
            if (int lr = verifyLockfileAgainstManifest(false, "check", false); lr != 0)
                return lr;
        }

        ResolvedKnPath rs;
        std::string resolveErr;
        if (!resolveKnScriptPath(path, rs, resolveErr)) {
            std::cerr << "error: " << resolveErr << "\n";
            return 1;
        }
        if (rs.warnedNonKn)
            std::cerr << "warning: not a .kn file; continuing: " << rs.path << "\n";
        std::string pathResolved = rs.path;

        struct SuppressHumanDiagnosticsGuard {
            ErrorReporter& rep;
            bool prev;
            SuppressHumanDiagnosticsGuard(ErrorReporter& r, bool on) : rep(r), prev(r.suppressHumanItemPrint()) {
                if (on) rep.setSuppressHumanItemPrint(true);
            }
            ~SuppressHumanDiagnosticsGuard() { rep.setSuppressHumanItemPrint(prev); }
        } guard(g_errorReporter, json);

        std::ifstream f(pathResolved);
        if (!f) {
            g_errorReporter.setFilename(pathResolved);
            g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
                "Could not open file: " + pathResolved, "Check that the file exists and is readable.", "FILE-OPEN",
                fileOpenErrorDetail());
            if (json) std::cout << g_errorReporter.toJson() << std::endl;
            else g_errorReporter.printSummary();
            return 1;
        }
        std::stringstream buf;
        buf << f.rdbuf();
        std::string source = buf.str();
        normalizeKernSourceText(source, pathResolved);
        (void)checkKernSource(source, pathResolved, strictTypes);
        if (json) std::cout << g_errorReporter.toJson() << std::endl;
        else g_errorReporter.printSummary();
        return g_errorReporter.errorCount() > 0 ? 1 : 0;
    }

    // fmt: format source (indent by brace level)
    if (argc > argBase + 1 && std::string(argv[argBase]) == "--fmt") {
        ResolvedKnPath rs;
        std::string resolveErr;
        if (!resolveKnScriptPath(argv[argBase + 1], rs, resolveErr)) {
            std::cerr << "error: " << resolveErr << "\n";
            return 1;
        }
        if (rs.warnedNonKn)
            std::cerr << "warning: not a .kn file; continuing: " << rs.path << "\n";
        std::string path = rs.path;
        std::ifstream f(path);
        if (!f) {
            std::cerr << "fmt: could not open " << path << std::endl;
            return 1;
        }
        std::stringstream buf;
        buf << f.rdbuf();
        std::string source = buf.str();
        normalizeKernSourceText(source, path);
        std::string out = formatSourceBraceIndentAware(source);
        std::ofstream of(path);
        if (!of) { std::cerr << "fmt: could not write " << path << std::endl; return 1; }
        of << out;
        return 0;
    }

    int scriptArg = argBase;
    if (argc > argBase && std::string(argv[argBase]) == "run") {
        if (argc <= argBase + 1) {
            std::cerr << "error: kern run: missing script path (e.g. kern run hello.kn)\n";
            return 1;
        }
        scriptArg = argBase + 1;
    }

    if (argc > scriptArg) {
        ResolvedKnPath rs;
        std::string resolveErr;
        if (!resolveKnScriptPath(argv[scriptArg], rs, resolveErr)) {
            std::cerr << "error: " << resolveErr << "\n";
            return 1;
        }
        if (rs.warnedNonKn)
            std::cerr << "warning: not a .kn file; running anyway: " << rs.path << "\n";
        std::string path = rs.path;
        // Make imports resilient to arbitrary CWD.
        maybeAutoSetKernLibFromEntryPath(path);
        std::ifstream f(path);
        if (!f) {
            g_errorReporter.setFilename(path);
            g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
                "Could not open file: " + path, "Check that the file exists and is readable.", "FILE-OPEN",
                fileOpenErrorDetail());
            return 1;
        }
        std::stringstream buf;
        buf << f.rdbuf();
        bool ok = runSource(vm, buf.str(), path);
        g_errorReporter.printSummary();
        if (!ok) return 1;
        int scriptExit = vm.getScriptExitCode();
        if (scriptExit >= 0) return (scriptExit > 255 ? 255 : scriptExit);
        return 0;
    }

    // rEPL
    // If started from outside the repo, set KERN_LIB so interactive imports still work.
    maybeAutoSetKernLibFromEntryPath(std::filesystem::current_path().string());
    std::cout << "Kern. Type expressions or statements. help | clear | history | search <q> | trace on|off | last | exit"
              << std::endl;
    std::string line;
    std::vector<std::string> history;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\\') {
            std::string block = line.substr(0, line.size() - 1);
            while (true) {
                std::cout << ". ";
                std::string next;
                if (!std::getline(std::cin, next)) break;
                if (!next.empty() && next.back() == '\\') block += "\n" + next.substr(0, next.size() - 1);
                else { block += "\n" + next; break; }
            }
            line = std::move(block);
        }
        if (line == "exit" || line == "quit") break;
        if (line == "help" || line == ".help") {
            std::cout << "  help / .help     — show this\n  clear / .clear   — clear screen\n  history          — show command history\n  search <text>    — search history\n  trace on / off   — verbose VM instruction trace (noisy)\n  last / .last     — show last reported error (code + message)\n  exit / quit      — exit REPL\n  Example: let x = 5   print(x)   print(2+3)\n  Modules: import \"math\"   or   from \"math\" import sqrt\n";
            continue;
        }
        if (line == "history" || line == ".history") {
            for (size_t i = 0; i < history.size(); ++i) std::cout << (i + 1) << ": " << history[i] << "\n";
            continue;
        }
        if (line.rfind("search ", 0) == 0) {
            std::string needle = line.substr(7);
            for (const auto& h : history) if (h.find(needle) != std::string::npos) std::cout << h << "\n";
            continue;
        }
        if (line == "clear" || line == ".clear") {
#ifdef _WIN32
            (void)std::system("cls");
#else
            // glibc marks system() with warn_unused_result; (void) cast is not enough.
            int clearRc = std::system("clear");
            (void)clearRc;
#endif
            continue;
        }
        if (line == "trace on" || line == ".trace on") {
            vm.setVmTraceEnabled(true);
            std::cout << "VM trace: on (stderr; very noisy)\n";
            continue;
        }
        if (line == "trace off" || line == ".trace off") {
            vm.setVmTraceEnabled(false);
            std::cout << "VM trace: off\n";
            continue;
        }
        if (line == "last" || line == ".last") {
            const auto& items = g_errorReporter.getItems();
            if (items.empty()) {
                std::cout << "(no diagnostics recorded this session)\n";
                continue;
            }
            const auto& it = items.back();
            std::cout << it.errorCode << ": " << it.message << "\n";
            if (!it.stackTrace.empty()) {
                for (const auto& fr : it.stackTrace) {
                    std::cout << "  at " << fr.functionName;
                    if (!fr.filePath.empty()) {
                        std::cout << " " << humanizePathForDisplay(fr.filePath) << ":" << fr.line;
                        if (fr.column > 0) std::cout << ":" << fr.column;
                    } else {
                        std::cout << " (" << fr.line << ":" << fr.column << ")";
                    }
                    std::cout << "\n";
                }
            }
            continue;
        }
        history.push_back(line);
        g_errorReporter.resetCounts();
        runSource(vm, line, "<repl>");
        g_errorReporter.printSummary();
    }
    return 0;
}
