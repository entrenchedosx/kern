#pragma once

#include <string>

namespace kern {

/* * HTTPS/HTTP GET using WinHTTP (Windows). Returns empty string on failure. */
std::string kernHttpGetWinHttp(const std::string& url);

/* * HTTPS/HTTP POST using WinHTTP (Windows). Sends payload as request body
 *  with Content-Type: application/json. Returns response body, or empty on failure. */
std::string kernHttpPostWinHttp(const std::string& url, const std::string& payload);

} // namespace kern
