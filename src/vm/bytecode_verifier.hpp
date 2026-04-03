/* *
 * Pre-run bytecode verification: operand shapes, jump bounds, and abstract stack depth.
 */

#ifndef KERN_BYTECODE_VERIFIER_HPP
#define KERN_BYTECODE_VERIFIER_HPP

#include "bytecode.hpp"
#include "vm_error_codes.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace kern {

struct BytecodeVerifyResult {
    bool ok = true;
    /** Failing instruction index (0-based) when ok is false. */
    size_t failPc = 0;
    int line = 0;
    int column = 0;
    VMErrorCode code = VMErrorCode::NONE;
    std::string message;
};

/** Validate bytecode before execution. stringPoolSize / valuePoolSize bound CONST_STR and future value-pool ops. */
bool verifyBytecode(const Bytecode& code, size_t stringPoolSize, size_t valuePoolSize, BytecodeVerifyResult& out);

} // namespace kern

#endif
