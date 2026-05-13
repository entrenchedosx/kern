#ifndef KERN_VM_ERROR_REGISTRY_HPP
#define KERN_VM_ERROR_REGISTRY_HPP

#include "errors/vm_error_codes.hpp"
#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

namespace kern {

struct VMErrorMeta {
    VMErrorCode code = VMErrorCode::NONE;
    int category = 1;
    const char* enumName = "NONE";
    const char* stableCode = "VM-RUN";
    const char* hint = "";
    const char* detail = "";
};

inline constexpr std::array<VMErrorMeta, 24> kVMErrorRegistryTable = {{
        {VMErrorCode::BROWSERKIT_INIT_FAIL, 7, "BROWSERKIT_INIT_FAIL", "BROWSERKIT-INIT-FAIL",
            "BrowserKit could not initialize a required runtime or bridge dependency.",
            "BrowserKit initialization failed while preparing internal runtime state or required native bridge dependencies."},
        {VMErrorCode::BROWSERKIT_RENDER_FAIL, 7, "BROWSERKIT_RENDER_FAIL", "BROWSERKIT-RENDER-FAIL",
            "BrowserKit render pipeline failed. Check layout/paint preconditions and graphics state.",
            "BrowserKit render pipeline failed after DOM/style/layout setup. Validate layout tree and graphics context availability."},
        {VMErrorCode::BROWSERKIT_UNSUPPORTED, 7, "BROWSERKIT_UNSUPPORTED", "BROWSERKIT-UNSUPPORTED",
            "This BrowserKit capability is not supported on the current platform/runtime.",
            "BrowserKit rejected this operation because the capability is unsupported on the current runtime/platform."},
        {VMErrorCode::BROWSERKIT_PROTOCOL_ERROR, 7, "BROWSERKIT_PROTOCOL_ERROR", "BROWSERKIT-PROTOCOL-ERROR",
            "URL scheme/handler failed validation or no handler is registered.",
            "BrowserKit URL protocol handling failed due to invalid scheme registration or unsupported protocol access."},
        {VMErrorCode::BROWSERKIT_SANDBOX_DENY, 7, "BROWSERKIT_SANDBOX_DENY", "BROWSERKIT-SANDBOX-DENY",
            "Sandbox policy denied the requested BrowserKit bridge operation.",
            "BrowserKit attempted an operation denied by runtime sandbox policy (network/process bridge restrictions)."},
        {VMErrorCode::BROWSERKIT_NOT_IMPLEMENTED, 7, "BROWSERKIT_NOT_IMPLEMENTED", "BROWSERKIT-NOT-IMPLEMENTED",
            "This BrowserKit path is intentionally unimplemented and fails loudly.",
            "BrowserKit hit a deliberately unimplemented path that fails loudly to avoid fake/partial behavior."},
        {VMErrorCode::BROWSERKIT_WS_NOT_CONNECTED, 7, "BROWSERKIT_WS_NOT_CONNECTED", "BROWSERKIT-WS-NOT-CONNECTED",
            "WebSocket operation requires an active BrowserKit websocket session.",
            "BrowserKit websocket operation was attempted without an active connected socket object."},
        {VMErrorCode::BROWSERKIT_PROTOCOL_INVALID, 7, "BROWSERKIT_PROTOCOL_INVALID", "BROWSERKIT-PROTOCOL-INVALID",
            "BrowserKit rejected a malformed URL protocol input.",
            "BrowserKit received an invalid URL/protocol format and rejected it before handler dispatch."},
        {VMErrorCode::IMPORT_NOT_FOUND, 8, "IMPORT_NOT_FOUND", "IMPORT-NOT-FOUND",
            "Module file was not found under CWD or KERN_LIB import roots.",
            "Import resolution failed because the requested module file does not exist under allowed roots."},
        {VMErrorCode::IMPORT_CYCLE, 8, "IMPORT_CYCLE", "IMPORT-CYCLE",
            "Import cycle detected. Break the cycle with a shared dependency module.",
            "Import graph contains a cycle. Kern hard-fails to avoid partial initialization order bugs."},
        {VMErrorCode::IMPORT_INVALID_PATH, 8, "IMPORT_INVALID_PATH", "IMPORT-INVALID-PATH",
            "Import path is invalid or violates path traversal / root constraints.",
            "Import path was rejected (invalid format, traversal attempt, or outside configured roots)."},
        {VMErrorCode::IMPORT_READ_FAIL, 8, "IMPORT_READ_FAIL", "IMPORT-READ-FAIL",
            "Import target exists but could not be read or decoded.",
            "Import target was resolved but could not be opened/read, likely due to permissions, locking, or encoding issues."},
        {VMErrorCode::IMPORT_INTERNAL, 8, "IMPORT_INTERNAL", "IMPORT-INTERNAL",
            "Import subsystem failed internally; inspect earlier diagnostics.",
            "Import subsystem raised an internal failure after diagnostics; inspect preceding IMP-* details."},
        {VMErrorCode::IMPORT_UNSUPPORTED_MODULE, 8, "IMPORT_UNSUPPORTED_MODULE", "IMPORT-UNSUPPORTED-MODULE",
            "Requested module is not available on this platform/runtime.",
            "Import requested a built-in module that is intentionally unsupported on this runtime/platform build."},
        {VMErrorCode::STACK_UNDERFLOW, 1, "STACK_UNDERFLOW", "VM-STACK-UNDERFLOW",
            "The VM stack was read with insufficient values.",
            "The VM attempted to pop/peek from an empty stack. This indicates invalid bytecode flow or exception edge handling."},
        {VMErrorCode::STEP_LIMIT_EXCEEDED, 1, "STEP_LIMIT_EXCEEDED", "VM-STEP-LIMIT",
            "Execution exceeded configured step guard limit.",
            "The VM exceeded its configured instruction budget and aborted to prevent runaway execution."},
        {VMErrorCode::RETURN_OUTSIDE_FUNCTION, 1, "RETURN_OUTSIDE_FUNCTION", "VM-RETURN-OUTSIDE-FN",
            "A return opcode executed without an active function frame.",
            "A return opcode executed with no active call frame, which indicates malformed control flow."},
        {VMErrorCode::INVALID_JUMP_TARGET, 1, "INVALID_JUMP_TARGET", "VM-INVALID-JUMP",
            "Control-flow jump target is out of bytecode bounds.",
            "A VM jump/catch target pointed outside valid bytecode bounds, indicating malformed or corrupted control flow."},
        {VMErrorCode::INVALID_BYTECODE, 1, "INVALID_BYTECODE", "VM-INVALID-BYTECODE",
            "Bytecode operand or constant reference is invalid.",
            "The VM rejected malformed bytecode (unexpected operand type/value or out-of-range constant index)."},
        {VMErrorCode::INVALID_CALL_TARGET, 2, "INVALID_CALL_TARGET", "VM-INVALID-CALL",
            "Attempted to call a non-callable value.",
            "A CALL opcode targeted a value that is not a function/builtin, so execution failed loudly."},
        {VMErrorCode::INVALID_OPERATION, 2, "INVALID_OPERATION", "VM-INVALID-OP",
            "Operation is not valid for the given operand types.",
            "The VM rejected an operation because operand types are incompatible for this opcode."},
        {VMErrorCode::DIVISION_BY_ZERO, 4, "DIVISION_BY_ZERO", "VM-DIV-ZERO",
            "Division or modulo by zero is not allowed.",
            "A DIV/MOD opcode attempted to use a zero denominator and was aborted deterministically."},
        {VMErrorCode::PERMISSION_DENIED, 5, "PERMISSION_DENIED", "VM-PERMISSION-DENIED",
            "Runtime permission required for this operation (filesystem, process, env, or shell).",
            "The VM blocked a sensitive builtin because no matching permission was granted, the call was not inside unsafe { }, and global --unsafe was not set."},
        {VMErrorCode::BYTECODE_VERIFY_STACK, 1, "BYTECODE_VERIFY_STACK", "VM-VERIFY-STACK",
            "Bytecode preflight found inconsistent stack depth at the same instruction (invalid control flow).",
            "The bytecode verifier simulated stack depth along control-flow edges and found conflicting merge depths for the same bytecode offset."},
}};

inline constexpr std::array<VMErrorCode, 24> kKnownVMErrorCodes = {{
    VMErrorCode::BROWSERKIT_INIT_FAIL,
    VMErrorCode::BROWSERKIT_RENDER_FAIL,
    VMErrorCode::BROWSERKIT_UNSUPPORTED,
    VMErrorCode::BROWSERKIT_PROTOCOL_ERROR,
    VMErrorCode::BROWSERKIT_SANDBOX_DENY,
    VMErrorCode::BROWSERKIT_NOT_IMPLEMENTED,
    VMErrorCode::BROWSERKIT_WS_NOT_CONNECTED,
    VMErrorCode::BROWSERKIT_PROTOCOL_INVALID,
    VMErrorCode::IMPORT_NOT_FOUND,
    VMErrorCode::IMPORT_CYCLE,
    VMErrorCode::IMPORT_INVALID_PATH,
    VMErrorCode::IMPORT_READ_FAIL,
    VMErrorCode::IMPORT_INTERNAL,
    VMErrorCode::IMPORT_UNSUPPORTED_MODULE,
    VMErrorCode::STACK_UNDERFLOW,
    VMErrorCode::STEP_LIMIT_EXCEEDED,
    VMErrorCode::RETURN_OUTSIDE_FUNCTION,
    VMErrorCode::INVALID_JUMP_TARGET,
    VMErrorCode::INVALID_BYTECODE,
    VMErrorCode::INVALID_CALL_TARGET,
    VMErrorCode::INVALID_OPERATION,
    VMErrorCode::DIVISION_BY_ZERO,
    VMErrorCode::PERMISSION_DENIED,
    VMErrorCode::BYTECODE_VERIFY_STACK,
}};

inline constexpr const std::array<VMErrorMeta, 24>& vmErrorRegistryTable() {
    return kVMErrorRegistryTable;
}

struct TokenMapEntry { std::string_view token; VMErrorCode code; };

inline constexpr std::array<TokenMapEntry, 26> kVMErrorTokenMap = {{
    {"BROWSERKIT-INIT-FAIL", VMErrorCode::BROWSERKIT_INIT_FAIL},
    {"BROWSERKIT-RENDER-FAIL", VMErrorCode::BROWSERKIT_RENDER_FAIL},
    {"BROWSERKIT-UNSUPPORTED", VMErrorCode::BROWSERKIT_UNSUPPORTED},
    {"BROWSERKIT-UNSUPPORTED-PROTOCOL", VMErrorCode::BROWSERKIT_PROTOCOL_ERROR},
    {"BROWSERKIT-PROTOCOL-ERROR", VMErrorCode::BROWSERKIT_PROTOCOL_ERROR},
    {"BROWSERKIT-PROTOCOL-INVALID", VMErrorCode::BROWSERKIT_PROTOCOL_INVALID},
    {"BROWSERKIT-SANDBOX-DENY", VMErrorCode::BROWSERKIT_SANDBOX_DENY},
    {"BROWSERKIT-NOT-IMPLEMENTED", VMErrorCode::BROWSERKIT_NOT_IMPLEMENTED},
    {"BROWSERKIT-WS-NOT-CONNECTED", VMErrorCode::BROWSERKIT_WS_NOT_CONNECTED},
    {"IMPORT-NOT-FOUND", VMErrorCode::IMPORT_NOT_FOUND},
    {"IMPORT-CYCLE", VMErrorCode::IMPORT_CYCLE},
    {"IMPORT-INVALID-PATH", VMErrorCode::IMPORT_INVALID_PATH},
    {"IMPORT-READ-FAIL", VMErrorCode::IMPORT_READ_FAIL},
    {"IMPORT-INTERNAL", VMErrorCode::IMPORT_INTERNAL},
    {"IMPORT-UNSUPPORTED-MODULE", VMErrorCode::IMPORT_UNSUPPORTED_MODULE},
    {"VM-STACK-UNDERFLOW", VMErrorCode::STACK_UNDERFLOW},
    {"VM-STEP-LIMIT", VMErrorCode::STEP_LIMIT_EXCEEDED},
    {"VM-RETURN-OUTSIDE-FN", VMErrorCode::RETURN_OUTSIDE_FUNCTION},
    {"VM-INVALID-JUMP", VMErrorCode::INVALID_JUMP_TARGET},
    {"VM-INVALID-BYTECODE", VMErrorCode::INVALID_BYTECODE},
    {"VM-INVALID-CALL", VMErrorCode::INVALID_CALL_TARGET},
    {"VM-INVALID-OP", VMErrorCode::INVALID_OPERATION},
    {"VM-DIV-ZERO", VMErrorCode::DIVISION_BY_ZERO},
    {"VM-PERMISSION-DENIED", VMErrorCode::PERMISSION_DENIED},
    {"KERN-PERMISSION-DENIED", VMErrorCode::PERMISSION_DENIED},
    {"VM-VERIFY-STACK", VMErrorCode::BYTECODE_VERIFY_STACK},
}};

constexpr bool vmErrorRegistryIsValid() {
    if (kKnownVMErrorCodes.size() != kVMErrorRegistryTable.size()) return false;
    for (size_t i = 0; i < kKnownVMErrorCodes.size(); ++i) {
        bool found = false;
        for (const auto& meta : kVMErrorRegistryTable) {
            if (meta.code == kKnownVMErrorCodes[i]) { found = true; break; }
        }
        if (!found) return false;
    }
    for (size_t i = 0; i < kVMErrorRegistryTable.size(); ++i) {
        const VMErrorMeta& cur = kVMErrorRegistryTable[i];
        if (cur.code == VMErrorCode::NONE) return false;
        if (!cur.enumName || cur.enumName[0] == '\0') return false;
        if (!cur.stableCode || cur.stableCode[0] == '\0') return false;
        if (!cur.hint || cur.hint[0] == '\0') return false;
        if (!cur.detail || cur.detail[0] == '\0') return false;
        for (size_t j = i + 1; j < kVMErrorRegistryTable.size(); ++j) {
            if (kVMErrorRegistryTable[j].code == cur.code) return false;
            if (std::string_view(kVMErrorRegistryTable[j].stableCode) == std::string_view(cur.stableCode)) return false;
        }
    }
    return true;
}

constexpr bool vmErrorTokenMapIsValid() {
    for (size_t i = 0; i < kVMErrorTokenMap.size(); ++i) {
        const TokenMapEntry& cur = kVMErrorTokenMap[i];
        if (cur.token.empty()) return false;
        bool codeFound = false;
        for (const auto& meta : kVMErrorRegistryTable) {
            if (meta.code == cur.code) { codeFound = true; break; }
        }
        if (!codeFound) return false;
        for (size_t j = i + 1; j < kVMErrorTokenMap.size(); ++j) {
            if (kVMErrorTokenMap[j].token == cur.token) return false;
        }
    }
    return true;
}

static_assert(vmErrorRegistryIsValid(), "VMErrorRegistry: invalid or incomplete metadata table");
static_assert(vmErrorTokenMapIsValid(), "VMErrorRegistry: invalid token map");

inline const VMErrorMeta* findVMErrorMeta(VMErrorCode code) {
    if (code == VMErrorCode::NONE) return nullptr;
    for (const auto& e : kVMErrorRegistryTable) {
        if (e.code == code) return &e;
    }
    return nullptr;
}

inline const VMErrorMeta* findVMErrorMeta(int code) {
    return findVMErrorMeta(static_cast<VMErrorCode>(code));
}

inline VMErrorCode vmErrorCodeFromToken(std::string_view token) {
    for (const auto& e : kVMErrorTokenMap) {
        if (e.token == token) return e.code;
    }
    return VMErrorCode::NONE;
}

inline int vmCategoryFromCode(VMErrorCode code, int fallback = 1) {
    const VMErrorMeta* meta = findVMErrorMeta(code);
    return meta ? meta->category : fallback;
}

inline std::string_view vmErrorStableName(VMErrorCode code) {
    const VMErrorMeta* meta = findVMErrorMeta(code);
    return meta ? std::string_view(meta->enumName) : std::string_view("NONE");
}

/** Machine-readable catalog: stable id "E{numeric}" matches VMErrorCode integer values. */
inline std::string formatVmErrorCatalogJson() {
    std::ostringstream os;
    os << "[\n";
    for (size_t i = 0; i < kVMErrorRegistryTable.size(); ++i) {
        const VMErrorMeta& m = kVMErrorRegistryTable[i];
        int n = static_cast<int>(m.code);
        if (i) os << ",\n";
        os << "  {\"id\":\"E" << n << "\",\"numeric\":" << n << ",\"enum\":\"" << m.enumName << "\",\"stableCode\":\"" << m.stableCode
           << "\",\"category\":" << m.category << "}";
    }
    os << "\n]\n";
    return os.str();
}

} // namespace kern

#endif // kERN_VM_ERROR_REGISTRY_HPP
