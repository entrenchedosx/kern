#ifndef KERN_COMPILER_PROJECT_RESOLVER_HPP
#define KERN_COMPILER_PROJECT_RESOLVER_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace kern {

struct ResolvedModule {
    std::string canonicalPath;
    std::string source;
    std::vector<std::string> dependencies;
};

struct ResolveOptions {
    std::string projectRoot;
    std::vector<std::string> includePaths;
    std::vector<std::string> excludeGlobs;
};

struct ResolveResult {
    std::unordered_map<std::string, ResolvedModule> modules;
    std::vector<std::string> topologicalOrder;
    std::vector<std::string> errors;
};

ResolveResult resolveProjectGraph(const std::string& entryFile, const ResolveOptions& options);

} // namespace kern

#endif // kERN_COMPILER_PROJECT_RESOLVER_HPP
