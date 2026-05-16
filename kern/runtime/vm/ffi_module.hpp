#pragma once
/* *
 * Kern FFI Module — Dynamic Foreign Function Interface
 *
 * Provides runtime loading of native shared libraries (.dll / .so)
 * and dynamic binding of C functions.
 */

#ifndef KERN_FFI_MODULE_HPP
#define KERN_FFI_MODULE_HPP

#include <vector>
#include <memory>
#include <string>

namespace kern {

class VM;
struct Value;
struct FfiClosure;
using ValuePtr = std::shared_ptr<Value>;

/// Register ffi_load, ffi_bind, ffi_free globals with the VM.
void registerFfiBuiltins(VM& vm);

/// Call an FFI closure with the given arguments.
/// Returns the result as a Kern Value.
Value callFfiFunction(const FfiClosure* ffi, const std::vector<ValuePtr>& args);

// ── C++ API (callable from tests / native code) ──────────────────────────

/// Load a shared library (.dll / .so).  Returns ptr Value or nil on failure.
Value builtinFfiLoad(VM* vm, std::vector<ValuePtr> args);

/// Bind a function from a loaded library.  Returns FFI_FN Value or nil.
Value builtinFfiBind(VM* vm, std::vector<ValuePtr> args);

/// Free a previously loaded library handle.  Returns nil.
Value builtinFfiFree(VM* vm, std::vector<ValuePtr> args);

} // namespace kern

#endif // KERN_FFI_MODULE_HPP
