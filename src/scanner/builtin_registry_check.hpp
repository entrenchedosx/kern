#ifndef KERN_SCANNER_BUILTIN_REGISTRY_CHECK_HPP
#define KERN_SCANNER_BUILTIN_REGISTRY_CHECK_HPP

#include "errors/errors.hpp"
#include "vm/vm.hpp"

namespace kern {

/* * Duplicate names in getBuiltinNames(), missing VM registrations, and high-water consistency.*/
void emitBuiltinRegistryDiagnostics(VM& vm, ErrorReporter& rep);

} // namespace kern

#endif
