/* *
 * build_core_final.cpp - TRUE BUILD TEST 1: Core Only Compilation
 * 
 * FINAL TEST: Compile ONLY VM + runtime + scheduler + bindings
 * NO ECS, NO graphics, NO modules, NO legacy references
 */

// STRICT: Only runtime core includes
#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"
#include "kern/runtime/bindings/native_bindings.h"

// FORBIDDEN: No module includes, no ECS, no graphics, no legacy

int main() {
    // Test minimal runtime functionality
    kern::runtime::KernRuntime runtime;
    runtime.tick(0.016f);
    return 0;
}
