#ifndef KERN_UTILS_KERNCONFIG_HPP
#define KERN_UTILS_KERNCONFIG_HPP

#include <string>
#include <vector>

namespace kern {

struct SplConfig {
    std::string entry;
    std::string output;
    std::string target = "native";          // native | kernel
    bool freestanding = false;              // true for kernel/system builds
    bool kernelBuildIso = false;            // emit bootable ISO when possible
    bool kernelRunQemu = false;             // run kernel in qemu after build
    std::string kernelCrossPrefix = "x86_64-elf-";
    std::string kernelArch = "x86_64";
    std::string kernelBootloader = "grub";
    std::string kernelLinkerScript;         // optional custom linker script
    std::vector<std::string> includePaths;
    // / optional explicit module paths (must appear in resolved import graph after entry).
    std::vector<std::string> explicitFiles;
    std::vector<std::string> exclude;
    std::vector<std::string> assets;
    // / windows: optional path to .ico embedded in standalone exe (empty = default).
    std::string icon;
    bool console = true;
    bool release = true;
    int optimizationLevel = 2;
    std::vector<std::string> preBuildCommands;
    std::vector<std::string> postBuildCommands;
    std::string featureSet = "stable";
    std::vector<std::string> enabledFeatures;
    std::vector<std::string> disabledFeatures;
    bool debugRuntime = false;
    bool allowUnsafe = false;
    bool ffiEnabled = false;
    bool sandboxEnabled = true;
    std::vector<std::string> ffiAllowLibraries;
};

bool loadSplConfig(const std::string& path, SplConfig& out, std::string& error);

} // namespace kern

#endif // kERN_UTILS_KERNCONFIG_HPP
