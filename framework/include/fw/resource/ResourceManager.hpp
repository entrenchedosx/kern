#pragma once

#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fw::resource {

class ResourceManager {
public:
    std::string loadText(const std::string& pathOrUrl);
    std::shared_future<std::string> loadTextAsync(const std::string& pathOrUrl);
    void clear();

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_future<std::string>> cache_;
};

} // namespace fw::resource

