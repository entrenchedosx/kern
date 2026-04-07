#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include "platform/win32_associate_kn.hpp"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace kern::win32 {
namespace {

constexpr wchar_t kProgId[] = L"KernFile";
constexpr wchar_t kSkipEnv[] = L"KERN_SKIP_FILE_ASSOCIATION";
constexpr wchar_t kRestartExplorerEnv[] = L"KERN_RESTART_EXPLORER_AFTER_ASSOC";

std::string narrowUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr, nullptr);
    return out;
}

bool envSkip() {
    wchar_t buf[32];
    DWORD got = GetEnvironmentVariableW(kSkipEnv, buf, static_cast<DWORD>(std::size(buf)));
    return got > 0 && got < static_cast<DWORD>(std::size(buf));
}

bool envRestartExplorer() {
    wchar_t buf[8];
    DWORD got = GetEnvironmentVariableW(kRestartExplorerEnv, buf, static_cast<DWORD>(std::size(buf)));
    return got > 0 && got < static_cast<DWORD>(std::size(buf));
}

fs::path appDataKernDir() {
    DWORD n = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    if (n == 0) return {};
    std::vector<wchar_t> buf(static_cast<size_t>(n));
    DWORD got = GetEnvironmentVariableW(L"APPDATA", buf.data(), n);
    if (got == 0 || got >= n) return {};
    return fs::path(buf.data()) / "kern";
}

fs::path setupFlagPath() { return appDataKernDir() / "setup_done.flag"; }
fs::path setupLogPath() { return appDataKernDir() / "setup.log"; }

void appendLog(std::string_view line) {
    try {
        fs::path dir = appDataKernDir();
        if (dir.empty()) return;
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (ec) return;
        std::ofstream f(setupLogPath(), std::ios::app | std::ios::binary);
        if (!f) return;
        SYSTEMTIME st{};
        GetLocalTime(&st);
        char ts[64];
        std::snprintf(ts, sizeof(ts), "%04u-%02u-%02u %02u:%02u:%02u ", static_cast<unsigned>(st.wYear),
            static_cast<unsigned>(st.wMonth), static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour),
            static_cast<unsigned>(st.wMinute), static_cast<unsigned>(st.wSecond));
        f << ts << line << '\n';
    } catch (...) {
        /* never propagate */
    }
}

bool fileExists(const fs::path& p) {
    std::error_code ec;
    return fs::is_regular_file(p, ec) && !ec;
}

fs::path modulePath() {
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size() - 1) return fs::path(std::wstring(buf.data(), n));
        buf.resize(buf.size() * 2);
    }
}

// Open command must invoke kern.exe (script runner), not kernc.exe / kern-impl.exe.
fs::path resolveKernExeForAssociation(const fs::path& selfPath) {
    fs::path dir = selfPath.parent_path();
    fs::path sibling = dir / "kern.exe";
    if (fileExists(sibling)) return fs::weakly_canonical(sibling);

    std::wstring name = selfPath.filename().wstring();
    if (_wcsicmp(name.c_str(), L"kern.exe") == 0) return fs::weakly_canonical(selfPath);

    appendLog("Association skipped: kern.exe not found beside " + narrowUtf8(selfPath.wstring()));
    return {};
}

std::wstring quoteForRegistry(const fs::path& p) {
    std::wstring w = p.wstring();
    return L"\"" + w + L"\"";
}

LSTATUS regSetSz(HKEY key, const wchar_t* valueName, const std::wstring& data) {
    const BYTE* bytes = reinterpret_cast<const BYTE*>(data.c_str());
    DWORD cb = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, valueName, 0, REG_SZ, bytes, cb);
}

bool createSetClose(HKEY root, const wchar_t* subKey, const wchar_t* valueName, const std::wstring& data, int retries) {
    HKEY h = nullptr;
    LSTATUS st = ERROR_ACCESS_DENIED;
    for (int attempt = 0; attempt < retries; ++attempt) {
        st = RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, nullptr, &h, nullptr);
        if (st == ERROR_SUCCESS) break;
        if (attempt + 1 < retries) Sleep(80);
    }
    if (st != ERROR_SUCCESS) {
        appendLog(std::string("RegCreateKeyEx failed: ") + std::to_string(static_cast<long>(st)) + " " +
                  narrowUtf8(subKey));
        return false;
    }
    st = regSetSz(h, valueName, data);
    RegCloseKey(h);
    if (st != ERROR_SUCCESS) {
        appendLog(std::string("RegSetValueEx failed: ") + std::to_string(static_cast<long>(st)) + " " +
                  narrowUtf8(subKey));
        return false;
    }
    return true;
}

bool applyAllRegistryValues(const fs::path& kernExe, const fs::path& iconPathOrExe, bool iconIsIco) {
    const std::wstring kernQ = quoteForRegistry(kernExe);
    const std::wstring openCmd = kernQ + L" \"%1\"";

    std::wstring iconVal = quoteForRegistry(iconPathOrExe);
    if (!iconIsIco) iconVal += L",0";

    const HKEY hk = HKEY_CURRENT_USER;
    bool ok = true;
    ok = ok && createSetClose(hk, L"Software\\Classes\\.kn", nullptr, kProgId, 2);
    ok = ok && createSetClose(hk, L"Software\\Classes\\.kn", L"Content Type", L"text/x-kern", 2);
    ok = ok && createSetClose(hk, L"Software\\Classes\\KernFile", nullptr, L"Kern Script", 2);
    ok = ok && createSetClose(hk, L"Software\\Classes\\KernFile", L"PerceivedType", L"text", 2);
    ok = ok && createSetClose(hk, L"Software\\Classes\\KernFile\\DefaultIcon", nullptr, iconVal, 2);
    ok = ok && createSetClose(hk, L"Software\\Classes\\KernFile\\shell\\open\\command", nullptr, openCmd, 2);
    ok = ok && createSetClose(hk, L"Software\\Classes\\KernFile\\shell\\EditWithKern", nullptr, L"Edit with Kern", 2);
    ok = ok &&
         createSetClose(hk, L"Software\\Classes\\KernFile\\shell\\EditWithKern\\command", nullptr, openCmd, 2);
    return ok;
}

void notifyExplorerShell() {
    // Primary: association cache refresh (no admin).
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    // Extra nudge so icons update on more builds without restarting Explorer.
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_FLUSH, nullptr, nullptr);
}

void maybeRestartExplorerAfterAssoc() {
    if (!envRestartExplorer()) return;
    appendLog("KERN_RESTART_EXPLORER_AFTER_ASSOC: restarting Explorer (desktop may blink)");
    // Portable-friendly: same pattern many dev tools document for stubborn icon cache.
    _wsystem(L"cmd.exe /C taskkill /F /IM explorer.exe >nul 2>&1");
    Sleep(1500);
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOW)) <= 32) {
        appendLog("ShellExecuteW(explorer.exe) after taskkill failed or returned <=32");
    }
}

bool writeSetupFlag() {
    std::error_code ec;
    fs::create_directories(appDataKernDir(), ec);
    if (ec) {
        appendLog(std::string("create_directories appdata kern failed: ") + ec.message());
        return false;
    }
    std::ofstream f(setupFlagPath(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f) {
        appendLog("Could not write setup_done.flag");
        return false;
    }
    f << "1\n";
    return true;
}

// Shared implementation: resolve paths, registry, notify, setup_done.flag on success.
// verbose: print status to console (repair command).
static void runAssociationPipeline(bool verbose) {
    fs::path self = modulePath();
    if (self.empty()) {
        appendLog("GetModuleFileNameW failed");
        if (verbose) std::cerr << "kern: could not get executable path.\n";
        return;
    }

    fs::path kernExe = resolveKernExeForAssociation(self);
    if (kernExe.empty()) {
        if (verbose) std::cerr << "kern: association failed (see %APPDATA%\\kern\\setup.log).\n";
        return;
    }

    fs::path ico = kernExe.parent_path() / "kern_logo.ico";
    bool useIco = fileExists(ico);
    fs::path iconTarget = useIco ? ico : kernExe;

    bool wrote = false;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (applyAllRegistryValues(kernExe, iconTarget, useIco)) {
            wrote = true;
            break;
        }
        if (attempt == 0) Sleep(120);
    }
    if (!wrote) {
        if (verbose) std::cerr << "kern: association repair failed (see %APPDATA%\\kern\\setup.log).\n";
        return;
    }

    notifyExplorerShell();
    maybeRestartExplorerAfterAssoc();

    if (!writeSetupFlag()) {
        if (verbose) std::cerr << "kern: association wrote registry but could not write setup_done.flag.\n";
        return;
    }

    if (verbose) {
        std::cout << "kern: re-applied per-user .kn file association (HKCU).\n";
        std::cout << "      runner: " << narrowUtf8(kernExe.wstring()) << "\n";
    }
}

} // namespace

void maybeRegisterKnFileAssociation() {
    if (envSkip()) return;

    try {
        if (fileExists(setupFlagPath())) return;
        runAssociationPipeline(false);
    } catch (const std::exception& e) {
        appendLog(std::string("exception: ") + e.what());
    } catch (...) {
        appendLog("unknown exception in maybeRegisterKnFileAssociation");
    }
}

void repairKnFileAssociation() {
    try {
        std::error_code ec;
        fs::remove(setupFlagPath(), ec);
        appendLog("repairKnFileAssociation: re-applying HKCU .kn association");
        runAssociationPipeline(true);
    } catch (const std::exception& e) {
        appendLog(std::string("repair exception: ") + e.what());
        std::cerr << "kern: repair failed: " << e.what() << "\n";
    } catch (...) {
        appendLog("unknown exception in repairKnFileAssociation");
        std::cerr << "kern: repair failed (unknown error).\n";
    }
}

} // namespace kern::win32

#endif // _WIN32
