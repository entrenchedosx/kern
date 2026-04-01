#ifndef KERN_BACKEND_CPP_BACKEND_HPP
#define KERN_BACKEND_CPP_BACKEND_HPP

#include "ir/ir.hpp"
#include "utils/kernconfig.hpp"

#include <string>

namespace kern {

struct CppBackendResult {
    std::string generatedCpp;
    std::string entryPath;
};

bool generateCppBundle(const IRProgram& program, const SplConfig& config, CppBackendResult& out, std::string& error);
bool buildStandaloneExe(const CppBackendResult& input, const SplConfig& config, const std::string& workspaceRoot, std::string& error);
bool buildKernelArtifacts(const SplConfig& config, const std::string& workspaceRoot, std::string& kernelBinOut, std::string& isoOut, std::string& error);

} // namespace kern

#endif // kERN_BACKEND_CPP_BACKEND_HPP
