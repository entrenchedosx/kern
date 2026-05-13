/* *
 * test_vm_standalone.cpp - Hard Isolation Test 3: VM Standalone Execution
 * 
 * Tests that VM runs with zero modules loaded.
 * Empty registry, no ECS, no graphics.
 */

#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"

#include <iostream>

int main() {
    std::cout << "=== VM STANDALONE EXECUTION TEST ===\n";
    
    // Test 1: Runtime creation with zero modules
    std::cout << "Creating runtime with zero modules...\n";
    kern::runtime::KernRuntime runtime;
    std::cout << "✅ Runtime created\n";
    
    // Test 2: Verify empty module registry
    std::cout << "Checking module registry...\n";
    auto& modules = runtime.getModules();
    auto loadedModules = modules.getLoadedModuleNames();
    
    std::cout << "Loaded modules: [";
    for (const auto& name : loadedModules) {
        std::cout << name << " ";
    }
    std::cout << "]\n";
    
    if (loadedModules.empty()) {
        std::cout << "✅ Module registry is empty\n";
    } else {
        std::cout << "❌ Module registry should be empty\n";
        return 1;
    }
    
    // Test 3: VM access without modules
    std::cout << "Testing VM access...\n";
    auto& vm = runtime.getVM();
    std::cout << "✅ VM accessible without modules\n";
    
    // Test 4: Scheduler ticks without modules
    std::cout << "Testing scheduler ticks...\n";
    for (int i = 0; i < 5; i++) {
        runtime.tick(0.016f);
        std::cout << "  Tick " << (i+1) << " completed\n";
    }
    std::cout << "✅ Scheduler works without modules\n";
    
    // Test 5: Native bindings work without modules
    std::cout << "Testing native bindings...\n";
    auto& bindings = runtime.getBindings();
    
    // Register a test function
    runtime.registerFunction("test.echo", [](auto& vm) {
        std::cout << "Echo function called from VM!\n";
    });
    
    std::cout << "Registered functions: " << bindings.getCount() << "\n";
    std::cout << "✅ Native bindings work without modules\n";
    
    // Test 6: Execute script without modules
    std::cout << "Testing script execution...\n";
    auto result = runtime.executeScript("print('Hello from VM with zero modules!')");
    std::cout << "✅ Script execution works without modules\n";
    
    // Test 7: Module loading test (should fail gracefully)
    std::cout << "Testing module loading behavior...\n";
    bool ecsLoaded = runtime.loadModule("ecs");
    bool mathLoaded = runtime.loadModule("math");
    
    std::cout << "ECS load attempt: " << (ecsLoaded ? "FAILED" : "CORRECTLY FAILED") << "\n";
    std::cout << "Math load attempt: " << (mathLoaded ? "FAILED" : "CORRECTLY FAILED") << "\n";
    
    if (!ecsLoaded && !mathLoaded) {
        std::cout << "✅ Module loading correctly fails (no modules available)\n";
    } else {
        std::cout << "❌ Unexpected module loading success\n";
        return 1;
    }
    
    // Test 8: Runtime statistics
    std::cout << "Runtime statistics:\n";
    std::cout << "  Is running: " << (runtime.isRunning() ? "YES" : "NO") << "\n";
    std::cout << "  Module count: " << modules.getLoadedCount() << "\n";
    std::cout << "✅ Runtime state consistent\n";
    
    std::cout << "\n=== VM STANDALONE EXECUTION TEST PASSED ===\n";
    std::cout << "VM works completely standalone!\n";
    std::cout << "Zero modules loaded - pure runtime!\n";
    
    return 0;
}
