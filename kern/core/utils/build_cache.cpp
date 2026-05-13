#include "utils/build_cache.hpp"

#include <fstream>
#include <sstream>

namespace kern {

bool loadBuildCache(const std::string& cacheFile, BuildCache& out) {
    std::ifstream in(cacheFile);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        size_t p1 = line.find('|');
        size_t p2 = line.find('|', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) continue;
        std::string path = line.substr(0, p1);
        CacheEntry e;
        e.hash = line.substr(p1 + 1, p2 - p1 - 1);
        e.timestamp = static_cast<uint64_t>(std::stoull(line.substr(p2 + 1)));
        out.modules[path] = e;
    }
    return true;
}

bool saveBuildCache(const std::string& cacheFile, const BuildCache& cache) {
    std::ofstream out(cacheFile, std::ios::trunc);
    if (!out) return false;
    for (const auto& kv : cache.modules) {
        out << kv.first << "|" << kv.second.hash << "|" << kv.second.timestamp << "\n";
    }
    return true;
}

std::string hashContent(const std::string& content) {
    // fNV-1a 64-bit
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : content) {
        h ^= static_cast<uint64_t>(c);
        h *= 1099511628211ull;
    }
    std::ostringstream ss;
    ss << std::hex << h;
    return ss.str();
}

} // namespace kern
