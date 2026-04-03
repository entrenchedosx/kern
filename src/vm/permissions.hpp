/* *
 * Runtime permission ids for sensitive builtins (filesystem, process, env, shell, network).
 * Safe-by-default: blocked unless unsafe {}, require("..."), --allow=..., or --unsafe.
 */
#ifndef KERN_VM_PERMISSIONS_HPP
#define KERN_VM_PERMISSIONS_HPP

#include "vm.hpp"
#include "vm_error_codes.hpp"
#include "errors.hpp"
#include <sstream>
#include <string>

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

inline bool vmPermissionAllowed(VM* vm, const char* permissionId) {
    if (!vm || !permissionId) return false;
    const RuntimeGuardPolicy& g = vm->getRuntimeGuards();
    if (!g.enforcePermissions) return true;
    if (g.allowUnsafe) return true;
    if (vm->isInUnsafeContext()) return true;
    return g.grantedPermissions.find(permissionId) != g.grantedPermissions.end();
}

inline void vmRequirePermission(VM* vm, const char* permissionId, const char* builtinName) {
    if (!vm || !permissionId) return;
    if (vmPermissionAllowed(vm, permissionId)) return;
    std::ostringstream os;
    os << "KERN-PERMISSION-DENIED: '" << permissionId << "' required\n"
       << "  Builtin: " << (builtinName ? builtinName : "?") << "\n";
    const auto cs = vm->getCallStackSlice(6);
    if (!cs.empty()) {
        os << "  Call site:\n";
        for (const auto& fr : cs) {
            os << "    " << (fr.functionName.empty() ? "<script>" : fr.functionName);
            if (!fr.filePath.empty())
                os << "  " << humanizePathForDisplay(fr.filePath) << ":" << fr.line;
            else
                os << "  line " << fr.line;
            if (fr.column > 0) os << ":" << fr.column;
            os << "\n";
        }
    } else {
        os << "  Call site: (stack unavailable — top-level builtin?)\n";
    }
    os << "  Fix:\n"
       << "    - require(\"" << permissionId << "\")\n"
       << "    - kern --allow=" << permissionId << "\n"
       << "    - unsafe { ... }\n"
       << "    - kern --unsafe  (global unlock)\n"
       << "    - KERN_ENFORCE_PERMISSIONS=0  (disable enforcement; dev/CI only)";
    throw VMError(os.str(), 0, 0, 5, static_cast<int>(VMErrorCode::PERMISSION_DENIED));
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
