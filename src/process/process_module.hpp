/* *
 * kern process module – safe process inspection utilities (read-only).
 */
#ifndef KERN_PROCESS_MODULE_HPP
#define KERN_PROCESS_MODULE_HPP

#include "vm/value.hpp"
#include <memory>

namespace kern {
class VM;
struct RuntimeServices;

/* * create the process module (map of name -> function). Used by import("process").*/
ValuePtr createProcessModule(VM& vm, const std::shared_ptr<RuntimeServices>& services);
} // namespace kern

#endif
