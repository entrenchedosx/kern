#ifndef KERN_INPUT_MODULE_HPP
#define KERN_INPUT_MODULE_HPP

#include "bytecode/value.hpp"

#include <memory>

namespace kern {

class VM;
struct RuntimeServices;

ValuePtr createInputModule(VM& vm, const std::shared_ptr<RuntimeServices>& services);

} // namespace kern

#endif

