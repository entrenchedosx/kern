#include "scanner/scan_driver.hpp"
#include "scanner/builtin_registry_check.hpp"
#include "scanner/stdlib_export_check.hpp"

#include "compiler/codegen.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/semantic.hpp"
#include "errors.hpp"
#include "vm/builtins.hpp"
#include "vm/bytecode.hpp"
#include "platform/env_compat.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#endif

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace kern {
namespace {

static void setEnvVarIfEmpty(const char* key, const std::string& value) {
    const char* existing = kernGetEnv(key);
    if (existing && existing[0]) return;
    if (value.empty()) return;
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}

static void maybeAutoSetKernLibFromEntryPath(const std::string& entryPath) {
    namespace fs = std::filesystem;
    fs::path entry(entryPath);
    if (entry.empty()) return;
    if (!fs::is_directory(entry)) entry = entry.parent_path();
    if (entry.empty()) return;
    fs::path cur = entry;
    for (int i = 0; i < 12 && !cur.empty(); ++i) {
        if (fs::exists(cur / "lib" / "kern")) {
            setEnvVarIfEmpty("KERN_LIB", cur.string());
            return;
        }
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

static void stripUtf8Bom(std::string& s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF)
        s.erase(0, 3);
}

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

static int lineForOffset(const std::string& source, size_t off) {
    int line = 1;
    for (size_t i = 0; i < off && i < source.size(); ++i)
        if (source[i] == '\n') ++line;
    return line;
}

static void emitRiskyApiWarnings(const std::string& source) {
    struct Risk {
        const char* needle;
        const char* message;
        const char* hint;
        const char* code;
    };
    static const Risk kRisky[] = {
        {"ffi_call(", "Potentially unsafe FFI call detected.",
         "Prefer ffi_call_typed descriptor mode and ensure unsafe context + allowlist.", "SCAN-RISK-FFI"},
        {"ffi_call_typed(", "Typed FFI call detected.",
         "Ensure library allowlist and minimal argument surface for production.", "SCAN-RISK-FFI-TYPED"},
        {"exec(", "Shell execution detected.",
         "Prefer exec_args/run wrappers to avoid shell-injection risks.", "SCAN-RISK-EXEC"},
        {"spawn(", "Process spawn detected.",
         "Validate command source and require process control permission intentionally.", "SCAN-RISK-SPAWN"},
        {"tcp_connect(", "Raw TCP API detected.",
         "Confirm net.client/network.tcp permission intent and timeout handling.", "SCAN-RISK-TCP"},
    };
    for (const auto& r : kRisky) {
        size_t pos = source.find(r.needle);
        while (pos != std::string::npos) {
            g_errorReporter.reportWarning(lineForOffset(source, pos), 1, r.message, r.hint, r.code, "");
            pos = source.find(r.needle, pos + 1);
        }
    }
}

static std::string readTextFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void collectKnFiles(const std::filesystem::path& root, bool isDir, std::vector<std::filesystem::path>& out) {
    namespace fs = std::filesystem;
    if (!isDir) {
        if (root.extension() == ".kn") out.push_back(root);
        return;
    }
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".kn") out.push_back(entry.path());
        }
    } catch (const fs::filesystem_error&) {
        // Caller reports missing path
    }
}

static bool scanOneFile(const std::string& path, bool strictTypes) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(path)) {
        g_errorReporter.setFilename(path);
        g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0, "File not found: " + path,
            "Verify the path.", "FILE-OPEN", fileOpenErrorDetail());
        return false;
    }
    auto fsize = fs::file_size(path, ec);
    if (ec) {
        g_errorReporter.setFilename(path);
        g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0, "Cannot access file: " + path,
            "Check permissions.", "FILE-OPEN", fileOpenErrorDetail());
        return false;
    }
    std::string source = readTextFile(path);
    if (source.empty() && fsize > 0) {
        g_errorReporter.setFilename(path);
        g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0, "Could not read file: " + path,
            "Check permissions.", "FILE-OPEN", fileOpenErrorDetail());
        return false;
    }

    normalizeKernSourceText(source, path);
    g_errorReporter.setSource(source);
    g_errorReporter.setFilename(path);
    emitRiskyApiWarnings(source);

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

        (void)applySemanticDiagnosticsToReporter(source, path, strictTypes);
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
        g_errorReporter.reportCompileError(ErrorCategory::Other, e.line, 0, std::string("Code generation failed: ") + e.what(),
            "The compiler hit an unsupported construct while lowering to bytecode.", "CODEGEN-UNSUPPORTED",
            internalFailureDetail("kern scan (codegen)"));
        return false;
    } catch (const std::exception& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("scan compile failed: ") + e.what(),
            "The compiler hit an unexpected error while scanning this file.", "INTERNAL-SCAN",
            internalFailureDetail("kern scan (compile)"));
        return false;
    } catch (...) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "scan compile failed: unknown exception",
            "This may be an internal compiler error.", "INTERNAL-SCAN-UNKNOWN",
            internalFailureDetail("kern scan (compile)"));
        return false;
    }
}

static int spawnKernTest(const char* progName, const std::string& dir) {
    namespace fs = std::filesystem;
    fs::path exe(progName);
    fs::path dirOfExe = exe.has_parent_path() ? exe.parent_path() : fs::current_path();
#ifdef _WIN32
    fs::path kernExe = dirOfExe / "kern.exe";
#else
    fs::path kernExe = dirOfExe / "kern";
#endif
    if (!fs::exists(kernExe)) {
        std::cerr << "[scan] --test: could not find sibling " << kernExe.string()
                  << "; run tests manually: kern test \"" << dir << "\"\n";
        return 1;
    }
    std::ostringstream cmd;
#ifdef _WIN32
    cmd << "\"" << kernExe.string() << "\" test \"" << dir << "\"";
    return std::system(cmd.str().c_str());
#else
    cmd << "\"" << kernExe.string() << "\" test \"" << dir << "\"";
    return std::system(cmd.str().c_str());
#endif
}

} // namespace

int runScanFromArgv(int argc, char** argv, int argStart, VM& vm, const char* progName) {
    ScanCliOptions opt;
    std::vector<std::string> paths;
    for (int i = argStart; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") opt.json = true;
        else if (a == "--verbose") opt.verbose = true;
        else if (a == "--strict-types") opt.strictTypes = true;
        else if (a == "--registry-only") opt.registryOnly = true;
        else if (a == "--test") {
            opt.runTestsAfter = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') opt.testDir = argv[++i];
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "error: unknown --scan flag: " << a << "\n";
            return 2;
        } else
            paths.push_back(std::move(a));
    }
    if (paths.empty()) paths.push_back(".");

    struct Guard {
        ErrorReporter& rep;
        bool prev;
        Guard(ErrorReporter& r, bool json) : rep(r), prev(r.suppressHumanItemPrint()) {
            if (json) rep.setSuppressHumanItemPrint(true);
        }
        ~Guard() { rep.setSuppressHumanItemPrint(prev); }
    } guard(g_errorReporter, opt.json);

    g_errorReporter.resetCounts();

    emitBuiltinRegistryDiagnostics(vm, g_errorReporter);
    emitStdlibExportDiagnostics(g_errorReporter);

    if (!opt.registryOnly) {
        for (const auto& pstr : paths) {
            namespace fs = std::filesystem;
            fs::path p(pstr);
            std::error_code ec;
            if (!fs::exists(p, ec)) {
                g_errorReporter.setFilename(pstr);
                g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0, "Path not found: " + pstr,
                    "Check the path.", "FILE-OPEN", fileOpenErrorDetail());
                continue;
            }
            maybeAutoSetKernLibFromEntryPath(fs::absolute(p).string());
            bool isDir = fs::is_directory(p);
            std::vector<fs::path> files;
            collectKnFiles(p, isDir, files);
            if (!isDir && files.empty()) {
                g_errorReporter.reportWarning(0, 0, "Not a .kn file (skipped): " + pstr,
                    "Pass a directory or a .kn file.", "SCAN-SKIP", "");
                continue;
            }
            std::sort(files.begin(), files.end());
            if (opt.verbose && !opt.json) {
                std::cerr << "kern-scan: " << files.size() << " file(s) under " << pstr << "\n";
            }
            for (const auto& f : files) {
                if (opt.verbose && !opt.json) std::cerr << "  scanning " << f.string() << "\n";
                scanOneFile(f.string(), opt.strictTypes);
            }
        }
    }

    if (opt.json) {
        std::cout << g_errorReporter.toJson() << std::endl;
    } else {
        g_errorReporter.printSummary();
        if (g_errorReporter.errorCount() == 0 && g_errorReporter.warningCount() == 0)
            std::cerr << "kern-scan: no issues.\n";
    }

    int code = g_errorReporter.errorCount() > 0 ? 1 : 0;
    if (code == 0 && opt.runTestsAfter && !opt.registryOnly) {
        std::string td = opt.testDir.empty() ? std::string("tests/coverage") : opt.testDir;
        int t = spawnKernTest(progName, td);
        if (t != 0) return t;
    }
    return code;
}

} // namespace kern
