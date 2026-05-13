#ifndef KERN_UTILS_BUILD_CACHE_HPP
#define KERN_UTILS_BUILD_CACHE_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

namespace kern {

struct CacheEntry {
    std::string hash;
    uint64_t timestamp = 0;
};

struct BuildCache {
    std::unordered_map<std::string, CacheEntry> modules;
};

bool loadBuildCache(const std::string& cacheFile, BuildCache& out);
bool saveBuildCache(const std::string& cacheFile, const BuildCache& cache);
std::string hashContent(const std::string& content);

} // namespace kern

#endif // kERN_UTILS_BUILD_CACHE_HPP
