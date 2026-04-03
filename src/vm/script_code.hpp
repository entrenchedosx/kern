/* *
 * script bytecode + constants for imported scripts so functions can be called after import returns.
 */

#ifndef KERN_SCRIPT_CODE_HPP
#define KERN_SCRIPT_CODE_HPP

#include "bytecode.hpp"
#include "value.hpp"
#include <vector>
#include <string>

namespace kern {

struct ScriptCode {
    Bytecode code;
    std::vector<std::string> stringConstants;
    std::vector<Value> valueConstants;
    /** Resolved path for diagnostics (import path or main script path); may be empty. */
    std::string sourcePath;
};

} // namespace kern

#endif // kERN_SCRIPT_CODE_HPP
