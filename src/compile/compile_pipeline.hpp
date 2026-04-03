#ifndef KERN_COMPILE_COMPILE_PIPELINE_HPP
#define KERN_COMPILE_COMPILE_PIPELINE_HPP

#include "backend/cpp_backend.hpp"
#include "ir/ir.hpp"
#include "utils/kernconfig.hpp"

#include <memory>
#include <string>
#include <vector>

namespace kern {
namespace compile {

struct Diagnostic {
    std::string severity;
    std::string file;
    int line = 0;
    int column = 0;
    std::string code;
    std::string message;
};

struct Result {
    bool success = false;
    std::string outputExecutable;
    std::string generatedCppPath;
    std::string errorSummary;
    std::vector<Diagnostic> diagnostics;
};

/** High-level options mirroring common CLI / IDE integration needs. Maps to SplConfig. */
struct Options {
    std::string entryKn;
    std::string outputExe;
    std::vector<std::string> includePaths;
    /** Optional explicit .kn paths (union with entry); must be reachable from import graph. */
    std::vector<std::string> extraKnFiles;
    std::vector<std::string> assets;
    std::string iconPath;
    int optimizationLevel = 2;
    bool release = true;
    bool consoleSubsystem = true;
    std::string featureSet = "stable";
    std::vector<std::string> preBuildCommands;
    std::vector<std::string> postBuildCommands;
};

/**
 * Plugin hook for alternate backends (e.g. future LLVM or precompiled-bytecode hosts).
 * Default implementation emits C++ translation unit with embedded .kn sources + links the existing VM.
 */
class StandaloneBackend {
public:
    virtual ~StandaloneBackend() = default;
    virtual bool emitHostSources(const IRProgram& ir, const SplConfig& cfg, CppBackendResult& out, std::string& error) {
        return generateCppBundle(ir, cfg, out, error);
    }
    virtual bool linkExecutable(const CppBackendResult& gen, const SplConfig& cfg, const std::string& kernRepoRoot,
        std::string& error) {
        return buildStandaloneExe(gen, cfg, kernRepoRoot, error);
    }
};

struct PipelineParams {
    /** When set, append diagnostics as JSON array (same schema as kernc --build-diagnostics-json). */
    std::string buildDiagnosticsJsonPath;
    /** Machine-readable stage lines on stdout (kernc --json). */
    bool emitStdoutJson = false;
    /** Suppress per-diagnostic human lines on stderr (set when emitStdoutJson). */
    bool quietHumanSemantic = false;
};

bool optionsToSplConfig(const Options& opts, SplConfig& cfg, std::string& error);

/**
 * Full native Windows .exe pipeline: resolve imports → semantic scan → IR + passes → backend → link.
 * @param cfg entry/output must be absolute/canonical paths; other fields as for kernc.
 * @param kernRepoRoot KERN source tree (contains src/compiler/lexer.cpp); use KERN_REPO_ROOT detection from kernc.
 * @param backend null = default C++ embedded-source + VM link.
 */
bool runStandaloneExecutablePipeline(SplConfig& cfg, const std::string& kernRepoRoot, const PipelineParams& params,
    StandaloneBackend* backend, Result& out);

/** Convenience: Options → SplConfig → pipeline. */
bool compileStandaloneExecutable(const Options& opts, const std::string& kernRepoRoot, const PipelineParams& params,
    StandaloneBackend* backend, Result& out);

} // namespace compile
} // namespace kern

#endif
