#ifndef KERN_PACKAGER_BUNDLE_WRITER_HPP
#define KERN_PACKAGER_BUNDLE_WRITER_HPP

#include "packager/bundle_manifest.hpp"

#include <string>

namespace kern {

bool writeBundleAsCppSource(const BundleManifest& manifest, const std::string& outCppPath, std::string& error);

} // namespace kern

#endif // kERN_PACKAGER_BUNDLE_WRITER_HPP
