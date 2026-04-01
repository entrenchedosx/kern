#include "fw/network/HttpClient.hpp"
#include "fw/network/TcpSocket.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>
#include <sstream>

namespace fw::network {

namespace {
struct ParsedUrl {
    std::string scheme;
    std::string host;
    uint16_t port{80};
    std::string path{"/"};
};

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::optional<ParsedUrl> parseUrl(const std::string& url) {
    ParsedUrl u{};
    const std::string prefix = "http://";
    size_t i = 0;
    if (url.rfind(prefix, 0) == 0) {
        u.scheme = "http";
        i = prefix.size();
    }
    const size_t slash = url.find('/', i);
    const std::string hostPort = slash == std::string::npos ? url.substr(i) : url.substr(i, slash - i);
    u.path = slash == std::string::npos ? "/" : url.substr(slash);
    const size_t colon = hostPort.find(':');
    if (colon == std::string::npos) {
        u.host = hostPort;
        u.port = 80;
    } else {
        u.host = hostPort.substr(0, colon);
        const std::string_view portView(hostPort.data() + colon + 1, hostPort.size() - colon - 1);
        if (portView.empty()) return std::nullopt;
        unsigned int parsedPort = 0;
        const char* begin = portView.data();
        const char* end = begin + portView.size();
        auto conv = std::from_chars(begin, end, parsedPort, 10);
        if (conv.ec != std::errc{} || conv.ptr != end || parsedPort == 0 || parsedPort > 65535) return std::nullopt;
        u.port = static_cast<uint16_t>(parsedPort);
    }
    if (u.host.empty()) return std::nullopt;
    return u;
}

static std::unordered_map<std::string, std::string> parseHeaders(const std::string& block) {
    std::unordered_map<std::string, std::string> out;
    out.reserve(24);
    std::istringstream in(block);
    std::string line;
    std::getline(in, line); // status line
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        const size_t sep = line.find(':');
        if (sep == std::string::npos) continue;
        std::string key = toLower(line.substr(0, sep));
        size_t start = sep + 1;
        while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) ++start;
        out[key] = line.substr(start);
    }
    return out;
}

static std::string decodeChunked(const std::string& body) {
    std::string out;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t lineEnd = body.find("\r\n", pos);
        if (lineEnd == std::string::npos) break;
        const std::string_view lineView(body.data() + pos, lineEnd - pos);
        const size_t ext = lineView.find(';');
        const std::string_view lenHex = ext == std::string_view::npos ? lineView : lineView.substr(0, ext);
        size_t chunkLen = 0;
        auto conv = std::from_chars(lenHex.data(), lenHex.data() + lenHex.size(), chunkLen, 16);
        if (conv.ec != std::errc{}) break;
        pos = lineEnd + 2;
        if (chunkLen == 0) break;
        if (pos + chunkLen + 2 > body.size()) break;
        out.append(body.substr(pos, chunkLen));
        pos += chunkLen;
        if (body.compare(pos, 2, "\r\n") != 0) break;
        pos += 2; // skip CRLF
    }
    return out;
}
} // namespace

HttpResponse HttpClient::get(const std::string& url) {
    HttpRequest req{};
    req.method = "GET";
    req.url = url;
    return execute(req);
}

HttpResponse HttpClient::execute(const HttpRequest& request) {
    HttpRequest current = request;
    HttpResponse response{};

    for (int redirect = 0; redirect <= current.maxRedirects; ++redirect) {
        auto parsed = parseUrl(current.url);
        if (!parsed.has_value()) {
            response.statusCode = 0;
            response.statusText = "invalid url";
            response.finalUrl = current.url;
            return response;
        }
        ParsedUrl u = *parsed;
        TcpSocket socket;
        if (!socket.connect(u.host, u.port, current.timeoutMs)) {
            response.statusCode = 0;
            response.statusText = "connect failed";
            response.finalUrl = current.url;
            return response;
        }

        std::ostringstream req;
        req << current.method << " " << u.path << " HTTP/1.1\r\n";
        req << "Host: " << u.host << "\r\n";
        req << "Connection: close\r\n";
        for (const auto& [k, v] : current.headers) req << k << ": " << v << "\r\n";
        if (!current.body.empty()) req << "Content-Length: " << current.body.size() << "\r\n";
        req << "\r\n";
        req << current.body;

        if (!socket.sendAll(req.str())) {
            response.statusCode = 0;
            response.statusText = "send failed";
            response.finalUrl = current.url;
            return response;
        }

        const std::string raw = socket.receiveAll();
        const size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            response.statusCode = 0;
            response.statusText = "invalid response";
            response.finalUrl = current.url;
            return response;
        }

        const std::string headerBlock = raw.substr(0, headerEnd);
        std::string body = raw.substr(headerEnd + 4);
        std::istringstream firstLine(headerBlock);
        std::string httpVersion;
        firstLine >> httpVersion >> response.statusCode;
        std::getline(firstLine, response.statusText);
        if (!response.statusText.empty() && response.statusText.front() == ' ') response.statusText.erase(response.statusText.begin());
        if (!response.statusText.empty() && response.statusText.back() == '\r') response.statusText.pop_back();

        response.headers = parseHeaders(headerBlock);
        const auto transferIt = response.headers.find("transfer-encoding");
        if (transferIt != response.headers.end() && toLower(transferIt->second).find("chunked") != std::string::npos) {
            body = decodeChunked(body);
        }
        response.body = std::move(body);
        response.finalUrl = current.url;

        if ((response.statusCode == 301 || response.statusCode == 302 || response.statusCode == 307 || response.statusCode == 308) &&
            response.headers.find("location") != response.headers.end()) {
            std::string location = response.headers["location"];
            if (location.rfind("http://", 0) != 0) {
                if (location.empty() || location.front() != '/') location = "/" + location;
                location = "http://" + u.host + location;
            }
            current.url = location;
            continue;
        }
        return response;
    }
    return response;
}

} // namespace fw::network

