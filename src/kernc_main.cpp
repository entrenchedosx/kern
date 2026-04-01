#include "backend/cpp_backend.hpp"
#include "analyzer/project_analyzer.hpp"
#include "compiler/project_resolver.hpp"
#include "compiler/semantic.hpp"
#include "ir/ir_builder.hpp"
#include "ir/passes/passes.hpp"
#include "utils/build_cache.hpp"
#include "utils/kernconfig.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

#ifdef _MSC_VER
static std::string kerncGetEnvStr(const char* name) {
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, name) != 0 || !buf) return {};
    std::string out(buf);
    std::free(buf);
    return out;
}
#else
static std::string kerncGetEnvStr(const char* name) {
    if (const char* v = std::getenv(name)) return std::string(v);
    return {};
}
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;
using namespace kern;

struct BuildDiagRecord {
    std::string severity;
    std::string file;
    int line = 0;
    int column = 0;
    std::string code;
    std::string message;
};

static std::string jsonEscapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else
                out += static_cast<char>(c);
            break;
        }
    }
    return out;
}

static const char* semanticSeverityJson(SemanticSeverity s) {
    switch (s) {
    case SemanticSeverity::Error:
        return "error";
    case SemanticSeverity::Warning:
        return "warning";
    default:
        return "info";
    }
}

static bool writeBuildDiagnosticsJson(const std::string& path, const std::vector<BuildDiagRecord>& diags) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "[";
    for (size_t i = 0; i < diags.size(); ++i) {
        if (i) out << ",";
        const auto& d = diags[i];
        out << "\n{\"severity\":\"" << jsonEscapeJsonString(d.severity) << "\""
            << ",\"file\":\"" << jsonEscapeJsonString(d.file) << "\""
            << ",\"line\":" << d.line << ",\"column\":" << d.column << ",\"code\":\"" << jsonEscapeJsonString(d.code)
            << "\",\"message\":\"" << jsonEscapeJsonString(d.message) << "\"}";
    }
    out << "\n]\n";
    return static_cast<bool>(out);
}

/* * kern checkout root (contains src/compiler/lexer.cpp). Not the process cwd — standalone CMake must not depend on cwd.*/
static bool kerncIsSplRepoRoot(const fs::path& root) {
    std::error_code ec;
    return fs::exists(root / "src" / "compiler" / "lexer.cpp", ec);
}

static fs::path kerncDetectSourceRoot() {
    {
        std::string env = kerncGetEnvStr("KERN_REPO_ROOT");
        if (!env.empty()) {
            fs::path p(env);
            std::error_code ec;
            fs::path can = fs::weakly_canonical(p, ec);
            if (!ec && kerncIsSplRepoRoot(can)) return can;
        }
    }
    {
        std::string env = kerncGetEnvStr("KERN_SRC_ROOT");
        if (!env.empty()) {
            fs::path p(env);
            std::error_code ec;
            fs::path can = fs::weakly_canonical(p, ec);
            if (!ec && kerncIsSplRepoRoot(can)) return can;
        }
    }
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD got = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (got > 0 && got < MAX_PATH) {
        fs::path dir = fs::path(buf).parent_path();
        for (int i = 0; i < 16 && !dir.empty(); ++i) {
            if (kerncIsSplRepoRoot(dir)) {
                std::error_code ec;
                return fs::weakly_canonical(dir, ec);
            }
            fs::path parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    }
#elif defined(__linux__)
    char selfPath[4096];
    ssize_t n = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
    if (n > 0) {
        selfPath[static_cast<size_t>(n)] = '\0';
        fs::path dir = fs::path(selfPath).parent_path();
        for (int i = 0; i < 16 && !dir.empty(); ++i) {
            if (kerncIsSplRepoRoot(dir)) {
                std::error_code ec;
                return fs::weakly_canonical(dir, ec);
            }
            fs::path parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    }
#elif defined(__APPLE__)
    char selfPath[4096];
    uint32_t sz = sizeof(selfPath);
    if (_NSGetExecutablePath(selfPath, &sz) == 0) {
        fs::path dir = fs::path(selfPath).parent_path();
        for (int i = 0; i < 16 && !dir.empty(); ++i) {
            if (kerncIsSplRepoRoot(dir)) {
                std::error_code ec;
                return fs::weakly_canonical(dir, ec);
            }
            fs::path parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    }
#endif
    std::error_code ec;
    return fs::current_path(ec);
}

namespace {

struct CliOptions {
    std::string input;
    std::string output;
    std::string configPath;
    std::string target = "native";
    std::string kernelCrossPrefix;
    std::string kernelArch;
    std::string kernelBootloader;
    std::string kernelLinkerScript;
    std::string featureSet;
    std::vector<std::string> enabledFeatures;
    std::vector<std::string> disabledFeatures;
    bool debugRuntime = false;
    bool allowUnsafe = false;
    bool ffiEnabled = false;
    bool sandboxEnabled = true;
    std::vector<std::string> ffiAllowLibraries;
    std::string projectRoot = ".";
    std::string reportJsonPath;
    std::string undoBackupPath;
    bool release = true;
    bool console = true;
    int opt = 2;
    bool json = false;
    bool fixAll = false;
    bool analyzeOnly = false;
    bool dryRun = false;
    bool freestanding = false;
    bool kernelIso = false;
    bool runQemu = false;
    bool targetSet = false;
    bool pkgInit = false;
    bool pkgLock = false;
    bool pkgValidate = false;
    bool showVersion = false;
    std::string buildDiagnosticsJsonPath;
};

static void printUsage(const char* prog) {
    std::cout
        << "Kern Standalone Compiler (kern)\n\n"
        << "Usage:\n"
        << "  " << prog << " input.kn -o output.exe [--release|--debug] [--opt 0..3] [--console|--no-console]\n"
        << "  " << prog << " --config kernconfig.json\n"
        << "  " << prog << " --fix-all [project-root] [--dry-run] [--report-json out.json]\n"
        << "  " << prog << " --analyze [project-root] [--report-json out.json]\n"
        << "  " << prog << " --undo-fixes <backup-path>\n"
        << "  " << prog << " --target kernel entry.kn -o dist/kernel.bin [--freestanding] [--iso] [--run-qemu]\n"
        << "  " << prog << " --pkg-init [project-root]\n"
        << "  " << prog << " --pkg-lock [project-root]\n"
        << "  " << prog << " --pkg-validate [project-root]\n"
        << "Options:\n"
        << "  --config <path>   Load build config from kernconfig.json\n"
        << "  -o <path>         Output EXE path\n"
        << "  --release         Release mode (default)\n"
        << "  --debug           Debug mode\n"
        << "  --opt <0..3>      Optimization level\n"
        << "  --console         Console subsystem (default)\n"
        << "  --no-console      Windows subsystem\n"
        << "  --json            Machine-readable status output\n"
        << "  --build-diagnostics-json <path>  Write compile diagnostics JSON (array) for GUI/tools\n"
        << "  --fix-all         Analyze all project files and apply safe autofixes\n"
        << "  --analyze         Analyze all project files without modifying files\n"
        << "  --dry-run         Preview fixes without writing files\n"
        << "  --report-json     Write detailed analysis report JSON file\n"
        << "  --undo-fixes      Restore files from backup folder created by --fix-all\n"
        << "  --target <name>   Build target: native (default) or kernel\n"
        << "  --freestanding    Build with freestanding/system-level mode\n"
        << "  --iso             Build bootable ISO (kernel target)\n"
        << "  --run-qemu        Run kernel in QEMU after build\n"
        << "  --cross-prefix    Cross toolchain prefix (default x86_64-elf-)\n"
        << "  --arch <name>     Kernel arch hint (default x86_64)\n"
        << "  --bootloader <x>  Bootloader mode (default grub)\n"
        << "  --linker-script   Custom kernel linker script path\n"
        << "  --feature-set     stable | preview | experimental\n"
        << "  --enable-feature  Comma-separated feature IDs to force enable\n"
        << "  --disable-feature Comma-separated feature IDs to force disable\n"
        << "  --debug-runtime   Enable debug runtime guard profile metadata\n"
        << "  --allow-unsafe    Allow unsafe operations without unsafe block (runtime)\n"
        << "  --ffi             Enable ffi_call in runtime builds\n"
        << "  --ffi-allow <dll> Allow a DLL in runtime sandbox allowlist\n"
        << "  --no-sandbox      Disable runtime sandbox allowlist checks\n"
        << "  --pkg-init        Create kernpackage.json if missing\n"
        << "  --pkg-lock        Generate kern.lock.json from manifest\n"
        << "  --pkg-validate    Validate package manifest + lockfile\n"
        << "  --version, -V     Print version and exit\n"
        << "\nEnvironment:\n"
        << "  KERN_REPO_ROOT     Override Kern source tree for standalone link (default: detected from kern executable)\n";
}

static int runCommand(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static bool parseArgs(int argc, char** argv, CliOptions& o, std::string& error) {
    if (argc < 2) {
        printUsage(argv[0]);
        return false;
    }
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (a == "--version" || a == "-V") {
            o.showVersion = true;
        } else if (a == "--fix-all") {
            o.fixAll = true;
        } else if (a == "--analyze") {
            o.analyzeOnly = true;
        } else if (a == "--pkg-init") {
            o.pkgInit = true;
        } else if (a == "--pkg-lock") {
            o.pkgLock = true;
        } else if (a == "--pkg-validate") {
            o.pkgValidate = true;
        } else if (a == "--dry-run") {
            o.dryRun = true;
        } else if (a == "--target" && i + 1 < argc) {
            o.target = argv[++i];
            o.targetSet = true;
        } else if (a == "--freestanding") {
            o.freestanding = true;
        } else if (a == "--iso") {
            o.kernelIso = true;
        } else if (a == "--run-qemu") {
            o.runQemu = true;
        } else if (a == "--cross-prefix" && i + 1 < argc) {
            o.kernelCrossPrefix = argv[++i];
        } else if (a == "--arch" && i + 1 < argc) {
            o.kernelArch = argv[++i];
        } else if (a == "--bootloader" && i + 1 < argc) {
            o.kernelBootloader = argv[++i];
        } else if (a == "--linker-script" && i + 1 < argc) {
            o.kernelLinkerScript = argv[++i];
        } else if (a == "--feature-set" && i + 1 < argc) {
            o.featureSet = argv[++i];
        } else if (a == "--enable-feature" && i + 1 < argc) {
            std::string raw = argv[++i];
            std::stringstream ss(raw);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) o.enabledFeatures.push_back(tok);
        } else if (a == "--disable-feature" && i + 1 < argc) {
            std::string raw = argv[++i];
            std::stringstream ss(raw);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) o.disabledFeatures.push_back(tok);
        } else if (a == "--debug-runtime") {
            o.debugRuntime = true;
        } else if (a == "--allow-unsafe") {
            o.allowUnsafe = true;
        } else if (a == "--ffi") {
            o.ffiEnabled = true;
        } else if (a == "--ffi-allow" && i + 1 < argc) {
            o.ffiAllowLibraries.push_back(argv[++i]);
        } else if (a == "--no-sandbox") {
            o.sandboxEnabled = false;
        } else if (a == "--report-json" && i + 1 < argc) {
            o.reportJsonPath = argv[++i];
        } else if (a == "--undo-fixes" && i + 1 < argc) {
            o.undoBackupPath = argv[++i];
        } else if (a == "--config" && i + 1 < argc) {
            o.configPath = argv[++i];
        } else if (a == "-o" && i + 1 < argc) {
            o.output = argv[++i];
        } else if (a == "--release") {
            o.release = true;
        } else if (a == "--debug") {
            o.release = false;
        } else if (a == "--console") {
            o.console = true;
        } else if (a == "--no-console") {
            o.console = false;
        } else if (a == "--opt" && i + 1 < argc) {
            o.opt = std::max(0, std::min(3, std::stoi(argv[++i])));
        } else if (a == "--json") {
            o.json = true;
        } else if (a == "--build-diagnostics-json" && i + 1 < argc) {
            o.buildDiagnosticsJsonPath = argv[++i];
        } else if (!a.empty() && a[0] != '-') {
            if (o.fixAll || o.analyzeOnly || o.pkgInit || o.pkgLock || o.pkgValidate) o.projectRoot = a;
            else o.input = a;
        } else {
            error = "unknown argument: " + a;
            return false;
        }
    }
    if (!o.undoBackupPath.empty()) return true;
    if (o.showVersion) return true;
    if (o.fixAll || o.analyzeOnly || o.pkgInit || o.pkgLock || o.pkgValidate) return true;
    if (o.input.empty() && o.configPath.empty()) {
        error = "expected input .kn file or --config";
        return false;
    }
    return true;
}

static bool fileRead(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

static uint64_t fileTimestamp(const std::string& path) {
    std::error_code ec;
    auto ft = fs::last_write_time(path, ec);
    if (ec) return 0;
    return static_cast<uint64_t>(ft.time_since_epoch().count());
}

static int emitError(const std::string& message, bool json) {
    if (json) {
        std::cout << "{\"ok\":false,\"error\":\"" << message << "\"}\n";
    } else {
        std::cerr << "kern: error: " << message << std::endl;
    }
    return 1;
}

static int emitEntryNotFoundError(const std::string& entry, bool json) {
    const char* hint = "Pass an existing .kn file, e.g. examples/basic/05_functions.kn or examples/graphics/3d_textured_scene.kn";
    if (json) {
        std::cout << "{\"ok\":false,\"error\":\"entry file not found: " << entry << "\",\"hint\":\"" << hint << "\"}\n";
    } else {
        std::cerr << "kern: error: entry file not found: " << entry << " | " << hint << std::endl;
    }
    return 1;
}

static int emitOk(const std::string& output, bool json) {
    if (json) {
        std::cout << "{\"ok\":true,\"output\":\"" << output << "\"}\n";
    } else {
        std::cout << "kern: built " << output << std::endl;
    }
    return 0;
}

static bool writeText(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

static int runAnalyzeMode(const CliOptions& cli) {
    AnalyzerOptions ao;
    ao.projectRoot = cli.projectRoot;
    ao.applyFixes = cli.fixAll;
    ao.dryRun = cli.dryRun;
    ao.includeRuntimeHeuristics = true;

    AnalyzerReport report = analyzeProjectAndMaybeFix(ao);
    std::string jsonReport = analyzerReportToJson(report);

    if (!cli.reportJsonPath.empty()) {
        fs::path out = fs::path(cli.reportJsonPath);
        if (!out.is_absolute()) out = fs::current_path() / out;
        std::error_code ec;
        fs::create_directories(out.parent_path(), ec);
        if (ec || !writeText(out.string(), jsonReport)) {
            return emitError("failed writing report JSON: " + out.string(), cli.json);
        }
    }

    if (cli.json) {
        std::cout << jsonReport << std::endl;
    } else {
        std::cout << "kern analyzer: scanned " << report.kernFiles.size()
                  << " .kn files and " << report.configFiles.size() << " config file(s)\n";
        std::cout << "  CRITICAL: " << report.criticalCount
                  << "  WARNING: " << report.warningCount
                  << "  INFO: " << report.infoCount << "\n";
        if (!report.backupRoot.empty()) std::cout << "  backup: " << report.backupRoot << "\n";
        if (!report.appliedChanges.empty()) {
            std::cout << "  applied fixes: " << report.appliedChanges.size() << "\n";
            for (size_t i = 0; i < std::min<size_t>(report.appliedChanges.size(), 20); ++i) {
                std::cout << "    - " << report.appliedChanges[i].file << " (" << report.appliedChanges[i].reason << ")\n";
            }
        }
        if (!report.pendingReviewChanges.empty()) {
            std::cout << "  dry-run fix candidates: " << report.pendingReviewChanges.size() << "\n";
        }
        for (const auto& issue : report.issues) {
            std::cout << "[" << severityToString(issue.severity) << "] "
                      << issue.file << ":" << issue.line << ":" << issue.column
                      << " " << issue.type << " - " << issue.message;
            if (!issue.fix.empty()) std::cout << " | Fix: " << issue.fix;
            if (issue.uncertain) std::cout << " | review-needed";
            std::cout << "\n";
        }
    }
    return report.criticalCount > 0 ? 1 : 0;
}

static std::string jsonEscapeSmall(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

static int runPackageMode(const CliOptions& cli) {
    fs::path root = fs::absolute(cli.projectRoot.empty() ? "." : cli.projectRoot);
    fs::path manifest = root / "kernpackage.json";
    fs::path lock = root / "kern.lock.json";
    std::error_code ec;
    fs::create_directories(root, ec);

    if (cli.pkgInit) {
        if (!fs::exists(manifest)) {
            std::string text =
                "{\n"
                "  \"name\": \"kern-project\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"entry\": \"main.kn\",\n"
                "  \"dependencies\": {}\n"
                "}\n";
            if (!writeText(manifest.string(), text)) return emitError("failed to write " + manifest.string(), cli.json);
        }
        if (cli.json) std::cout << "{\"ok\":true,\"manifest\":\"" << jsonEscapeSmall(manifest.string()) << "\"}\n";
        else std::cout << "kern: package manifest ready: " << manifest.string() << std::endl;
        return 0;
    }

    std::string manifestText;
    if (!fileRead(manifest.string(), manifestText)) return emitError("missing package manifest: " + manifest.string(), cli.json);

    if (cli.pkgLock) {
        std::string lockText =
            "{\n"
            "  \"lock_version\": 1,\n"
            "  \"generated_by\": \"kern\",\n"
            "  \"manifest_hash\": \"" + hashContent(manifestText) + "\",\n"
            "  \"packages\": []\n"
            "}\n";
        if (!writeText(lock.string(), lockText)) return emitError("failed to write lockfile: " + lock.string(), cli.json);
        if (cli.json) std::cout << "{\"ok\":true,\"lock\":\"" << jsonEscapeSmall(lock.string()) << "\"}\n";
        else std::cout << "kern: lockfile generated: " << lock.string() << std::endl;
        return 0;
    }

    if (cli.pkgValidate) {
        if (!fs::exists(lock)) return emitError("missing lockfile: " + lock.string(), cli.json);
        std::string lockText;
        if (!fileRead(lock.string(), lockText)) return emitError("failed to read lockfile: " + lock.string(), cli.json);
        std::string mh = hashContent(manifestText);
        bool ok = lockText.find(mh) != std::string::npos;
        if (!ok) return emitError("lockfile out of date; run --pkg-lock", cli.json);
        if (cli.json) std::cout << "{\"ok\":true,\"validated\":true}\n";
        else std::cout << "kern: package validation passed" << std::endl;
        return 0;
    }
    return emitError("invalid package mode", cli.json);
}

} // namespace

int main(int argc, char** argv) {
    CliOptions cli;
    std::string error;
    if (!parseArgs(argc, argv, cli, error)) {
        if (!error.empty()) return emitError(error, cli.json);
        return 1;
    }

    if (cli.showVersion) {
        std::cout << "kern 1.0.0\n";
        return 0;
    }

    if (!cli.undoBackupPath.empty()) {
        std::string undoError;
        if (!undoFixesFromBackup(cli.undoBackupPath, undoError)) return emitError(undoError, cli.json);
        if (cli.json) std::cout << "{\"ok\":true,\"undo\":\"done\"}\n";
        else std::cout << "kern: restored files from backup: " << cli.undoBackupPath << std::endl;
        return 0;
    }

    if (cli.fixAll || cli.analyzeOnly) {
        return runAnalyzeMode(cli);
    }
    if (cli.pkgInit || cli.pkgLock || cli.pkgValidate) {
        return runPackageMode(cli);
    }

    SplConfig cfg;
    if (!cli.configPath.empty()) {
        if (!loadSplConfig(cli.configPath, cfg, error)) return emitError(error, cli.json);
    } else {
        cfg.entry = cli.input;
        cfg.output = cli.output.empty() ? std::string("dist/app.exe") : cli.output;
        cfg.release = cli.release;
        cfg.console = cli.console;
        cfg.optimizationLevel = cli.opt;
    }
    if (!cli.output.empty()) cfg.output = cli.output;
    cfg.release = cli.release;
    cfg.console = cli.console;
    cfg.optimizationLevel = cli.opt;
    if (cli.targetSet) cfg.target = cli.target;
    if (!cli.kernelCrossPrefix.empty()) cfg.kernelCrossPrefix = cli.kernelCrossPrefix;
    if (!cli.kernelArch.empty()) cfg.kernelArch = cli.kernelArch;
    if (!cli.kernelBootloader.empty()) cfg.kernelBootloader = cli.kernelBootloader;
    if (!cli.kernelLinkerScript.empty()) cfg.kernelLinkerScript = cli.kernelLinkerScript;
    cfg.freestanding = cfg.freestanding || cli.freestanding || (cfg.target == "kernel");
    cfg.kernelBuildIso = cfg.kernelBuildIso || cli.kernelIso;
    cfg.kernelRunQemu = cfg.kernelRunQemu || cli.runQemu;
    if (!cli.featureSet.empty()) cfg.featureSet = cli.featureSet;
    for (const auto& f : cli.enabledFeatures) cfg.enabledFeatures.push_back(f);
    for (const auto& f : cli.disabledFeatures) cfg.disabledFeatures.push_back(f);
    cfg.debugRuntime = cfg.debugRuntime || cli.debugRuntime;
    cfg.allowUnsafe = cfg.allowUnsafe || cli.allowUnsafe;
    cfg.ffiEnabled = cfg.ffiEnabled || cli.ffiEnabled;
    if (!cli.sandboxEnabled) cfg.sandboxEnabled = false;
    for (const auto& lib : cli.ffiAllowLibraries) cfg.ffiAllowLibraries.push_back(lib);
    if (cfg.target == "kernel") {
        fs::path outGuess(cfg.output);
        if (outGuess.extension() == ".exe" || cfg.output.empty()) cfg.output = "dist/kernel.bin";
    }

    fs::path entryPath(cfg.entry);
    std::error_code ec;
    fs::path entryCan = fs::weakly_canonical(entryPath, ec);
    if (ec || !fs::exists(entryCan)) return emitEntryNotFoundError(cfg.entry, cli.json);
    cfg.entry = entryCan.string();

    fs::path outputPath(cfg.output);
    if (!outputPath.is_absolute()) outputPath = fs::current_path() / outputPath;
    cfg.output = outputPath.string();

    if (cfg.target == "kernel") {
        std::string kernelBin;
        std::string isoOut;
        if (!buildKernelArtifacts(cfg, kerncDetectSourceRoot().string(), kernelBin, isoOut, error)) {
            return emitError(error, cli.json);
        }
        if (cli.json) {
            std::cout << "{\"ok\":true,\"target\":\"kernel\",\"kernel_bin\":\"" << kernelBin
                      << "\",\"iso\":\"" << isoOut << "\"}\n";
        } else {
            std::cout << "kern: kernel built " << kernelBin << std::endl;
            if (!isoOut.empty()) std::cout << "kern: bootable iso " << isoOut << std::endl;
        }
        return 0;
    }

    ResolveOptions ro;
    ro.projectRoot = entryCan.parent_path().string();
    ro.includePaths = cfg.includePaths;
    if (ro.includePaths.empty()) ro.includePaths.push_back(ro.projectRoot);
    ro.excludeGlobs = cfg.exclude;

    std::vector<BuildDiagRecord> buildDiags;
    auto flushDiags = [&]() {
        if (cli.buildDiagnosticsJsonPath.empty()) return;
        writeBuildDiagnosticsJson(cli.buildDiagnosticsJsonPath, buildDiags);
    };

    ResolveResult resolved = resolveProjectGraph(cfg.entry, ro);
    if (!resolved.errors.empty()) {
        for (const auto& err : resolved.errors)
            buildDiags.push_back({"error", "", 0, 0, "resolve", err});
        flushDiags();
        return emitError(resolved.errors.front(), cli.json);
    }

    if (!cfg.explicitFiles.empty()) {
        for (const std::string& efRaw : cfg.explicitFiles) {
            fs::path ep(efRaw);
            std::error_code ecEx;
            fs::path efCan = fs::weakly_canonical(ep, ecEx);
            if (ecEx || !fs::exists(efCan)) {
                std::string msg = "explicit file not found: " + efRaw;
                buildDiags.push_back({"error", efRaw, 0, 0, "files", msg});
                flushDiags();
                return emitError(msg, cli.json);
            }
            std::string key = efCan.generic_string();
            if (resolved.modules.find(key) == resolved.modules.end()) {
                std::string msg = "explicit file not reachable from entry (missing import path): " + key;
                buildDiags.push_back({"error", key, 0, 0, "files", msg});
                flushDiags();
                return emitError(msg, cli.json);
            }
        }
    }

    // semantic analysis (module-aware, stability-first):
    const bool strictTypes = (cfg.featureSet == "preview" || cfg.featureSet == "experimental");
    int semWarnings = 0;
    int semErrors = 0;
    for (const auto& kv : resolved.modules) {
        SemanticResult sr = analyzeSemanticSource(kv.second.source, kv.first, strictTypes);
        for (const auto& d : sr.diagnostics) {
            buildDiags.push_back({semanticSeverityJson(d.severity), d.file, d.line, d.column, d.code, d.message});
            if (d.severity == SemanticSeverity::Error) ++semErrors;
            else if (d.severity == SemanticSeverity::Warning) ++semWarnings;
            if (!cli.json) {
                std::cerr << "kern semantic [" << semanticSeverityName(d.severity) << "] "
                          << d.file << ":" << d.line << ":" << d.column << " "
                          << d.code << " " << d.message << std::endl;
            }
        }
    }
    if (semErrors > 0) {
        flushDiags();
        if (cli.json) {
            std::cout << "{\"ok\":false,\"error\":\"semantic analysis failed\",\"semantic_errors\":" << semErrors << "}\n";
        }
        return 1;
    }

    // phase1 explicit diagnostic: kernc native backend does not lower extern/FFI declarations yet.
    for (const auto& kv : resolved.modules) {
        if (kv.second.source.find("extern def") != std::string::npos ||
            kv.second.source.find("extern function") != std::string::npos) {
            std::string msg = "kernc phase1 backend does not support extern/FFI declarations yet; run with `kern` runtime path for ffi_call.";
            buildDiags.push_back({"error", kv.first, 0, 0, "ffi-kernc-unsupported", msg});
            flushDiags();
            return emitError(msg + " file=" + kv.first, cli.json);
        }
    }

    // incremental cache: detect changed modules.
    fs::path cacheDir = outputPath.parent_path() / ".kern-cache";
    fs::create_directories(cacheDir, ec);
    fs::path cacheFile = cacheDir / "modules.cache";
    BuildCache cache;
    loadBuildCache(cacheFile.string(), cache);
    bool anyChanged = false;
    for (const auto& kv : resolved.modules) {
        const std::string& p = kv.first;
        const std::string& src = kv.second.source;
        std::string h = hashContent(src);
        uint64_t ts = fileTimestamp(p);
        auto it = cache.modules.find(p);
        if (it == cache.modules.end() || it->second.hash != h || it->second.timestamp != ts) {
            anyChanged = true;
        }
        cache.modules[p] = {h, ts};
    }

    if (cli.json) {
        std::cout << "{\"stage\":\"resolve\",\"modules\":" << resolved.modules.size() << ",\"feature_set\":\"" << cfg.featureSet << "\"}\n";
    } else {
        std::cout << "kern: resolved " << resolved.modules.size() << " module(s)"
                  << " [feature-set=" << cfg.featureSet << "]" << std::endl;
    }

    IRProgram ir = buildIRFromResolvedGraph(resolved);
    runTypedIRCanonicalization(ir);
    runTypedIRConstantPropagation(ir);
    runTypedIRDeadBlockElimination(ir);
    runDeadCodeElimination(ir);
    runConstantFolding(ir);
    runBasicInlining(ir);

    CppBackendResult gen;
    if (!generateCppBundle(ir, cfg, gen, error)) {
        buildDiags.push_back({"error", "", 0, 0, "codegen", error});
        flushDiags();
        return emitError(error, cli.json);
    }
    if (cli.json) std::cout << "{\"stage\":\"codegen\",\"file\":\"" << gen.generatedCpp << "\"}\n";

    // plugin commands (pre-build)
    for (const auto& cmd : cfg.preBuildCommands) {
        if (runCommand(cmd) != 0) {
            buildDiags.push_back({"error", "", 0, 0, "pre_build", "pre-build plugin command failed: " + cmd});
            flushDiags();
            return emitError("pre-build plugin command failed: " + cmd, cli.json);
        }
    }

    if (anyChanged || !fs::exists(cfg.output)) {
        if (!buildStandaloneExe(gen, cfg, kerncDetectSourceRoot().string(), error)) {
            buildDiags.push_back({"error", "", 0, 0, "link", error});
            flushDiags();
            return emitError(error, cli.json);
        }
    }

    for (const auto& cmd : cfg.postBuildCommands) {
        if (runCommand(cmd) != 0) {
            buildDiags.push_back({"error", "", 0, 0, "post_build", "post-build plugin command failed: " + cmd});
            flushDiags();
            return emitError("post-build plugin command failed: " + cmd, cli.json);
        }
    }

    saveBuildCache(cacheFile.string(), cache);
    flushDiags();
    return emitOk(cfg.output, cli.json);
}
