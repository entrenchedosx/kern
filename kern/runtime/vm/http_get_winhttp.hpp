#pragma once

#include <string>

namespace kern {

/* * hTTPS/HTTP GET using WinHTTP (Windows). Returns empty string on failure.*/
std::string kernHttpGetWinHttp(const std::string& url);

} // namespace kern
