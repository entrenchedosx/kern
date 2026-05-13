#pragma once

#include "fw/network/HttpTypes.hpp"

namespace fw::network {

class HttpClient {
public:
    HttpClient() = default;
    HttpResponse get(const std::string& url);
    HttpResponse execute(const HttpRequest& request);
};

} // namespace fw::network

