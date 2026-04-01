/* *
 * kern standard library modules – import math; math.sqrt(2)
 */
#ifndef KERN_STDLIB_MODULES_HPP
#define KERN_STDLIB_MODULES_HPP

#include "vm/vm.hpp"
#include "vm/value.hpp"
#include <string>

namespace kern {

ValuePtr createStdlibModule(VM& vm, const std::string& name);
bool isStdlibModuleName(const std::string& name);

} // namespace kern

#endif
