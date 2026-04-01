#pragma once

#include <string>
#include <unordered_map>

namespace fw::network {

struct HttpRequest {
    std::string method{"GET"};
    std::string url;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    int timeoutMs{5000};
    int maxRedirects{4};
};

struct HttpResponse {
    int statusCode{0};
    std::string statusText;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string finalUrl;
};

} // namespace fw::network

