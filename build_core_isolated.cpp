/* *
 * build_core_isolated.cpp - TRUE BUILD TEST 1: Core Only Compilation
 * 
 * Compile ONLY: VM + runtime + scheduler + bindings
 * NO ECS, NO graphics, NO modules
 */

// STRICT: Only runtime core includes
#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"
#include "kern/runtime/bindings/native_bindings.h"

// FORBIDDEN: No module includes
// FORBIDDEN: No ECS includes
// FORBIDDEN: No graphics includes
// FORBIDDEN: No legacy includes

int main() {
    // Test minimal runtime functionality
    kern::runtime::KernRuntime runtime;
    runtime.tick(0.016f);
    return 0;
}
