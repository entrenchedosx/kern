#include "fw/resource/ResourceManager.hpp"

#include "fw/network/HttpClient.hpp"

#include <fstream>

namespace fw::resource {

std::string ResourceManager::loadText(const std::string& pathOrUrl) {
    if (pathOrUrl.rfind("http://", 0) == 0) {
        network::HttpClient client;
        auto resp = client.get(pathOrUrl);
        return resp.body;
    }
    std::ifstream f(pathOrUrl, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    if (size <= 0) return {};
    std::string content(static_cast<size_t>(size), '\0');
    f.seekg(0, std::ios::beg);
    f.read(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f) content.resize(static_cast<size_t>(f.gcount()));
    return content;
}

std::shared_future<std::string> ResourceManager::loadTextAsync(const std::string& pathOrUrl) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(pathOrUrl);
    if (it != cache_.end()) return it->second;

    auto fut = std::async(std::launch::async, [this, pathOrUrl]() {
        return loadText(pathOrUrl);
    }).share();
    return cache_.emplace(pathOrUrl, fut).first->second;
}

void ResourceManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

} // namespace fw::resource

