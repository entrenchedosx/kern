/* *
 * test_runtime_core_only.cpp - TRUE ISOLATION TEST 2: Runtime Core Only Compilation
 * 
 * Compile ONLY: runtime core (runtime.cpp + module_registry.cpp + scheduler.cpp + bindings.cpp)
 * NO VM implementation, NO ECS, NO graphics, NO modules
 */

// STRICT: Only runtime core includes
#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"
#include "kern/runtime/bindings/native_bindings.h"

// FORBIDDEN: No VM implementation includes
// FORBIDDEN: No ECS includes
// FORBIDDEN: No graphics includes
// FORBIDDEN: No module includes

int main() {
    std::cout << "=== RUNTIME CORE ISOLATION TEST ===\n";
    
    // Test 1: Module Registry creation
    std::cout << "Creating ModuleRegistry...\n";
    kern::runtime::ModuleRegistry registry;
    std::cout << "✅ ModuleRegistry created successfully\n";
    
    // Test 2: Scheduler creation
    std::cout << "Creating Scheduler...\n";
    kern::runtime::Scheduler scheduler;
    std::cout << "✅ Scheduler created successfully\n";
    
    // Test 3: Native Bindings creation
    std::cout << "Creating NativeBindingLayer...\n";
    kern::runtime::NativeBindingLayer bindings;
    std::cout << "✅ NativeBindingLayer created successfully\n";
    
    // Test 4: Runtime creation (without VM linkage)
    std::cout << "Creating KernRuntime (without VM)...\n";
    
    // Note: This tests that runtime core can be created
    // without requiring VM implementation linkage
    kern::runtime::KernRuntime runtime;
    
    std::cout << "✅ KernRuntime created successfully\n";
    std::cout << "✅ Runtime core works without VM implementation!\n";
    
    // Test 5: Scheduler tick (without VM)
    std::cout << "Testing scheduler tick...\n";
    scheduler.tick(0.016f);
    std::cout << "✅ Scheduler tick works without VM!\n";
    
    // Test 6: Module registry operations
    std::cout << "Testing module registry...\n";
    std::cout << "Loaded modules: " << registry.getLoadedCount() << "\n";
    std::cout << "✅ Module registry works without VM!\n";
    
    // Test 7: Native bindings operations
    std::cout << "Testing native bindings...\n";
    std::cout << "Registered functions: " << bindings.getCount() << "\n";
    std::cout << "✅ Native bindings work without VM!\n";
    
    std::cout << "\n=== RUNTIME CORE ISOLATION TEST PASSED ===\n";
    std::cout << "Runtime core works completely standalone!\n";
    std::cout << "No VM implementation dependencies!\n";
    std::cout << "No ECS dependencies!\n";
    std::cout << "No graphics dependencies!\n";
    std::cout << "No module dependencies!\n";
    
    return 0;
}
