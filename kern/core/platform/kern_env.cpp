#include "platform/kern_env.hpp"

#include "platform/env_compat.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace kern {
namespace fs = std::filesystem;

// Used by cache/env.json helpers below before this TU's definition.
bool isValidKernRoot(const fs::path& root);

namespace {

std::string g_kernHomeOverride;

static std::string trimSpace(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

static bool fileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec) && !ec;
}

static bool dirExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p, ec) && !ec;
}

static bool rootHasKernExe(const fs::path& root) {
#ifdef _WIN32
    return fileExists(root / "kern.exe");
#else
    return fileExists(root / "kern");
#endif
}

static fs::path kernRootCacheFilePath() {
#ifdef _WIN32
    const char* appData = kernGetEnv("APPDATA");
    if (!appData || !appData[0]) return {};
    return fs::path(appData) / "kern" / "root.txt";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    fs::path base = xdg && xdg[0] ? fs::path(xdg) : (fs::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / ".config");
    return base / "kern" / "root.txt";
#endif
}

static void deleteKernRootCacheFile() {
    std::error_code ec;
    fs::path cf = kernRootCacheFilePath();
    if (cf.empty()) return;
    fs::remove(cf, ec);
}

static std::optional<fs::path> readCacheRootPathValidated() {
    fs::path cacheFile = kernRootCacheFilePath();
    if (cacheFile.empty()) return std::nullopt;
    std::ifstream f(cacheFile, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string line = trimSpace(ss.str());
    if (line.empty()) return std::nullopt;
    fs::path p(line);
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        deleteKernRootCacheFile();
        return std::nullopt;
    }
    fs::path canon = fs::weakly_canonical(p, ec);
    if (!isValidKernRoot(canon)) {
        deleteKernRootCacheFile();
        return std::nullopt;
    }
    return canon;
}

static std::optional<fs::path> tryReadEnvJsonRoot(const fs::path& exeParent) {
    fs::path j = exeParent / "config" / "env.json";
    std::error_code ec;
    if (!fs::is_regular_file(j, ec)) return std::nullopt;
    std::ifstream f(j, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string raw = ss.str();
    static const std::regex re("\"root\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch m;
    if (!std::regex_search(raw, m, re) || m.size() < 2) return std::nullopt;
    fs::path pathOut(trimSpace(m[1].str()));
    if (pathOut.empty()) return std::nullopt;
    std::error_code ec2;
    if (!fs::exists(pathOut, ec2)) return std::nullopt;
    fs::path canon = fs::weakly_canonical(pathOut, ec2);
    if (!isValidKernRoot(canon)) return std::nullopt;
    return canon;
}

} // namespace

void initKernEnvironmentFromArgv(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--root") == 0) {
            setKernHomePathOverride(argv[i + 1]);
            return;
        }
    }
}

void setKernHomePathOverride(const std::string& utf8Path) {
    g_kernHomeOverride = trimSpace(utf8Path);
}

fs::path getKernExecutablePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return fs::path(std::string(buf, n));
#else
    char self[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (len > 0) {
        self[len] = '\0';
        return fs::path(self);
    }
#endif
    std::error_code ec;
    return fs::current_path(ec);
}

bool isFullKernEnvironmentRoot(const fs::path& root) {
    if (!rootHasKernExe(root)) return false;
#ifdef _WIN32
    if (!fileExists(root / "kargo.exe")) return false;
#else
    if (!fileExists(root / "kargo")) return false;
#endif
    if (!dirExists(root / "lib")) return false;
    if (!dirExists(root / "runtime")) return false;
    if (!dirExists(root / "packages")) return false;
    if (!dirExists(root / "cache")) return false;
    if (!dirExists(root / "config")) return false;
    return true;
}

bool isValidKernRoot(const fs::path& root) {
    std::error_code ec;
    if (!dirExists(root)) return false;
    if (!rootHasKernExe(root)) return false;
#ifdef _WIN32
    if (!fileExists(root / "kargo.exe")) return false;
#else
    if (!fileExists(root / "kargo")) return false;
#endif
    if (!dirExists(root / "lib")) return false;
    if (!dirExists(root / "runtime")) return false;
    if (!dirExists(root / "lib" / "kern")) return false;
    return true;
}

bool isResolvedToolchainRoot(const fs::path& root) {
    if (isValidKernRoot(root)) return true;
#ifdef KERN_DEV_BUILD
    return dirExists(root / "lib" / "kern");
#else
    return false;
#endif
}

bool isAcceptableKernRoot(const fs::path& root) {
    return isResolvedToolchainRoot(root);
}

std::optional<fs::path> tryResolveKernRoot() {
    std::error_code ec;

    if (!g_kernHomeOverride.empty()) {
        fs::path p(g_kernHomeOverride);
        if (isValidKernRoot(p)) return fs::weakly_canonical(p, ec);
    }

    const char* envHome = kernGetEnv("KERN_HOME");
    if (envHome && envHome[0]) {
        fs::path p(envHome);
        if (isValidKernRoot(p)) return fs::weakly_canonical(p, ec);
    }

    fs::path exe = getKernExecutablePath();
    fs::path parent = exe.parent_path();
    if (isValidKernRoot(parent)) return fs::weakly_canonical(parent, ec);

    if (auto fromJson = tryReadEnvJsonRoot(parent)) return *fromJson;

#ifdef KERN_DEV_BUILD
    // Dev/monorepo (Debug builds only): binaries under build/... while `lib/kern` lives at repo root.
    for (fs::path p = parent;; p = p.parent_path()) {
        if (dirExists(p / "lib" / "kern")) return fs::weakly_canonical(p, ec);
        if (p.empty() || p == p.root_path()) break;
    }
#endif

    if (auto cached = readCacheRootPathValidated()) return *cached;

    return std::nullopt;
}

fs::path resolveKernConfigDir() {
    std::error_code ec;
    if (auto r = tryResolveKernRoot()) return *r / "config";
    return fs::current_path(ec) / "config";
}

fs::path resolvePackagePathsJsonFile() {
    std::error_code ec;
    // Prefer environment root (where native kargo writes) so imports work without activation scripts.
    if (auto r = tryResolveKernRoot()) {
        fs::path envCfg = *r / "config" / "package-paths.json";
        if (fs::is_regular_file(envCfg, ec)) return envCfg;
    }
    fs::path cwd = fs::current_path(ec);
    fs::path local = cwd / "config" / "package-paths.json";
    if (fs::is_regular_file(local, ec)) return local;
    if (auto r = tryResolveKernRoot()) return *r / "config" / "package-paths.json";
    return local;
}

} // namespace kern
