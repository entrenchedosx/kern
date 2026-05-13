/* *
 * windows-native HTTP(S) GET for http_get() â€” WinHTTP + WinINet (no curl/powershell).
 * winINet uses the same URL-moniker path as many desktop apps and often works when WinHTTP does not.
 */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winhttp.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
// Note: wininet.lib disabled due to MinGW header conflicts

namespace kern {

namespace {

void trimAsciiWsAndBom(std::string& u) {
    while (!u.empty() && static_cast<unsigned char>(u.front()) <= 32) u.erase(u.begin());
    while (!u.empty() && static_cast<unsigned char>(u.back()) <= 32) u.pop_back();
    if (u.size() >= 3 && static_cast<unsigned char>(u[0]) == 0xEF && static_cast<unsigned char>(u[1]) == 0xBB &&
        static_cast<unsigned char>(u[2]) == 0xBF)
        u.erase(0, 3);
    while (!u.empty() && static_cast<unsigned char>(u.front()) <= 32) u.erase(u.begin());
}

bool asciiPrefixIeq(const std::string& s, size_t pos, const char* lit, size_t n) {
    if (s.size() < pos + n) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = s[pos + i];
        char b = lit[i];
        if (std::tolower(static_cast<unsigned char>(a)) != std::tolower(static_cast<unsigned char>(b))) return false;
    }
    return true;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0)
        n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

bool parseHttpUrl(const std::string& url, bool& https, std::string& host, INTERNET_PORT& port, std::string& object) {
    https = false;
    size_t pos = 0;
    if (asciiPrefixIeq(url, 0, "https://", 8)) {
        https = true;
        pos = 8;
    } else if (asciiPrefixIeq(url, 0, "http://", 7)) {
        https = false;
        pos = 7;
    } else {
        return false;
    }

    size_t slash = url.find('/', pos);
    std::string hostPort = (slash == std::string::npos) ? url.substr(pos) : url.substr(pos, slash - pos);
    object = (slash == std::string::npos) ? "/" : url.substr(slash);
    if (object.empty()) object = "/";

    port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

    if (!hostPort.empty() && hostPort[0] == '[') {
        size_t close = hostPort.find(']');
        if (close == std::string::npos) return false;
        host = hostPort.substr(1, close - 1);
        if (close + 1 < hostPort.size() && hostPort[close + 1] == ':') {
            const std::string ps = hostPort.substr(close + 2);
            int p = std::atoi(ps.c_str());
            if (p > 0 && p < 65536) port = static_cast<INTERNET_PORT>(p);
        }
        return !host.empty();
    }

    size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos && colon > 0) {
        const std::string maybePort = hostPort.substr(colon + 1);
        bool allDigits = !maybePort.empty();
        for (char c : maybePort) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            host = hostPort.substr(0, colon);
            int p = std::atoi(maybePort.c_str());
            if (p > 0 && p < 65536) port = static_cast<INTERNET_PORT>(p);
            return !host.empty();
        }
    }

    host = hostPort;
    return !host.empty();
}

void winHttpGetTry(const std::string& url, bool ignoreCertErrors, std::string& bodyOut) {
    bodyOut.clear();
    bool https = false;
    std::string host, object;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    if (!parseHttpUrl(url, https, host, port, object)) return;

    const std::wstring whost = utf8ToWide(host);
    const std::wstring wobj = utf8ToWide(object);
    if (whost.empty() || wobj.empty()) return;

    HINTERNET hSession =
        WinHttpOpen(L"Kern/1.0 (WinHTTP)", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return;

    const DWORD ms = 60000;
    WinHttpSetTimeouts(hSession, ms, ms, ms, ms);

    DWORD secureProto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#if defined(WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3)
    secureProto |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(hSession, 9 /* WINHTTP_OPTION_SECURE_PROTOCOL*/, &secureProto, sizeof(secureProto));

    HINTERNET hConn = WinHttpConnect(hSession, whost.c_str(), port, 0);
    if (!hConn) {
        WinHttpCloseHandle(hSession);
        return;
    }

    const DWORD openFlags = https ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", wobj.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openFlags);
    if (!hReq) {
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return;
    }

    if (ignoreCertErrors) {
        DWORD dwFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                        SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
    }

    static const wchar_t kHdr[] =
        L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) Kern/1.0\r\n"
        L"Accept: */* \r\n"
        L"Accept-Encoding: identity\r\n";

    if (!WinHttpSendRequest(hReq, kHdr, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return;
    }

    if (!WinHttpReceiveResponse(hReq, nullptr)) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hSession);
        return;
    }

    DWORD status = 0;
    DWORD sz = sizeof(status);
    if (WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
            WINHTTP_NO_HEADER_INDEX)) {
        if (status < 200 || status >= 300) {
            WinHttpCloseHandle(hReq);
            WinHttpCloseHandle(hConn);
            WinHttpCloseHandle(hSession);
            return;
        }
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;
        std::vector<char> buf(static_cast<size_t>(avail));
        DWORD read = 0;
        if (!WinHttpReadData(hReq, buf.data(), avail, &read)) break;
        if (read == 0) break;
        bodyOut.append(buf.data(), read);
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
}

// WinINet functions stubbed out due to MinGW header conflicts
// TODO: Re-enable when WinINet header conflicts are resolved
void readAllInternet(void* hUrl, std::string& out) {
    // WinINet stub - not implemented
    out.clear();
}

void winInetGetTry(const std::string& url, bool ignoreCertErrors, std::string& bodyOut) {
    // WinINet stub - not implemented due to header conflicts
    bodyOut.clear();
}

void stripBodyBom(std::string& b) {
    if (b.size() >= 3 && static_cast<unsigned char>(b[0]) == 0xEF && static_cast<unsigned char>(b[1]) == 0xBB &&
        static_cast<unsigned char>(b[2]) == 0xBF)
        b.erase(0, 3);
}

} // namespace

std::string kernHttpGetWinHttp(const std::string& urlRaw) {
    std::string url = urlRaw;
    trimAsciiWsAndBom(url);
    if (url.empty()) return "";

    std::string body;

    winHttpGetTry(url, false, body);
    if (!body.empty()) {
        stripBodyBom(body);
        return body;
    }

    winHttpGetTry(url, true, body);
    if (!body.empty()) {
        stripBodyBom(body);
        return body;
    }

    winInetGetTry(url, false, body);
    if (!body.empty()) {
        stripBodyBom(body);
        return body;
    }

    winInetGetTry(url, true, body);
    if (!body.empty()) {
        stripBodyBom(body);
        return body;
    }

    return "";
}

} // namespace kern

#endif // _WIN32

