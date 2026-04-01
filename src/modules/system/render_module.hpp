#ifndef KERN_RENDER_MODULE_HPP
#define KERN_RENDER_MODULE_HPP

#include "vm/value.hpp"

#include <memory>

namespace kern {

class VM;
struct RuntimeServices;

ValuePtr createRenderModule(VM& vm, const std::shared_ptr<RuntimeServices>& services);

} // namespace kern

#endif

