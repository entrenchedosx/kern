#include "utils/kernconfig.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace kern {

namespace {

static bool extractString(const std::string& json, const std::string& key, std::string& out) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return false;
    if (m.size() < 2) return false;
    out = m[1].str();
    return true;
}

static bool extractBool(const std::string& json, const std::string& key, bool& out) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return false;
    if (m.size() < 2) return false;
    out = (m[1].str() == "true");
    return true;
}

static bool extractInt(const std::string& json, const std::string& key, int& out) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9]+)");
    std::smatch m;
    if (!std::regex_search(json, m, re)) return false;
    if (m.size() < 2) return false;
    try {
        out = std::stoi(m[1].str());
    } catch (const std::logic_error&) {
        return false;
    }
    return true;
}

static bool extractStringArray(const std::string& json, const std::string& key, std::vector<std::string>& out) {
    std::regex arrRe("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch arr;
    if (!std::regex_search(json, arr, arrRe)) return false;
    if (arr.size() < 2) return false;
    std::string body = arr[1].str();
    std::regex item("\"([^\"]*)\"");
    for (std::sregex_iterator it(body.begin(), body.end(), item), end; it != end; ++it) {
        out.push_back((*it)[1].str());
    }
    return true;
}

} // namespace

bool loadSplConfig(const std::string& path, SplConfig& out, std::string& error) {
    std::ifstream probe(path, std::ios::binary);
    if (!probe) {
        error = "could not open config file: " + path;
        return false;
    }
    std::string text;
    {
        std::ostringstream ss;
        ss << probe.rdbuf();
        text = ss.str();
    }
    if (text.empty()) {
        error = "config file is empty: " + path;
        return false;
    }

    extractString(text, "entry", out.entry);
    extractString(text, "output", out.output);
    extractString(text, "target", out.target);
    extractString(text, "kernel_cross_prefix", out.kernelCrossPrefix);
    extractString(text, "kernel_arch", out.kernelArch);
    extractString(text, "bootloader", out.kernelBootloader);
    extractString(text, "kernel_linker_script", out.kernelLinkerScript);
    extractString(text, "feature_set", out.featureSet);
    extractStringArray(text, "include_paths", out.includePaths);
    extractStringArray(text, "files", out.explicitFiles);
    extractStringArray(text, "exclude", out.exclude);
    extractStringArray(text, "assets", out.assets);
    extractString(text, "icon", out.icon);
    extractStringArray(text, "plugins_pre_build", out.preBuildCommands);
    extractStringArray(text, "plugins_post_build", out.postBuildCommands);
    extractStringArray(text, "enabled_features", out.enabledFeatures);
    extractStringArray(text, "disabled_features", out.disabledFeatures);
    extractStringArray(text, "ffi_allow_libraries", out.ffiAllowLibraries);
    extractString(text, "capability_profile", out.capabilityProfile);
    extractBool(text, "console", out.console);
    extractBool(text, "release", out.release);
    extractBool(text, "debug_runtime", out.debugRuntime);
    extractBool(text, "allow_unsafe", out.allowUnsafe);
    extractBool(text, "ffi_enabled", out.ffiEnabled);
    extractBool(text, "sandbox", out.sandboxEnabled);
    extractBool(text, "freestanding", out.freestanding);
    extractBool(text, "kernel_iso", out.kernelBuildIso);
    extractBool(text, "kernel_run_qemu", out.kernelRunQemu);
    extractInt(text, "optimization", out.optimizationLevel);

    if (out.entry.empty()) {
        error = "kernconfig missing required key: entry";
        return false;
    }
    if (out.output.empty()) out.output = "dist/app.exe";
    if (out.target.empty()) out.target = "native";
    if (out.kernelCrossPrefix.empty()) out.kernelCrossPrefix = "x86_64-elf-";
    if (out.kernelArch.empty()) out.kernelArch = "x86_64";
    if (out.kernelBootloader.empty()) out.kernelBootloader = "grub";
    if (out.featureSet.empty()) out.featureSet = "stable";
    if (out.optimizationLevel < 0) out.optimizationLevel = 0;
    if (out.optimizationLevel > 3) out.optimizationLevel = 3;
    return true;
}

} // namespace kern
