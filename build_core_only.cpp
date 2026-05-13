/* *
 * build_core_only.cpp - Build Matrix Test 1: Core Only
 * 
 * Minimal build: runtime core + VM only
 * No ECS, no graphics, no modules
 */

// ONLY runtime core includes
#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"
#include "kern/runtime/bindings/native_bindings.h"

// NO module includes
// NO ECS includes
// NO graphics includes

int main() {
    // Test minimal runtime functionality
    kern::runtime::KernRuntime runtime;
    runtime.tick(0.016f);
    return 0;
}
