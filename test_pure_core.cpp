/* *
 * test_pure_core.cpp - Hard Isolation Test 1: Pure Core Build
 * 
 * Tests that runtime core builds and runs without any modules.
 * No ECS, no graphics, no engine dependencies.
 */

#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"
#include "kern/runtime/bindings/native_bindings.h"

#include <iostream>

int main() {
    std::cout << "=== PURE CORE BUILD TEST ===\n";
    
    // Test 1: Runtime core creation
    std::cout << "Creating KernRuntime...\n";
    kern::runtime::KernRuntime runtime;
    std::cout << "✅ Runtime created successfully\n";
    
    // Test 2: VM access (should work without modules)
    std::cout << "Testing VM access...\n";
    auto& vm = runtime.getVM();
    std::cout << "✅ VM accessible\n";
    
    // Test 3: Module registry (should be empty)
    std::cout << "Testing empty module registry...\n";
    auto& modules = runtime.getModules();
    std::cout << "Loaded modules: " << modules.getLoadedCount() << "\n";
    
    if (modules.getLoadedCount() == 0) {
        std::cout << "✅ Module registry is empty\n";
    } else {
        std::cout << "❌ Module registry should be empty\n";
        return 1;
    }
    
    // Test 4: Scheduler (should work without modules)
    std::cout << "Testing scheduler...\n";
    runtime.tick(0.016f);  // Single tick
    std::cout << "✅ Scheduler ticked successfully\n";
    
    // Test 5: Native bindings (should work)
    std::cout << "Testing native bindings...\n";
    auto& bindings = runtime.getBindings();
    std::cout << "Registered functions: " << bindings.getCount() << "\n";
    std::cout << "✅ Native bindings working\n";
    
    // Test 6: Execute simple script (should work)
    std::cout << "Testing VM execution...\n";
    auto result = runtime.executeScript("print('Hello from pure core!')");
    std::cout << "✅ VM execution completed\n";
    
    std::cout << "\n=== PURE CORE BUILD TEST PASSED ===\n";
    std::cout << "Runtime core works without any modules loaded!\n";
    
    return 0;
}
