/* *
 * updater.cpp — Self-updater for the `kern` CLI compiler binary.
 *
 * Implements cross-platform update logic using native OS APIs only:
 *   WinHTTP (Windows), pipes/curl (Linux/macOS).
 *
 * The high-level flow for `kern --update`:
 *   1. Resolve current executable path
 *   2. Query GitHub Releases API
 *   3. Parse JSON for latest tag + platform binary URL
 *   4. Compare versions; skip if up-to-date
 *   5. Download new binary to temporary file
 *   6. Hot-swap the running executable
 *   7. Clean up the old binary
 *
 * C++17, zero external dependencies beyond libc / Win32 / POSIX.
 */

#include "platform/updater.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// Platform detection and includes
// ============================================================================

#if defined(_WIN32)
#define KERN_OS_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#elif defined(__linux__)
#define KERN_OS_LINUX 1
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#elif defined(__APPLE__)
#define KERN_OS_MACOS 1
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/syslimits.h>
#include <mach-o/dyld.h>

#else
#error "Unsupported platform for self-updater"
#endif

// ============================================================================
// Constants
// ============================================================================

namespace {

// Repository owner/name for GitHub Releases API.
constexpr const char* kGitHubRepo = "entrenchedosx/kern";

// GitHub API URL for the latest release.
constexpr const char* kGitHubApiLatest =
    "https://api.github.com/repos/entrenchedosx/kern/releases/latest";

// User-Agent required by GitHub API.
constexpr const char* kGitHubUserAgent = "Kern-Update/1.0";

// Maximum size for a downloaded release JSON response (256 KB).
constexpr std::size_t kMaxResponseSize = 256 * 1024;

// Maximum size for a downloaded binary (256 MB).
constexpr std::size_t kMaxBinarySize = 256 * 1024 * 1024;

// Maximum path length (matching PATH_MAX on most systems).
constexpr std::size_t kPathMax = 4096;

// Temp file suffix for downloaded binary.
constexpr const char* kTempSuffix = ".kern_update.tmp";

// Backup suffix for the old binary on Windows.
constexpr const char* kOldSuffix = ".kern.old";

// Timeout for HTTP operations in milliseconds.
// DWORD is Windows-specific; use unsigned long for cross-platform compat.
#if defined(_WIN32)
constexpr DWORD kHttpTimeoutMs = 30000;
#else
constexpr unsigned long kHttpTimeoutMs = 30000;
#endif

} // anonymous namespace

// ============================================================================
// Internal helpers
// ============================================================================

namespace kern {

// ---------------------------------------------------------------------------
// Platform: Current executable path
// ---------------------------------------------------------------------------

std::string getCurrentExecutablePath() {
#if defined(KERN_OS_WINDOWS)
    wchar_t buf[kPathMax] = {};
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(kPathMax));
    if (n == 0 || n >= kPathMax) {
        throw std::runtime_error("GetModuleFileNameW failed");
    }
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        throw std::runtime_error("WideCharToMultiByte failed for exe path");
    }
    std::string result(static_cast<std::size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf, static_cast<int>(n),
                        result.data(), len, nullptr, nullptr);
    return result;

#elif defined(KERN_OS_LINUX)
    char buf[kPathMax] = {};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        throw std::runtime_error("readlink(/proc/self/exe) failed");
    }
    buf[n] = '\0';
    return std::string(buf);

#elif defined(KERN_OS_MACOS)
    uint32_t size = static_cast<uint32_t>(kPathMax);
    std::vector<char> buf(static_cast<std::size_t>(size));
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        // Buffer too small; size now contains the required length.
        buf.resize(static_cast<std::size_t>(size));
        if (_NSGetExecutablePath(buf.data(), &size) != 0) {
            throw std::runtime_error("_NSGetExecutablePath failed");
        }
    }
    // Resolve symlinks / relative path to absolute real path.
    char real[kPathMax] = {};
    if (realpath(buf.data(), real) == nullptr) {
        throw std::runtime_error("realpath failed for executable path");
    }
    return std::string(real);
#endif
}

// ---------------------------------------------------------------------------
// Platform: Binary name for the current architecture
// ---------------------------------------------------------------------------

std::string getPlatformBinaryName() {
    // The expected naming convention for GitHub release assets:
    //   kern-<arch>-<os>-<abi>[.exe]
    // Examples: kern-x86_64-pc-windows-msvc.exe
    //           kern-x86_64-unknown-linux-gnu
    //           kern-aarch64-apple-darwin
    //           kern-x86_64-apple-darwin

    std::string name = "kern-";

#if defined(KERN_OS_WINDOWS)
    // Architecture detection on Windows via GetNativeSystemInfo
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            name += "x86_64-pc-windows-msvc";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            name += "aarch64-pc-windows-msvc";
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            name += "i686-pc-windows-msvc";
            break;
        default:
            name += "x86_64-pc-windows-msvc"; // safest fallback
            break;
    }
    name += ".exe";

#elif defined(KERN_OS_LINUX)
    // Architecture detection on Linux via uname
    struct utsname u {};
    if (uname(&u) == 0) {
        if (std::strcmp(u.machine, "x86_64") == 0) {
            name += "x86_64-unknown-linux-gnu";
        } else if (std::strcmp(u.machine, "aarch64") == 0) {
            name += "aarch64-unknown-linux-gnu";
        } else if (std::strcmp(u.machine, "armv7l") == 0) {
            name += "armv7-unknown-linux-gnueabihf";
        } else {
            name += u.machine;
            name += "-unknown-linux-gnu";
        }
    } else {
        name += "x86_64-unknown-linux-gnu";
    }

#elif defined(KERN_OS_MACOS)
    // Architecture detection on macOS via uname or sysctl
#if defined(__aarch64__)
    name += "aarch64-apple-darwin";
#elif defined(__x86_64__)
    name += "x86_64-apple-darwin";
#else
    // Runtime detection fallback
    struct utsname u {};
    if (uname(&u) == 0) {
        if (std::strcmp(u.machine, "arm64") == 0) {
            name += "aarch64-apple-darwin";
        } else {
            name += "x86_64-apple-darwin";
        }
    } else {
        name += "x86_64-apple-darwin";
    }
#endif
#endif

    return name;
}

// ---------------------------------------------------------------------------
// GitHub API URL builder
// ---------------------------------------------------------------------------

std::string buildGitHubApiUrl() {
    return std::string(kGitHubApiLatest);
}

// ---------------------------------------------------------------------------
// Minimal JSON field extractor (no external dependency)
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Find the value of a top-level string field in a JSON response.
 *
 * Scans @p json for  `"fieldName": "value"`  (or  "fieldName": "value")
 * and extracts the unescaped string value. Only handles ASCII printable
 * characters and standard JSON escapes (\\, \/, \", \n, \r, \t).
 *
 * @returns true and writes the extracted value to @p out if found.
 */
bool extractJsonString(const std::string& json,
                       const std::string& fieldName,
                       std::string& out) {
    // Search for `"fieldName":` or `"fieldName" :`
    std::string pattern = "\"" + fieldName + "\"";
    std::size_t pos = 0;
    while (true) {
        std::size_t namePos = json.find(pattern, pos);
        if (namePos == std::string::npos) return false;

        // Skip past the field name
        std::size_t colonPos = namePos + pattern.size();
        // Skip whitespace
        while (colonPos < json.size() &&
               (json[colonPos] == ' ' || json[colonPos] == '\t' ||
                json[colonPos] == '\r' || json[colonPos] == '\n')) {
            ++colonPos;
        }
        if (colonPos >= json.size()) return false;
        if (json[colonPos] != ':') {
            pos = colonPos;
            continue; // not the field we're looking for
        }
        ++colonPos; // skip ':'
        // Skip whitespace
        while (colonPos < json.size() &&
               (json[colonPos] == ' ' || json[colonPos] == '\t' ||
                json[colonPos] == '\r' || json[colonPos] == '\n')) {
            ++colonPos;
        }
        if (colonPos >= json.size()) return false;
        if (json[colonPos] != '"') {
            pos = colonPos;
            continue; // not a string value
        }
        ++colonPos; // skip opening '"'

        // Extract the string value handling escapes
        std::string value;
        bool escaped = false;
        for (std::size_t i = colonPos; i < json.size(); ++i) {
            char c = json[i];
            if (escaped) {
                switch (c) {
                    case '"':  value.push_back('"');  break;
                    case '\\': value.push_back('\\'); break;
                    case '/':  value.push_back('/');  break;
                    case 'n':  value.push_back('\n'); break;
                    case 'r':  value.push_back('\r'); break;
                    case 't':  value.push_back('\t'); break;
                    case 'u': {
                        // Simple \\uXXXX - we just store a placeholder
                        value.push_back('?');
                        i += 4; // skip 4 hex digits
                        break;
                    }
                    default:
                        value.push_back(c);
                        break;
                }
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                out = value;
                return true;
            } else {
                value.push_back(c);
            }
        }
        // Reached end of string without closing quote; malformed JSON
        return false;
    }
}

/**
 * @brief Search within a JSON array for an object containing a specific
 *        name/value pair, then extract a field from that object.
 *
 * Scans the JSON response for an array element like:
 *   { ..., "name": "<binaryName>", ..., "browser_download_url": "<url>", ... }
 *
 * @returns true if the asset was found and @p urlOut was populated.
 */
bool findAssetUrl(const std::string& json,
                  const std::string& binaryName,
                  std::string& urlOut) {
    // Locate the "assets" array
    static constexpr const char kAssetsKey[] = "\"assets\"";
    std::size_t assetsPos = json.find(kAssetsKey);
    if (assetsPos == std::string::npos) return false;

    // Find the opening '[' of the assets array
    std::size_t bracket = assetsPos + sizeof(kAssetsKey) - 1;
    while (bracket < json.size() && json[bracket] != '[') {
        if (json[bracket] == ':') { ++bracket; break; }
        ++bracket;
    }
    while (bracket < json.size() && json[bracket] != '[') ++bracket;
    if (bracket >= json.size()) return false;

    // Scan each object in the array for "name": "binaryName"
    std::size_t searchStart = bracket + 1;
    while (true) {
        std::size_t objStart = json.find('{', searchStart);
        if (objStart == std::string::npos || objStart >= json.size()) break;

        // Find the closing '}' of this object (handle nesting simply)
        int depth = 0;
        std::size_t objEnd = objStart;
        for (; objEnd < json.size(); ++objEnd) {
            if (json[objEnd] == '{') ++depth;
            else if (json[objEnd] == '}') { --depth; if (depth == 0) break; }
        }
        if (objEnd >= json.size()) break;

        std::string obj = json.substr(objStart, objEnd - objStart + 1);

        // Check if this object has "name": "binaryName"
        std::string assetName;
        if (extractJsonString(obj, "name", assetName) && assetName == binaryName) {
            // Extract the download URL from this object
            return extractJsonString(obj, "browser_download_url", urlOut);
        }

        searchStart = objEnd + 1;
    }

    return false;
}

/**
 * @brief Simple semver-like version comparison.
 *        Strips optional leading 'v' or 'V' prefix.
 *        Compares dot-separated numeric segments.
 *
 * @returns -1 if a < b, 0 if a == b, 1 if a > b.
 */
int compareVersions(const std::string& a, const std::string& b) {
    auto stripPrefix = [](std::string s) -> std::string {
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
            s.erase(0, 1);
        }
        return s;
    };

    std::string va = stripPrefix(a);
    std::string vb = stripPrefix(b);

    auto nextSeg = [](const std::string& s, std::size_t& pos) -> int {
        if (pos >= s.size()) return 0;
        std::size_t dot = s.find('.', pos);
        std::string seg;
        if (dot == std::string::npos) {
            seg = s.substr(pos);
            pos = s.size();
        } else {
            seg = s.substr(pos, dot - pos);
            pos = dot + 1;
        }
        // Handle non-numeric segments gracefully
        char* end = nullptr;
        long val = std::strtol(seg.c_str(), &end, 10);
        if (end == seg.c_str() || seg.empty()) return 0;
        return static_cast<int>(val);
    };

    std::size_t pa = 0, pb = 0;
    while (pa < va.size() || pb < vb.size()) {
        int sa = nextSeg(va, pa);
        int sb = nextSeg(vb, pb);
        if (sa < sb) return -1;
        if (sa > sb) return 1;
    }
    return 0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// HTTP download (platform-specific)
// ---------------------------------------------------------------------------

bool downloadToFile(const std::string& url, const std::string& destPath,
                    std::string& errorMsg) {
#if defined(KERN_OS_WINDOWS)
    // ---- Windows: WinHTTP ----
    // Parse URL
    bool https = false;
    std::string host, object;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;

    if (url.find("https://") == 0) {
        https = true;
        std::size_t start = 8;
        std::size_t slash = url.find('/', start);
        host = url.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        object = (slash == std::string::npos) ? "/" : url.substr(slash);
    } else {
        errorMsg = "Unsupported URL scheme (only https://)";
        return false;
    }

    // Convert to wide strings
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                    static_cast<int>(s.size()),
                                    nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                            static_cast<int>(s.size()),
                            w.data(), n);
        return w;
    };

    std::wstring whost = toWide(host);
    std::wstring wobj = toWide(object);
    if (whost.empty() || wobj.empty()) {
        errorMsg = "Host/path conversion to wide string failed";
        return false;
    }

    // Open session
    HINTERNET hSession = WinHttpOpen(
        L"Kern-Update/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        errorMsg = "WinHttpOpen failed";
        return false;
    }

    // Set timeouts
    WinHttpSetTimeouts(hSession, kHttpTimeoutMs, kHttpTimeoutMs,
                       kHttpTimeoutMs, kHttpTimeoutMs);

    // Enable TLS 1.2+
    DWORD secureProto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
    secureProto |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
                     &secureProto, sizeof(secureProto));

    // Connect
    HINTERNET hConn = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConn) {
        errorMsg = "WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Open request
    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(
        hConn, L"GET", wobj.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        errorMsg = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set User-Agent header
    std::wstring userAgent = toWide(std::string(kGitHubUserAgent));
    if (!userAgent.empty()) {
        WinHttpSetOption(hReq, WINHTTP_OPTION_USER_AGENT,
                         const_cast<wchar_t*>(userAgent.c_str()),
                         static_cast<DWORD>(userAgent.size() * sizeof(wchar_t)));
    }

    // Send request
    static const wchar_t kHeaders[] =
        L"Accept: application/octet-stream\r\n"
        L"Accept-Encoding: identity\r\n";

    if (!WinHttpSendRequest(hReq, kHeaders, static_cast<DWORD>(-1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        errorMsg = "WinHttpSendRequest failed";
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        errorMsg = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Check HTTP status
    DWORD status = 0;
    DWORD sz = sizeof(status);
    if (WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX)) {
        if (status < 200 || status >= 300) {
            errorMsg = "HTTP " + std::to_string(status);
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }

    // Open output file
    std::ofstream outFile(destPath, std::ios::binary);
    if (!outFile) {
        errorMsg = "Cannot open output file: " + destPath;
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read data in chunks
    std::uint64_t totalRead = 0;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;

        std::vector<char> buf(static_cast<std::size_t>(avail));
        DWORD read = 0;
        if (!WinHttpReadData(hReq, buf.data(), avail, &read)) break;
        if (read == 0) break;

        outFile.write(buf.data(), static_cast<std::streamsize>(read));
        totalRead += static_cast<std::uint64_t>(read);

        if (totalRead > kMaxBinarySize) {
            errorMsg = "Download exceeds maximum binary size (256 MB)";
            outFile.close();
            std::remove(destPath.c_str());
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }

    outFile.close();
    if (!outFile) {
        errorMsg = "Failed to write output file";
        std::remove(destPath.c_str());
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
    return true;

#elif defined(KERN_OS_LINUX) || defined(KERN_OS_MACOS)
    // ---- Unix: pipe to curl ----
    // Build curl command:
    //   curl -sL -o <dest> --connect-timeout 30 --max-time 300 <url>
    // We capture stderr to detect errors.
    std::string cmd = "curl -sL -o \"";
    cmd += destPath;
    cmd += "\" --connect-timeout 30 --max-time 300 \"";
    cmd += url;
    cmd += "\" 2>/dev/null; echo EXITCODE:$?";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        errorMsg = "Failed to launch curl (popen)";
        return false;
    }

    // Read the last line to get the exit code
    std::string output;
    char buf[128];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        output += buf;
    }
    int exitCode = pclose(pipe);

    if (exitCode != 0) {
        errorMsg = "curl exited with code " + std::to_string(exitCode);
        std::remove(destPath.c_str());
        return false;
    }

    // Check that the file was actually created and has content
    std::ifstream checkFile(destPath, std::ios::binary | std::ios::ate);
    if (!checkFile) {
        errorMsg = "Download produced no output file";
        return false;
    }
    std::streamsize fileSize = checkFile.tellg();
    checkFile.close();
    if (fileSize == 0) {
        errorMsg = "Downloaded file is empty";
        std::remove(destPath.c_str());
        return false;
    }

    return true;
#endif
}

// ---------------------------------------------------------------------------
// GitHub API query and response parsing
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Download the GitHub API response and parse it.
 *
 * @param[out] tagName     The latest release tag (e.g. "v2.3.0").
 * @param[out] downloadUrl The browser_download_url for the platform binary.
 * @param[out] errorMsg    Human-readable error description.
 * @returns true on success.
 */
bool queryGitHubRelease(std::string& tagName,
                        std::string& downloadUrl,
                        std::string& errorMsg) {
    std::string apiUrl = buildGitHubApiUrl();

#if defined(KERN_OS_WINDOWS)
    // ---- Windows: WinHTTP for API JSON ----
    bool https = false;
    std::string host, object;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;

    if (apiUrl.find("https://") == 0) {
        https = true;
        std::size_t start = 8;
        std::size_t slash = apiUrl.find('/', start);
        host = apiUrl.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        object = (slash == std::string::npos) ? "/" : apiUrl.substr(slash);
    } else {
        errorMsg = "Unsupported API URL scheme";
        return false;
    }

    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                    static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                            static_cast<int>(s.size()), w.data(), n);
        return w;
    };

    std::wstring whost = toWide(host);
    std::wstring wobj = toWide(object);
    if (whost.empty() || wobj.empty()) {
        errorMsg = "Failed to convert API URL to wide string";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(
        L"Kern-Update/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        errorMsg = "WinHttpOpen failed for API call";
        return false;
    }

    WinHttpSetTimeouts(hSession, kHttpTimeoutMs, kHttpTimeoutMs,
                       kHttpTimeoutMs, kHttpTimeoutMs);

    DWORD secureProto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
    secureProto |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS,
                     &secureProto, sizeof(secureProto));

    HINTERNET hConn = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConn) {
        errorMsg = "WinHttpConnect failed for API call";
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(
        hConn, L"GET", wobj.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        errorMsg = "WinHttpOpenRequest failed for API call";
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set GitHub-required User-Agent header
    std::wstring wUserAgent = toWide(std::string(kGitHubUserAgent));
    if (!wUserAgent.empty()) {
        WinHttpSetOption(hReq, WINHTTP_OPTION_USER_AGENT,
                         const_cast<wchar_t*>(wUserAgent.c_str()),
                         static_cast<DWORD>(wUserAgent.size() * sizeof(wchar_t)));
    }

    // Custom headers: GitHub API requires a proper User-Agent
    static const wchar_t kApiHeaders[] =
        L"Accept: application/vnd.github+json\r\n"
        L"User-Agent: " L"Kern-Update/1.0" L"\r\n";

    if (!WinHttpSendRequest(hReq, kApiHeaders, static_cast<DWORD>(-1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        errorMsg = "WinHttpSendRequest failed for API call";
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        errorMsg = "WinHttpReceiveResponse failed for API call";
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Check HTTP status
    DWORD status = 0;
    DWORD sz = sizeof(status);
    if (WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &sz, WINHTTP_NO_HEADER_INDEX)) {
        if (status < 200 || status >= 300) {
            errorMsg = "GitHub API returned HTTP " + std::to_string(status);
            if (status == 403) {
                errorMsg += " (rate limited?)";
            } else if (status == 404) {
                errorMsg += " (repo/ release not found?)";
            }
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }

    // Read the response into a string
    std::string jsonResponse;
    std::uint64_t totalRead = 0;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;

        std::vector<char> buf(static_cast<std::size_t>(avail));
        DWORD read = 0;
        if (!WinHttpReadData(hReq, buf.data(), avail, &read)) break;
        if (read == 0) break;

        jsonResponse.append(buf.data(), read);
        totalRead += static_cast<std::uint64_t>(read);

        if (totalRead > kMaxResponseSize) {
            errorMsg = "API response exceeds maximum size";
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSession);
            return false;
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);

    if (jsonResponse.empty()) {
        errorMsg = "GitHub API returned empty response";
        return false;
    }

#else
    // ---- Unix: pipe to curl for the API JSON ----
    std::string curlCmd = "curl -sL -H \"Accept: application/vnd.github+json\"";
    curlCmd += " -H \"User-Agent: " + std::string(kGitHubUserAgent) + "\"";
    curlCmd += " --connect-timeout 30 --max-time 60 \"";
    curlCmd += apiUrl + "\"";

    // On Unix we read the API response via pipe and parse in memory
    FILE* pipe = popen(curlCmd.c_str(), "r");
    if (!pipe) {
        errorMsg = "Failed to launch curl for API call";
        return false;
    }

    std::string jsonResponse;
    char buf[4096];
    std::size_t totalRead = 0;
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        std::size_t n = std::strlen(buf);
        jsonResponse.append(buf, n);
        totalRead += n;
        if (totalRead > kMaxResponseSize) {
            errorMsg = "API response exceeds maximum size";
            pclose(pipe);
            return false;
        }
    }
    int exitCode = pclose(pipe);
    if (exitCode != 0) {
        errorMsg = "curl for API call exited with code " +
                   std::to_string(exitCode);
        return false;
    }

    if (jsonResponse.empty()) {
        errorMsg = "GitHub API returned empty response";
        return false;
    }
#endif

    // Parse the JSON response for tag_name
    std::string latestTag;
    if (!extractJsonString(jsonResponse, "tag_name", latestTag)) {
        errorMsg = "Failed to parse 'tag_name' from GitHub API response";
        return false;
    }

    // Find the platform-specific binary asset
    std::string binaryName = getPlatformBinaryName();
    std::string assetUrl;
    if (!findAssetUrl(jsonResponse, binaryName, assetUrl)) {
        errorMsg = "No binary found for platform: " + binaryName;
        return false;
    }

    tagName = latestTag;
    downloadUrl = assetUrl;
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Hot-swap: replace running executable
// ---------------------------------------------------------------------------

bool applyBinarySwap(const std::string& downloadedPath,
                     const std::string& targetPath,
                     std::string& errorMsg) {
#if defined(KERN_OS_WINDOWS)
    // ---- Windows: rename trick with DETACHED_PROCESS cleanup ----
    //
    // Strategy:
    //   1. Rename the running exe to targetPath + ".kern.old" (Windows allows
    //      renaming the directory entry of a running executable).
    //   2. Move the downloaded binary into targetPath.
    //   3. Spawn a DETACHED_PROCESS that waits a few seconds and deletes the
    //      .old file. This runs independently of our process lifetime.

    std::string oldPath = targetPath + kOldSuffix;

    // Convert paths to wide strings for MoveFileExW
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                    static_cast<int>(s.size()), nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                            static_cast<int>(s.size()), w.data(), n);
        return w;
    };

    std::wstring wTarget = toWide(targetPath);
    std::wstring wOld = toWide(oldPath);
    std::wstring wDownloaded = toWide(downloadedPath);

    if (wTarget.empty() || wOld.empty() || wDownloaded.empty()) {
        errorMsg = "Path conversion to wide string failed";
        return false;
    }

    // Step 1: Rename existing exe → .kern.old
    // MOVEFILE_REPLACE_EXISTING allows overwriting a previous .old file.
    // MOVEFILE_WRITE_THROUGH ensures durability.
    if (!MoveFileExW(wTarget.c_str(), wOld.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD err = GetLastError();
        errorMsg = "Failed to rename running executable (error " +
                   std::to_string(err) + ")";
        return false;
    }

    // Step 2: Move downloaded file into the original exe path
    if (!MoveFileExW(wDownloaded.c_str(), wTarget.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD err = GetLastError();
        errorMsg = "Failed to stage new executable (error " +
                   std::to_string(err) + ")";

        // Attempt to restore the original binary
        MoveFileExW(wOld.c_str(), wTarget.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        return false;
    }

    // Step 3: Schedule old binary for deletion via DETACHED_PROCESS
    // Use cmd.exe /C with a timeout before deleting.
    // The command: cmd.exe /C choice /T 3 /D Y > nul & del "<oldPath>"
    // choice /T 3 /D Y waits 3 seconds and auto-chooses Yes.
    std::string cleanupCmd =
        std::string("cmd.exe /C choice /T 3 /D Y > nul & del \"") +
        oldPath + "\"";
    std::wstring wCleanup = toWide(cleanupCmd);
    if (wCleanup.empty()) {
        // Non-fatal: old binary will remain on disk but won't affect operation
        return true;
    }

    // Mutable copy for CreateProcessW
    std::vector<wchar_t> mutableCmd(wCleanup.begin(), wCleanup.end());
    mutableCmd.push_back(L'\0');

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    // DETACHED_PROCESS: no console window, independent lifetime
    BOOL created = CreateProcessW(
        nullptr,            // no module name (use command line)
        mutableCmd.data(),  // command line
        nullptr,            // process attributes
        nullptr,            // thread attributes
        FALSE,              // inherit handles
        DETACHED_PROCESS | CREATE_NO_WINDOW,
        nullptr,            // environment
        nullptr,            // current directory
        &si,
        &pi);

    if (created) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    // If creation fails, the .old file stays behind — harmless.

    return true;

#elif defined(KERN_OS_LINUX) || defined(KERN_OS_MACOS)
    // ---- Unix: unlink + rename + chmod ----
    //
    // On Linux/macOS, a running executable's inode remains accessible even
    // after unlink. So we can safely:
    //   1. unlink(targetPath) — removes the directory entry
    //   2. rename(downloadedPath → targetPath) — atomically puts new binary
    //   3. chmod(targetPath, 0755) — ensure executable permission

    // Step 1: Remove the existing binary's directory entry
    if (unlink(targetPath.c_str()) != 0 && errno != ENOENT) {
        errorMsg = "Failed to unlink existing binary: " +
                   std::string(std::strerror(errno));
        return false;
    }

    // Step 2: Move the downloaded binary into place
    if (rename(downloadedPath.c_str(), targetPath.c_str()) != 0) {
        errorMsg = "Failed to rename downloaded binary: " +
                   std::string(std::strerror(errno));
        return false;
    }

    // Step 3: Set executable permissions
    if (chmod(targetPath.c_str(), 0755) != 0) {
        // Non-fatal: file is already in place, permissions may still work
        // but warn via error message
        errorMsg = "Warning: chmod 0755 failed: " +
                   std::string(std::strerror(errno));
        return true; // still a success, just log the warning
    }

    return true;
#endif
}

// ============================================================================
// Public API: handleUpdate
// ============================================================================

void handleUpdate(const std::string& currentVersion) {
    std::cout << "\n";
    std::cout << "  ╔══════════════════════════════════════════╗\n";
    std::cout << "  ║     Kern Self-Updater                    ║\n";
    std::cout << "  ╚══════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "  Current version: " << currentVersion << "\n";

    // ---------------------------------------------------------------
    // Step 1: Resolve executable path
    // ---------------------------------------------------------------
    std::string exePath;
    try {
        exePath = getCurrentExecutablePath();
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Error: " << e.what() << "\n";
        std::cerr << "  Update aborted.\n";
        return;
    }
    std::cout << "  Executable: " << exePath << "\n";

    // ---------------------------------------------------------------
    // Step 2: Query GitHub Releases API
    // ---------------------------------------------------------------
    std::cout << "  Checking for updates...\n";

    std::string latestTag;
    std::string downloadUrl;
    std::string errorMsg;

    bool apiOk = queryGitHubRelease(latestTag, downloadUrl, errorMsg);
    if (!apiOk) {
        std::cerr << "  ✗ Failed to query GitHub releases:\n";
        std::cerr << "    " << errorMsg << "\n";
        std::cerr << "  Update aborted.\n";
        return;
    }

    std::cout << "  Latest release: " << latestTag << "\n";

    // ---------------------------------------------------------------
    // Step 3: Compare versions
    // ---------------------------------------------------------------
    int cmp = compareVersions(currentVersion, latestTag);
    if (cmp >= 0) {
        std::cout << "  ✓ You are already running the latest version.\n";
        std::cout << "\n";
        return;
    }

    std::cout << "  ↻ New version available: " << latestTag << "\n";
    std::cout << "  Downloading...\n";

    // ---------------------------------------------------------------
    // Step 4: Create temporary file for download
    // ---------------------------------------------------------------
    std::string tempFilePath;
#if defined(KERN_OS_WINDOWS)
    char tempDir[MAX_PATH + 1] = {};
    DWORD envLen = GetEnvironmentVariableA("TEMP", tempDir, MAX_PATH);
    if (envLen == 0 || envLen > MAX_PATH) {
        tempFilePath = exePath + kTempSuffix;
    } else {
        tempFilePath = std::string(tempDir) + "\\kern_update_binary" + kTempSuffix;
    }
#else
    const char* tmpdir = std::getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    tempFilePath = std::string(tmpdir) + "/kern_update_binary" + kTempSuffix;
#endif

    // ---------------------------------------------------------------
    // Step 5: Download the new binary
    // ---------------------------------------------------------------
    if (!downloadToFile(downloadUrl, tempFilePath, errorMsg)) {
        std::cerr << "  ✗ Download failed:\n";
        std::cerr << "    " << errorMsg << "\n";
        std::cerr << "  Update aborted.\n";
        return;
    }

    // Quick sanity check: new binary should be at least 1 KB
    std::ifstream sanityCheck(tempFilePath, std::ios::binary | std::ios::ate);
    if (!sanityCheck) {
        std::cerr << "  ✗ Downloaded file is unreadable\n";
        std::remove(tempFilePath.c_str());
        std::cerr << "  Update aborted.\n";
        return;
    }
    auto fileSize = sanityCheck.tellg();
    sanityCheck.close();

    if (fileSize < 1024) {
        std::cerr << "  ✗ Downloaded file is too small (" << fileSize
                  << " bytes), possibly invalid\n";
        std::remove(tempFilePath.c_str());
        std::cerr << "  Update aborted.\n";
        return;
    }

    std::cout << "  ✓ Downloaded " << (fileSize / 1024) << " KB\n";

    // ---------------------------------------------------------------
    // Step 6: Hot-swap the binary
    // ---------------------------------------------------------------
    std::cout << "  Applying update...\n";

    if (!applyBinarySwap(tempFilePath, exePath, errorMsg)) {
        std::cerr << "  ✗ Failed to apply update:\n";
        std::cerr << "    " << errorMsg << "\n";
        std::remove(tempFilePath.c_str());
        std::cerr << "  Update aborted.\n";
        return;
    }

    std::cout << "  ✓ Update applied successfully!\n";
    std::cout << "\n";
    std::cout << "  The new version (" << latestTag
              << ") will be used next time you run kern.\n";
    std::cout << "  Restart any running sessions to pick up the update.\n";
    std::cout << "\n";
}

} // namespace kern
