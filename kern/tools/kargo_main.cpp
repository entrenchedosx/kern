/* Native kargo: kargo.json + kargo.lock, project-local packages/, config/package-paths.json */

#include "platform/kern_env.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string trim(std::string s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') o.push_back('\\');
        o.push_back(c);
    }
    return o;
}

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool writeFile(const fs::path& p, const std::string& data) {
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << data;
    return true;
}

std::string runCapture(const std::string& cmd) {
    std::string out;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return trim(out);
}

int runCmd(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string sanitizeKeyFolder(const std::string& key) {
    std::string s;
    for (char c : key) {
        if (c == '/' || c == '\\') s.push_back('_');
        else s.push_back(c);
    }
    return s;
}

bool parseOwnerRepo(const std::string& spec, std::string& owner, std::string& repo, std::string& ref) {
    ref.clear();
    std::string s = spec;
    size_t at = s.find('@');
    if (at != std::string::npos) {
        ref = trim(s.substr(at + 1));
        s = trim(s.substr(0, at));
    }
    size_t slash = s.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= s.size()) return false;
    owner = trim(s.substr(0, slash));
    repo = trim(s.substr(slash + 1));
    return !owner.empty() && !repo.empty();
}

fs::path findMainFile(const fs::path& pkgRoot) {
    for (const char* name : {"index.kn", "main.kn", "src/index.kn"}) {
        fs::path p = pkgRoot / name;
        std::error_code ec;
        if (fs::is_regular_file(p, ec)) return fs::weakly_canonical(p, ec);
    }
    return {};
}

struct DepMap {
    std::map<std::string, std::string> deps; // key -> ref (tag or branch)
};

bool loadKargoJson(const fs::path& p, DepMap& out, std::string& err) {
    out.deps.clear();
    std::string raw = readFile(p);
    if (raw.empty()) {
        err = "missing or empty kargo.json";
        return false;
    }
    size_t pos = raw.find("\"dependencies\"");
    if (pos == std::string::npos) {
        err = "kargo.json: no dependencies object";
        return false;
    }
    size_t brace = raw.find('{', pos);
    if (brace == std::string::npos) return false;
    int depth = 0;
    size_t bodyStart = 0;
    for (size_t i = brace; i < raw.size(); ++i) {
        if (raw[i] == '{') {
            if (depth == 0) bodyStart = i + 1;
            ++depth;
        } else if (raw[i] == '}') {
            --depth;
            if (depth == 0) {
                std::string body = raw.substr(bodyStart, i - bodyStart);
                std::regex pairRe("\"([^\"]+)\"\\s*:\\s*\"([^\"]*)\"");
                for (std::sregex_iterator it(body.begin(), body.end(), pairRe), end; it != end; ++it) {
                    if ((*it).size() >= 3) out.deps[(*it)[1].str()] = (*it)[2].str();
                }
                return true;
            }
        }
    }
    err = "kargo.json: malformed dependencies";
    return false;
}

void writeKargoJson(const fs::path& p, const DepMap& dm) {
    std::ostringstream o;
    o << "{\n  \"dependencies\": {\n";
    bool first = true;
    for (const auto& kv : dm.deps) {
        if (!first) o << ",\n";
        first = false;
        o << "    \"" << jsonEscape(kv.first) << "\": \"" << jsonEscape(kv.second) << "\"";
    }
    o << "\n  }\n}\n";
    writeFile(p, o.str());
}

struct LockEntry {
    std::string ref;
    std::string path;
    std::string commit;
    std::string version;
    std::string source;
};

using LockMap = std::map<std::string, LockEntry>;

std::string defaultRef(const std::string& r) {
    return r.empty() ? "main" : r;
}

std::string gitHead(const fs::path& repo) {
#ifdef _WIN32
    std::string cmd = "git -C \"" + repo.string() + "\" rev-parse HEAD";
#else
    std::string cmd = "git -C '" + repo.string() + "' rev-parse HEAD";
#endif
    return runCapture(cmd);
}

void rebuildLockFromDisk(const DepMap& dm, const fs::path& kernRoot, const fs::path& pkgDir,
                         LockMap& locks) {
    locks.clear();
    std::error_code ec;
    for (const auto& kv : dm.deps) {
        fs::path dest = pkgDir / sanitizeKeyFolder(kv.first);
        if (!fs::is_directory(dest, ec)) continue;
        LockEntry le;
        le.ref = defaultRef(kv.second);
        le.path = fs::relative(dest, kernRoot, ec).generic_string();
        if (le.path.empty()) le.path = std::string("packages/") + sanitizeKeyFolder(kv.first);
        le.commit = gitHead(dest);
        if (le.commit.empty()) le.commit = "?";
        le.version = le.ref;
        le.source = std::string("github:") + kv.first;
        locks[kv.first] = le;
    }
}

void writeKargoLock(const fs::path& p, const LockMap& locks) {
    std::ostringstream o;
    o << "{\n  \"lockfile_version\": 2,\n  \"packages\": {\n";
    bool first = true;
    for (const auto& kv : locks) {
        if (!first) o << ",\n";
        first = false;
        o << "    \"" << jsonEscape(kv.first) << "\": {\n";
        o << "      \"ref\": \"" << jsonEscape(kv.second.ref) << "\",\n";
        o << "      \"path\": \"" << jsonEscape(kv.second.path) << "\",\n";
        o << "      \"commit\": \"" << jsonEscape(kv.second.commit) << "\",\n";
        o << "      \"version\": \"" << jsonEscape(kv.second.version.empty() ? kv.second.ref : kv.second.version)
          << "\",\n";
        o << "      \"source\": \"" << jsonEscape(kv.second.source.empty() ? std::string("github:") + kv.first
                                                  : kv.second.source)
          << "\"\n";
        o << "    }";
    }
    o << "\n  }\n}\n";
    writeFile(p, o.str());
}

static bool pathHasParentTraversal(const fs::path& p) {
    for (const auto& part : p) {
        if (part == "..") return true;
    }
    return false;
}

void writePackagePaths(const fs::path& cfgDir, const LockMap& locks, const fs::path& kernRoot) {
    std::ostringstream o;
    o << "{\n";
    bool first = true;
    for (const auto& kv : locks) {
        fs::path rel = fs::path(kv.second.path);
        fs::path abs = kernRoot / rel;
        std::error_code ec;
        fs::path mainKn = findMainFile(abs);
        if (mainKn.empty()) continue;
        std::error_code ecRel;
        fs::path mainRel = fs::relative(mainKn, kernRoot, ecRel);
        if (ecRel || mainRel.empty() || pathHasParentTraversal(mainRel)) continue;
        if (!first) o << ",\n";
        first = false;
        o << "  \"" << jsonEscape(kv.first) << "\": {\n";
        o << "    \"main\": \"" << jsonEscape(mainRel.generic_string()) << "\"\n";
        o << "  }";
    }
    o << "\n}\n";
    fs::path outPath = cfgDir / "package-paths.json";
    writeFile(outPath, o.str());
}

static std::string winPath(const fs::path& p) {
    std::string s = p.string();
#ifdef _WIN32
    for (char& c : s) {
        if (c == '/') c = '\\';
    }
#endif
    return s;
}

static int gitClone(const std::string& owner, const std::string& repo, const std::string& ref,
                    const fs::path& dest) {
    std::string url = "https://github.com/" + owner + "/" + repo + ".git";
    std::string d = winPath(dest);
#ifdef _WIN32
    std::string cmd = "git clone --depth 1 --branch \"" + ref + "\" \"" + url + "\" \"" + d + "\"";
#else
    std::string cmd = "git clone --depth 1 --branch '" + ref + "' '" + url + "' '" + dest.string() + "'";
#endif
    return runCmd(cmd);
}

static int downloadToFile(const std::string& url, const fs::path& out) {
#ifdef _WIN32
    std::string cmd =
        "curl -fsSL -L -o \"" + winPath(out) + "\" \"" + url + "\"";
#else
    std::string cmd = "curl -fsSL -L -o '" + out.string() + "' '" + url + "'";
#endif
    return runCmd(cmd);
}

static std::string regexEscapeRe(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '\\':
            case '^':
            case '$':
            case '.':
            case '|':
            case '?':
            case '*':
            case '+':
            case '(':
            case ')':
            case '[':
            case '{':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

/// Return locked `ref` for `key` in existing lockfile text, or empty if unknown.
static std::string lockedRefForKey(const std::string& lockRaw, const std::string& key) {
    if (lockRaw.empty()) return "";
    try {
        std::regex re("\"" + regexEscapeRe(key) + "\"\\s*:\\s*\\{[\\s\\S]*?\"ref\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch m;
        if (std::regex_search(lockRaw, m, re) && m.size() >= 2) return m[1].str();
    } catch (...) {}
    return "";
}

static bool safeZipEntryLeaf(const fs::path& name) {
    std::string s = name.string();
    if (s.empty() || s == "." || s == "..") return false;
    if (s.find('/') != std::string::npos || s.find('\\') != std::string::npos) return false;
    return true;
}

static bool isStrictSubpathOfDest(const fs::path& destRoot, const fs::path& candidate) {
    std::error_code ec;
    fs::path d = fs::weakly_canonical(destRoot, ec);
    if (ec) return false;
    fs::path c = fs::weakly_canonical(candidate, ec);
    if (ec) return false;
    auto cIt = c.begin();
    auto dIt = d.begin();
    while (dIt != d.end() && cIt != c.end()) {
        if (*cIt != *dIt) return false;
        ++cIt;
        ++dIt;
    }
    return dIt == d.end();
}

static bool skipZipRootEntry(const fs::path& filename) {
    std::string n = filename.string();
    if (n == "__MACOSX") return true;
    if (!n.empty() && n[0] == '.') return true;
    return false;
}

/// Extract GitHub `archive/*.zip` into `dest` with a single top-level folder flattened (no `repo-ref/` wrapper).
static int extractGithubZipArchive(const fs::path& zipPath, const fs::path& dest) {
    std::error_code ec;
    fs::path parent = dest.parent_path();
    fs::path tmp = parent / "_kargo_archive_unpack";
    if (fs::exists(tmp)) fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
#ifdef _WIN32
    std::string cmd = "tar -xf \"" + winPath(zipPath) + "\" -C \"" + winPath(tmp) + "\"";
#else
    std::string cmd = "tar -xf '" + zipPath.string() + "' -C '" + tmp.string() + "'";
#endif
    if (runCmd(cmd) != 0) {
#ifdef _WIN32
        std::string ps = "powershell -NoProfile -Command \"Expand-Archive -Path '" + winPath(zipPath) +
                          "' -DestinationPath '" + winPath(tmp) + "' -Force\"";
        if (runCmd(ps) != 0) return 1;
#else
        return 1;
#endif
    }
    std::vector<fs::directory_entry> kept;
    for (auto const& e : fs::directory_iterator(tmp, ec)) {
        if (skipZipRootEntry(e.path().filename())) continue;
        kept.push_back(e);
    }
    if (kept.empty()) {
        fs::remove_all(tmp, ec);
        return 1;
    }
    std::vector<fs::directory_entry> dirs;
    std::vector<fs::directory_entry> files;
    for (const auto& e : kept) {
        if (e.is_directory(ec)) dirs.push_back(e);
        else files.push_back(e);
    }
    if (fs::exists(dest)) fs::remove_all(dest, ec);
    fs::create_directories(dest, ec);

    auto moveIntoDest = [&](const fs::path& from) {
        if (!safeZipEntryLeaf(from.filename())) {
            std::cerr << "kargo: archive entry has an unsafe path name; aborting extraction.\n";
            return false;
        }
        std::error_code ecMove;
        fs::path to = dest / from.filename();
        if (!isStrictSubpathOfDest(dest, to)) {
            std::cerr << "kargo: archive would escape target directory; aborting extraction.\n";
            return false;
        }
        fs::rename(from, to, ecMove);
        return !ecMove;
    };

    if (dirs.size() == 1) {
        for (const auto& f : files) {
            if (!moveIntoDest(f.path())) {
                fs::remove_all(tmp, ec);
                return 1;
            }
        }
        fs::path inner = dirs[0].path();
        for (auto const& child : fs::directory_iterator(inner, ec)) {
            if (!moveIntoDest(child.path())) {
                fs::remove_all(tmp, ec);
                return 1;
            }
        }
        fs::remove_all(inner, ec);
    } else if (dirs.empty() && !files.empty()) {
        for (const auto& f : files) {
            if (!moveIntoDest(f.path())) {
                fs::remove_all(tmp, ec);
                return 1;
            }
        }
    } else {
        fs::remove_all(dest, ec);
        fs::remove_all(tmp, ec);
        return 1;
    }

    fs::remove_all(tmp, ec);
    fs::remove(zipPath, ec);
    return 0;
}

/// Clone via git, or download GitHub archive zip if git is unavailable.
int cloneOrFetch(const std::string& owner, const std::string& repo, const std::string& ref,
                 const fs::path& dest) {
    std::error_code ec;
    if (fs::exists(dest)) fs::remove_all(dest, ec);

    if (gitClone(owner, repo, ref, dest) == 0) return 0;

    std::cerr << "kargo: git clone failed; trying GitHub archive download (curl + zip)...\n";
    fs::path zipPath = dest.parent_path() / ("_kargo_dl_" + sanitizeKeyFolder(owner + "_" + repo) + ".zip");
    const std::string base = "https://github.com/" + owner + "/" + repo + "/archive/";
    const std::vector<std::string> urls = {
        base + "refs/heads/" + ref + ".zip",
        base + "refs/tags/" + ref + ".zip",
        base + ref + ".zip",
    };
    for (const auto& u : urls) {
        if (downloadToFile(u, zipPath) != 0) continue;
        if (extractGithubZipArchive(zipPath, dest) == 0) return 0;
        fs::remove(zipPath, ec);
    }
    std::cerr << "kargo: could not clone or download archive. Install git and/or curl, check ref name.\n";
    return 1;
}

void usage() {
    std::cerr
        << "kargo — Kern package manager\n"
        << "  kargo [--root <path>] install <owner/repo>[@ref]   add dependency; clone/fetch into <KernRoot>/packages\n"
        << "  kargo [--root <path>] remove <owner/repo>        remove dependency\n"
        << "  kargo [--root <path>] update                     reinstall from kargo.json\n"
        << "  kargo [--root <path>] list                       show locked packages\n"
        << "\nResolves Kern root via --root, KERN_HOME, exe directory, config/env.json, or cache (portable must "
           "include kern, kargo, lib/kern, runtime).\n"
        << "kargo.json and kargo.lock live in the current working directory; packages and config/package-paths.json use the Kern root.\n";
}

} // namespace

int main(int argc, char** argv) {
    kern::initKernEnvironmentFromArgv(argc, argv);
    if (argc < 2) {
        usage();
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "--version" || cmd == "-v") {
        std::cout << "kargo "
#ifdef KERN_VERSION
                  << KERN_VERSION
#else
                  << "0.0.0"
#endif
                  << "\n";
        return 0;
    }

    std::error_code ec;
    auto kernRootOpt = kern::tryResolveKernRoot();
    if (!kernRootOpt || !kern::isResolvedToolchainRoot(*kernRootOpt)) {
        std::cerr << "Failed to locate kern environment.\n\n"
                  << "Fix one of:\n"
                  << "  - Run inside a kern-<version>/ folder (or set KERN_HOME to it)\n"
                  << "  - Set KERN_HOME to your Kern install root\n"
                  << "  - Use: kargo --root <path> ...\n";
        return 1;
    }
    fs::path kernRoot = fs::weakly_canonical(*kernRootOpt, ec);
    fs::path projectRoot = fs::current_path(ec);
    fs::path kjson = projectRoot / "kargo.json";
    fs::path klock = projectRoot / "kargo.lock";
    fs::path pkgDir = kernRoot / "packages";
    fs::path cfgDir = kernRoot / "config";

    if (cmd == "list") {
        DepMap dm;
        std::string err;
        if (!fs::is_regular_file(kjson) || !loadKargoJson(kjson, dm, err)) {
            std::cout << "(no kargo.json dependencies)\n";
            return 0;
        }
        LockMap locks;
        rebuildLockFromDisk(dm, kernRoot, pkgDir, locks);
        if (locks.empty()) {
            std::cout << "(no packages installed under " << pkgDir.generic_string() << ")\n";
            return 0;
        }
        for (const auto& kv : locks) {
            std::cout << kv.first << " @ " << kv.second.ref << " (" << kv.second.commit << ")\n";
        }
        return 0;
    }

    if (cmd == "install") {
        if (argc < 3) {
            std::cerr << "kargo install: need owner/repo[@ref]\n";
            return 1;
        }
        std::string spec = argv[2];
        std::string owner, repo, ref;
        if (!parseOwnerRepo(spec, owner, repo, ref)) {
            std::cerr << "kargo install: expected owner/repo[@ref]\n";
            return 1;
        }
        ref = defaultRef(ref);

        DepMap dm;
        std::string err;
        if (fs::is_regular_file(kjson)) {
            if (!loadKargoJson(kjson, dm, err)) {
                std::cerr << err << "\n";
                return 1;
            }
        }
        std::string key = owner + "/" + repo;
        dm.deps[key] = ref;
        writeKargoJson(kjson, dm);

        fs::path dest = pkgDir / sanitizeKeyFolder(key);
        fs::create_directories(pkgDir, ec);
        std::string lockRaw = readFile(klock);
        std::string prevRef = lockedRefForKey(lockRaw, key);
        if (prevRef == ref && fs::is_directory(dest) && !findMainFile(dest).empty()) {
            std::cout << "pkg already installed (" << ref << ") — skipping\n";
            LockMap locks;
            rebuildLockFromDisk(dm, kernRoot, pkgDir, locks);
            writeKargoLock(klock, locks);
            fs::create_directories(cfgDir, ec);
            writePackagePaths(cfgDir, locks, kernRoot);
            return 0;
        }
        if (cloneOrFetch(owner, repo, ref, dest) != 0) return 1;
        LockMap locks;
        rebuildLockFromDisk(dm, kernRoot, pkgDir, locks);
        writeKargoLock(klock, locks);
        fs::create_directories(cfgDir, ec);
        writePackagePaths(cfgDir, locks, kernRoot);
        std::cout << "installed " << key << " → " << dest.generic_string() << "\n";
        return 0;
    }

    if (cmd == "remove") {
        if (argc < 3) {
            std::cerr << "kargo remove: need owner/repo\n";
            return 1;
        }
        std::string key = argv[2];
        DepMap dm;
        std::string err;
        if (!loadKargoJson(kjson, dm, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        fs::path dest = pkgDir / sanitizeKeyFolder(key);
        fs::remove_all(dest, ec);
        dm.deps.erase(key);
        writeKargoJson(kjson, dm);
        LockMap locks;
        rebuildLockFromDisk(dm, kernRoot, pkgDir, locks);
        writeKargoLock(klock, locks);
        writePackagePaths(cfgDir, locks, kernRoot);
        std::cout << "removed " << key << "\n";
        return 0;
    }

    if (cmd == "update") {
        DepMap dm;
        std::string err;
        if (!loadKargoJson(kjson, dm, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        for (const auto& kv : dm.deps) {
            std::string owner, repo, refExtra;
            if (!parseOwnerRepo(kv.first, owner, repo, refExtra)) {
                std::cerr << "skip invalid key: " << kv.first << "\n";
                continue;
            }
            std::string ref = defaultRef(kv.second);
            fs::path dest = pkgDir / sanitizeKeyFolder(kv.first);
            if (cloneOrFetch(owner, repo, ref, dest) != 0) return 1;
        }
        LockMap locks;
        rebuildLockFromDisk(dm, kernRoot, pkgDir, locks);
        writeKargoLock(klock, locks);
        fs::create_directories(cfgDir, ec);
        writePackagePaths(cfgDir, locks, kernRoot);
        std::cout << "updated " << locks.size() << " package(s)\n";
        return 0;
    }

    usage();
    return 1;
}
