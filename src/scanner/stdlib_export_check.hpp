#ifndef KERN_SCANNER_STDLIB_EXPORT_CHECK_HPP
#define KERN_SCANNER_STDLIB_EXPORT_CHECK_HPP

#include "errors.hpp"

namespace kern {

/* * Ensures std.v1 module export targets resolve to registered builtin names (or documented aliases).*/
void emitStdlibExportDiagnostics(ErrorReporter& rep);

} // namespace kern

#endif
