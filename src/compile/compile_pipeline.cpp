#include "compile/compile_pipeline.hpp"

#include "compiler/project_resolver.hpp"
#include "compiler/semantic.hpp"

#include <filesystem>
#include "ir/ir_builder.hpp"
#include "ir/passes/passes.hpp"
#include "utils/build_cache.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace kern {
namespace compile {
namespace fs = std::filesystem;

namespace {

static int runShellCommand(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static uint64_t fileTimestamp(const std::string& path) {
    std::error_code ec;
    auto ft = fs::last_write_time(path, ec);
    if (ec) return 0;
    return static_cast<uint64_t>(ft.time_since_epoch().count());
}

static std::string jsonEscape(const std::string& s) {
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

static bool writeDiagnosticsJson(const std::string& path, const std::vector<Diagnostic>& diags) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "[";
    for (size_t i = 0; i < diags.size(); ++i) {
        if (i) out << ",";
        const auto& d = diags[i];
        out << "\n{\"severity\":\"" << jsonEscape(d.severity) << "\""
            << ",\"file\":\"" << jsonEscape(d.file) << "\""
            << ",\"line\":" << d.line << ",\"column\":" << d.column << ",\"code\":\"" << jsonEscape(d.code)
            << "\",\"message\":\"" << jsonEscape(d.message) << "\"}";
    }
    out << "\n]\n";
    return static_cast<bool>(out);
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

} // namespace

bool optionsToSplConfig(const Options& opts, SplConfig& cfg, std::string& error) {
    (void)error;
    cfg.entry = opts.entryKn;
    cfg.output = opts.outputExe;
    cfg.includePaths = opts.includePaths;
    cfg.explicitFiles = opts.extraKnFiles;
    cfg.assets = opts.assets;
    cfg.icon = opts.iconPath;
    cfg.optimizationLevel = opts.optimizationLevel;
    cfg.release = opts.release;
    cfg.console = opts.consoleSubsystem;
    cfg.featureSet = opts.featureSet;
    cfg.preBuildCommands = opts.preBuildCommands;
    cfg.postBuildCommands = opts.postBuildCommands;
    return true;
}

bool runStandaloneExecutablePipeline(SplConfig& cfg, const std::string& kernRepoRoot, const PipelineParams& params,
    StandaloneBackend* backend, Result& out) {
    out = Result{};
    StandaloneBackend defaultBackend;
    if (!backend) backend = &defaultBackend;

    auto flushDiags = [&]() {
        if (!params.buildDiagnosticsJsonPath.empty())
            writeDiagnosticsJson(params.buildDiagnosticsJsonPath, out.diagnostics);
    };

    fs::path entryPath(cfg.entry);
    std::error_code ec;
    fs::path entryCan = fs::weakly_canonical(entryPath, ec);
    if (ec || !fs::exists(entryCan)) {
        out.errorSummary = "entry file not found: " + cfg.entry;
        out.diagnostics.push_back({"error", cfg.entry, 0, 0, "entry", out.errorSummary});
        flushDiags();
        return false;
    }
    cfg.entry = entryCan.string();

    fs::path outputPath(cfg.output);
    if (!outputPath.is_absolute()) outputPath = fs::current_path(ec) / outputPath;
    cfg.output = outputPath.string();

    ResolveOptions ro;
    ro.projectRoot = entryCan.parent_path().string();
    ro.includePaths = cfg.includePaths;
    if (ro.includePaths.empty()) ro.includePaths.push_back(ro.projectRoot);
    ro.excludeGlobs = cfg.exclude;

    ResolveResult resolved = resolveProjectGraph(cfg.entry, ro);
    if (!resolved.errors.empty()) {
        for (const auto& err : resolved.errors)
            out.diagnostics.push_back({"error", "", 0, 0, "resolve", err});
        out.errorSummary = resolved.errors.empty() ? "resolve failed" : resolved.errors.front();
        flushDiags();
        return false;
    }

    if (!cfg.explicitFiles.empty()) {
        for (const std::string& efRaw : cfg.explicitFiles) {
            fs::path ep(efRaw);
            std::error_code ecEx;
            fs::path efCan = fs::weakly_canonical(ep, ecEx);
            if (ecEx || !fs::exists(efCan)) {
                std::string msg = "explicit file not found: " + efRaw;
                out.diagnostics.push_back({"error", efRaw, 0, 0, "files", msg});
                out.errorSummary = msg;
                flushDiags();
                return false;
            }
            std::string key = efCan.generic_string();
            if (resolved.modules.find(key) == resolved.modules.end()) {
                std::string msg = "explicit file not reachable from entry (missing import path): " + key;
                out.diagnostics.push_back({"error", key, 0, 0, "files", msg});
                out.errorSummary = msg;
                flushDiags();
                return false;
            }
        }
    }

    const bool strictTypes = (cfg.featureSet == "preview" || cfg.featureSet == "experimental");
    int semErrors = 0;
    for (const auto& kv : resolved.modules) {
        SemanticResult sr = analyzeSemanticSource(kv.second.source, kv.first, strictTypes);
        for (const auto& d : sr.diagnostics) {
            out.diagnostics.push_back(
                {semanticSeverityJson(d.severity), d.file, d.line, d.column, d.code, d.message});
            if (d.severity == SemanticSeverity::Error) ++semErrors;
            if (!params.quietHumanSemantic) {
                std::cerr << "kern semantic [" << semanticSeverityName(d.severity) << "] " << d.file << ":" << d.line
                          << ":" << d.column << " " << d.code << " " << d.message << std::endl;
            }
        }
    }
    if (semErrors > 0) {
        out.errorSummary = "semantic analysis failed";
        flushDiags();
        if (params.emitStdoutJson)
            std::cout << "{\"ok\":false,\"error\":\"semantic analysis failed\",\"semantic_errors\":" << semErrors << "}\n";
        return false;
    }

    for (const auto& kv : resolved.modules) {
        if (kv.second.source.find("extern def") != std::string::npos ||
            kv.second.source.find("extern function") != std::string::npos) {
            std::string msg =
                "standalone phase1 backend does not support extern/FFI declarations yet; use `kern` for ffi_call.";
            out.diagnostics.push_back({"error", kv.first, 0, 0, "ffi-standalone-unsupported", msg});
            out.errorSummary = msg;
            flushDiags();
            return false;
        }
    }

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

    if (params.emitStdoutJson) {
        std::cout << "{\"stage\":\"resolve\",\"modules\":" << resolved.modules.size() << ",\"feature_set\":\""
                  << cfg.featureSet << "\"}\n";
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
    std::string genErr;
    if (!backend->emitHostSources(ir, cfg, gen, genErr)) {
        out.diagnostics.push_back({"error", "", 0, 0, "codegen", genErr});
        out.errorSummary = genErr;
        flushDiags();
        return false;
    }
    out.generatedCppPath = gen.generatedCpp;
    if (params.emitStdoutJson) std::cout << "{\"stage\":\"codegen\",\"file\":\"" << jsonEscape(gen.generatedCpp) << "\"}\n";

    for (const auto& cmd : cfg.preBuildCommands) {
        if (runShellCommand(cmd) != 0) {
            std::string msg = "pre-build plugin command failed: " + cmd;
            out.diagnostics.push_back({"error", "", 0, 0, "pre_build", msg});
            out.errorSummary = msg;
            flushDiags();
            return false;
        }
    }

    if (anyChanged || !fs::exists(cfg.output)) {
        std::string linkErr;
        if (!backend->linkExecutable(gen, cfg, kernRepoRoot, linkErr)) {
            out.diagnostics.push_back({"error", "", 0, 0, "link", linkErr});
            out.errorSummary = linkErr;
            flushDiags();
            return false;
        }
    }

    for (const auto& cmd : cfg.postBuildCommands) {
        if (runShellCommand(cmd) != 0) {
            std::string msg = "post-build plugin command failed: " + cmd;
            out.diagnostics.push_back({"error", "", 0, 0, "post_build", msg});
            out.errorSummary = msg;
            flushDiags();
            return false;
        }
    }

    saveBuildCache(cacheFile.string(), cache);
    out.outputExecutable = cfg.output;
    out.success = true;
    flushDiags();
    return true;
}

bool compileStandaloneExecutable(const Options& opts, const std::string& kernRepoRoot, const PipelineParams& params,
    StandaloneBackend* backend, Result& out) {
    SplConfig cfg;
    std::string convErr;
    if (!optionsToSplConfig(opts, cfg, convErr)) {
        out.success = false;
        out.errorSummary = convErr;
        return false;
    }
    return runStandaloneExecutablePipeline(cfg, kernRepoRoot, params, backend, out);
}

} // namespace compile
} // namespace kern
