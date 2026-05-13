#ifndef KERN_CORE_PLATFORM_KERN_ENV_HPP
#define KERN_CORE_PLATFORM_KERN_ENV_HPP

#include <filesystem>
#include <optional>
#include <string>

namespace kern {

/// Call once at process start (before imports). Parses `--root <path>` without consuming argv.
void initKernEnvironmentFromArgv(int argc, char** argv);

/// Explicit override (UTF-8 path). Empty clears override.
void setKernHomePathOverride(const std::string& utf8Path);

std::filesystem::path getKernExecutablePath();

/// Resolution order: override → KERN_HOME → executable parent → config/env.json (`root`) →
/// dev parent-walk (Debug only) → validated cache file (`root.txt`, deleted if stale).
/// Override, KERN_HOME, and exe parent require a **strict** root (`isValidKernRoot`); dev walk
/// returns a repo with `lib/kern/` only in Debug builds.
std::optional<std::filesystem::path> tryResolveKernRoot();

/// Full portable tree (kern.exe, kargo.exe, lib, runtime, packages, cache, config).
bool isFullKernEnvironmentRoot(const std::filesystem::path& root);

/// Strict portable/toolchain root: both executables, `lib/`, `lib/kern/`, and `runtime/`.
/// Used for cache, env.json, and rejecting half-installed trees.
bool isValidKernRoot(const std::filesystem::path& root);

/// True after `tryResolveKernRoot()`: strict portable root, or (Debug only) a dev tree with `lib/kern/`.
bool isResolvedToolchainRoot(const std::filesystem::path& root);

/// Alias for `isResolvedToolchainRoot`.
bool isAcceptableKernRoot(const std::filesystem::path& root);

/// `config/package-paths.json` for Kargo imports (project-local if no KERN root).
std::filesystem::path resolvePackagePathsJsonFile();

/// `config/` under resolved root, or cwd/config.
std::filesystem::path resolveKernConfigDir();

} // namespace kern

#endif
