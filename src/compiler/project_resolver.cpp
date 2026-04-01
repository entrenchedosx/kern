#include "compiler/project_resolver.hpp"
#include "compiler/import_aliases.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace kern {
namespace fs = std::filesystem;

namespace {

enum class NodeState { Unvisited, Visiting, Done };

static std::string readText(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static std::vector<std::string> discoverImports(const std::string& source) {
    std::vector<std::string> out;
    std::regex reA(R"re(\bimport\s*"([^"]+)")re");
    std::regex reB(R"re(\binclude\s*"([^"]+)")re");
    std::regex reC(R"re(\bimport\s*\(\s*"([^"]+)"\s*\))re");
    for (const auto* re : {&reA, &reB, &reC}) {
        for (std::sregex_iterator it(source.begin(), source.end(), *re), end; it != end; ++it) {
            out.push_back((*it)[1].str());
        }
    }
    return out;
}

static std::string normalizeIncludeSyntax(const std::string& source) {
    std::regex includeRe(R"re(\binclude\s*"([^"]+)")re");
    return std::regex_replace(source, includeRe, "import \"$1\"");
}

static bool excluded(const fs::path& p, const ResolveOptions& options) {
    std::string s = p.generic_string();
    for (const auto& ex : options.excludeGlobs) {
        if (!ex.empty() && s.find(ex) != std::string::npos) return true;
    }
    return false;
}

static fs::path resolveImportPath(const fs::path& importer, const std::string& raw, const ResolveOptions& options) {
    std::vector<fs::path> probes;
    fs::path r(raw);
    if (r.extension().empty()) r += ".kn";
    if (r.is_absolute()) probes.push_back(r);
    probes.push_back(importer.parent_path() / r);
    if (!options.projectRoot.empty()) probes.push_back(fs::path(options.projectRoot) / r);
    for (const auto& inc : options.includePaths) {
        fs::path incPath(inc);
        if (!incPath.is_absolute() && !options.projectRoot.empty()) incPath = fs::path(options.projectRoot) / incPath;
        probes.push_back(incPath / r);
    }
    for (auto& p : probes) {
        std::error_code ec;
        fs::path can = fs::weakly_canonical(p, ec);
        if (!ec && fs::exists(can)) return can;
    }
    return {};
}

static void dfsResolve(const fs::path& path, const ResolveOptions& options,
                       std::unordered_map<std::string, ResolvedModule>& modules,
                       std::unordered_map<std::string, NodeState>& state,
                       std::vector<std::string>& order,
                       std::vector<std::string>& errors,
                       std::vector<std::string>& stack) {
    std::string key = path.generic_string();
    if (excluded(path, options)) return;
    auto st = state.find(key);
    if (st != state.end() && st->second == NodeState::Done) return;
    if (st != state.end() && st->second == NodeState::Visiting) {
        std::string cycle = "circular import detected: ";
        for (const auto& s : stack) cycle += s + " -> ";
        cycle += key;
        errors.push_back(cycle);
        return;
    }
    state[key] = NodeState::Visiting;
    stack.push_back(key);

    std::string src = readText(path);
    if (src.empty()) {
        errors.push_back("failed to read module: " + key);
        state[key] = NodeState::Done;
        stack.pop_back();
        return;
    }

    src = normalizeIncludeSyntax(src);

    ResolvedModule mod;
    mod.canonicalPath = key;
    mod.source = src;
    auto imports = discoverImports(src);
    for (const auto& rawDep : imports) {
        // builtins / stdlib / game modules — no .kn file; satisfied by __import at runtime.
        if (isVirtualResolvedImport(rawDep) || isIntentionalMissingImportFixture(rawDep)) {
            continue;
        }
        fs::path dep = resolveImportPath(path, rawDep, options);
        if (dep.empty()) {
            errors.push_back("missing import from " + key + ": " + rawDep);
            continue;
        }
        std::string depKey = dep.generic_string();
        mod.dependencies.push_back(depKey);
        dfsResolve(dep, options, modules, state, order, errors, stack);
    }
    modules[key] = std::move(mod);
    state[key] = NodeState::Done;
    order.push_back(key);
    stack.pop_back();
}

} // namespace

ResolveResult resolveProjectGraph(const std::string& entryFile, const ResolveOptions& options) {
    ResolveResult result;
    std::error_code ec;
    fs::path entry = fs::weakly_canonical(fs::path(entryFile), ec);
    if (ec || !fs::exists(entry)) {
        result.errors.push_back("entry file not found: " + entryFile);
        return result;
    }
    std::unordered_map<std::string, NodeState> state;
    std::vector<std::string> stack;
    dfsResolve(entry, options, result.modules, state, result.topologicalOrder, result.errors, stack);
    return result;
}

} // namespace kern
