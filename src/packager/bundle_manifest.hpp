#ifndef KERN_PACKAGER_BUNDLE_MANIFEST_HPP
#define KERN_PACKAGER_BUNDLE_MANIFEST_HPP

#include <string>
#include <vector>

namespace kern {

struct EmbeddedFile {
    std::string virtualPath;
    std::vector<unsigned char> bytes;
};

struct BundleManifest {
    std::string entryModulePath;
    std::vector<EmbeddedFile> modules;
    std::vector<EmbeddedFile> assets;
};

} // namespace kern

#endif // kERN_PACKAGER_BUNDLE_MANIFEST_HPP
