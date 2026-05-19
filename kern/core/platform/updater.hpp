/* *
 * updater.hpp — Self-updater for the `kern` CLI compiler binary.
 *
 * Provides a single public entry point `kern::handleUpdate()` that:
 *   1. Contacts the GitHub Releases API for `entrenchedosx/kern`
 *   2. Compares the latest tag against the running version
 *   3. If newer, downloads the platform-specific binary asset
 *   4. Performs a hot-swap of the running executable
 *
 * Platform back-ends:
 *   Windows: WinHTTP (HTTPS), GetModuleFileNameW, rename-and-drop, DETACHED_PROCESS cleanup
 *   Linux:   popen("curl ..."), readlink /proc/self/exe, unlink + rename + chmod
 *   macOS:   popen("curl ..."), _NSGetExecutablePath, unlink + rename + chmod
 *
 * No external dependencies beyond the OS native API / POSIX + libc.
 * C++17, header-only types; implementation in updater.cpp.
 */

#pragma once

#include <string>

namespace kern {

// ---------------------------------------------------------------------------
// Public API — single entry point called by `kern --update`
// ---------------------------------------------------------------------------

/**
 * @brief Check GitHub for a newer version of `kern` and apply it if found.
 *
 * Progression:
 *   1. Resolves the currently running executable's filesystem path.
 *   2. Queries https://api.github.com/repos/entrenchedosx/kern/releases/latest.
 *   3. Parses the JSON response to extract `tag_name` and the download URL
 *      of the binary matching the current platform.
 *   4. If the tag differs from `currentVersion`, downloads the new binary to
 *      a temporary path, then performs an OS-specific hot-swap:
 *      - Windows:   rename existing → kern.old, move downloaded → exe path,
 *                   spawn DETACHED_PROCESS to delete kern.old after a short delay.
 *      - Linux:     unlink existing, rename downloaded into place, chmod 0755.
 *      - macOS:     same as Linux.
 *
 * @param currentVersion  The version string of the running binary
 *                        (e.g. "2.2.0"). Compared lexicographically to the
 *                        GitHub release tag (prefix "v" stripped).
 *
 * Behaviour on error:
 *   Prints a descriptive message to std::cerr. Does NOT terminate the process.
 *   Returns without modifying the filesystem if a recoverable error occurs.
 */
void handleUpdate(const std::string& currentVersion);

// ---------------------------------------------------------------------------
// Internal helpers (exposed for testability)
// ---------------------------------------------------------------------------

/// Resolve the absolute path to the currently running executable.
std::string getCurrentExecutablePath();

/// Build the GitHub API URL for the latest release.
std::string buildGitHubApiUrl();

/// Determine the platform-specific binary filename that the updater should
/// look for in the release assets (e.g. "kern-x86_64-pc-windows-msvc.exe").
std::string getPlatformBinaryName();

/// Download a URL to a local file path. Returns true on success.
/// On failure, sets @p errorMsg with a human-readable description.
bool downloadToFile(const std::string& url, const std::string& destPath,
                    std::string& errorMsg);

/// Perform the hot-swap: replace @p targetPath with @p downloadedPath.
/// @param downloadedPath  Path to the freshly downloaded binary.
/// @param targetPath      Path to the currently running executable.
/// @param errorMsg        Filled on failure.
/// @returns true on success.
bool applyBinarySwap(const std::string& downloadedPath,
                     const std::string& targetPath,
                     std::string& errorMsg);

} // namespace kern
