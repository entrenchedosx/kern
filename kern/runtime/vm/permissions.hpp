/* *
 * Runtime permission ids for sensitive builtins (filesystem, process, env, shell, network).
 * Safe-by-default: blocked unless unsafe {}, require("..."), --allow=..., or --unsafe.
 */
#ifndef KERN_VM_PERMISSIONS_HPP
#define KERN_VM_PERMISSIONS_HPP

#include "vm.hpp"
#include "errors/vm_error_codes.hpp"
#include "errors/errors.hpp"
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace kern {

namespace Perm {
inline constexpr const char* kFilesystemRead = "filesystem.read";
inline constexpr const char* kFilesystemWrite = "filesystem.write";
inline constexpr const char* kSystemExec = "system.exec";
inline constexpr const char* kProcessControl = "process.control";
inline constexpr const char* kEnvAccess = "env.access";
/** Outbound HTTP/HTTPS (http_get, http_request, http_post). */
inline constexpr const char* kNetworkHttp = "network.http";
/** Reserved for future TCP socket APIs. */
inline constexpr const char* kNetworkTcp = "network.tcp";
/** Reserved for future UDP socket APIs. */
inline constexpr const char* kNetworkUdp = "network.udp";
} // namespace Perm

inline const std::unordered_map<std::string, std::vector<std::string>>& permissionGroupMap() {
    static const std::unordered_map<std::string, std::vector<std::string>> groups = {
        {"fs.readonly", {Perm::kFilesystemRead}},
        {"fs.readwrite", {Perm::kFilesystemRead, Perm::kFilesystemWrite}},
        {"net.client", {Perm::kNetworkHttp, Perm::kNetworkTcp, Perm::kNetworkUdp}},
        {"proc.control", {Perm::kProcessControl, Perm::kSystemExec}},
        {"env.manage", {Perm::kEnvAccess}},
        {"system.full", {Perm::kFilesystemRead, Perm::kFilesystemWrite, Perm::kSystemExec, Perm::kProcessControl,
                         Perm::kEnvAccess, Perm::kNetworkHttp, Perm::kNetworkTcp, Perm::kNetworkUdp}},
    };
    return groups;
}

inline std::vector<std::string> resolvePermissionToken(const std::string& token) {
    auto it = permissionGroupMap().find(token);
    if (it != permissionGroupMap().end()) return it->second;
    return {token};
}

inline const std::unordered_map<std::string, std::vector<std::string>>& capabilityProfiles() {
    static const std::unordered_map<std::string, std::vector<std::string>> profiles = {
        {"secure", {}},
        {"dev", {"fs.readwrite", "net.client", "proc.control", "env.manage"}},
        {"ci", {"fs.readwrite", "net.client", "proc.control"}},
    };
    return profiles;
}

inline bool applyCapabilityProfile(const std::string& profile, RuntimeGuardPolicy& g) {
    auto it = capabilityProfiles().find(profile);
    if (it == capabilityProfiles().end()) return false;
    for (const auto& tok : it->second) {
        for (const auto& p : resolvePermissionToken(tok))
            g.grantedPermissions.insert(p);
    }
    return true;
}

inline bool vmPermissionAllowed(VM* vm, const char* permissionId) {
    // Kern operates on "trust everything" model - all operations allowed
    (void)vm;
    (void)permissionId;
    return true;
}

inline void vmRequirePermission(VM* vm, const char* permissionId, const char* builtinName) {
    // No-op: all permissions granted automatically
    (void)vm;
    (void)permissionId;
    (void)builtinName;
}

/** Pre-grant all standard permission strings (used by kern test / REPL). */
inline void registerAllStandardPermissions(RuntimeGuardPolicy& g) {
    g.grantedPermissions.insert(Perm::kFilesystemRead);
    g.grantedPermissions.insert(Perm::kFilesystemWrite);
    g.grantedPermissions.insert(Perm::kSystemExec);
    g.grantedPermissions.insert(Perm::kProcessControl);
    g.grantedPermissions.insert(Perm::kEnvAccess);
    g.grantedPermissions.insert(Perm::kNetworkHttp);
    g.grantedPermissions.insert(Perm::kNetworkTcp);
    g.grantedPermissions.insert(Perm::kNetworkUdp);
}

} // namespace kern

#endif
