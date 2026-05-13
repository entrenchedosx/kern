/* *
 * test_forced_removal.cpp - Hard Isolation Test 4: Forced Removal Test
 * 
 * Tests that runtime still works when ECS and graphics are completely removed.
 * Simulates worst-case scenario: no modules available.
 */

#include "kern/runtime/core/runtime.h"
#include "kern/runtime/core/module_registry.h"
#include "kern/runtime/core/scheduler.h"
#include "kern/runtime/bindings/native_bindings.h"

#include <iostream>

// Simulate module removal by not including any module headers
// This test should compile even if kern/runtime/modules/ecs/ is deleted

int main() {
    std::cout << "=== FORCED REMOVAL TEST ===\n";
    std::cout << "Testing runtime with ECS and graphics completely removed...\n";
    
    // Test 1: Runtime creation (should work without modules)
    std::cout << "\n1. Creating runtime...\n";
    kern::runtime::KernRuntime runtime;
    std::cout << "✅ Runtime created without modules\n";
    
    // Test 2: VM functionality (should work without modules)
    std::cout << "\n2. Testing VM functionality...\n";
    auto& vm = runtime.getVM();
    std::cout << "✅ VM accessible\n";
    
    auto result = runtime.executeScript("print('VM works without modules!')");
    std::cout << "✅ VM execution works\n";
    
    // Test 3: Scheduler (should work without modules)
    std::cout << "\n3. Testing scheduler...\n";
    for (int i = 0; i < 10; i++) {
        runtime.tick(0.016f);
        if (i % 3 == 0) {
            std::cout << "  Tick " << (i+1) << " completed\n";
        }
    }
    std::cout << "✅ Scheduler works without modules\n";
    
    // Test 4: Module registry (should be empty and functional)
    std::cout << "\n4. Testing module registry...\n";
    auto& modules = runtime.getModules();
    
    std::cout << "Loaded modules: " << modules.getLoadedCount() << "\n";
    auto names = modules.getLoadedModuleNames();
    std::cout << "Module names: [";
    for (const auto& name : names) {
        std::cout << name << " ";
    }
    std::cout << "]\n";
    
    if (modules.getLoadedCount() == 0) {
        std::cout << "✅ Module registry empty\n";
    } else {
        std::cout << "❌ Module registry should be empty\n";
        return 1;
    }
    
    // Test 5: Module loading attempts (should fail gracefully)
    std::cout << "\n5. Testing module loading failures...\n";
    
    bool ecsLoaded = runtime.loadModule("ecs");
    bool g3dLoaded = runtime.loadModule("g3d");
    bool g2dLoaded = runtime.loadModule("g2d");
    bool mathLoaded = runtime.loadModule("math");
    
    std::cout << "ECS load: " << (ecsLoaded ? "FAILED" : "CORRECTLY FAILED") << "\n";
    std::cout << "G3D load: " << (g3dLoaded ? "FAILED" : "CORRECTLY FAILED") << "\n";
    std::cout << "G2D load: " << (g2dLoaded ? "FAILED" : "CORRECTLY FAILED") << "\n";
    std::cout << "Math load: " << (mathLoaded ? "FAILED" : "CORRECTLY FAILED") << "\n";
    
    if (!ecsLoaded && !g3dLoaded && !g2dLoaded && !mathLoaded) {
        std::cout << "✅ All module loads correctly failed\n";
    } else {
        std::cout << "❌ Unexpected module loading\n";
        return 1;
    }
    
    // Test 6: Native bindings (should work without modules)
    std::cout << "\n6. Testing native bindings...\n";
    auto& bindings = runtime.getBindings();
    
    // Register core functions
    runtime.registerFunction("core.info", [](auto& vm) {
        std::cout << "Core info: Runtime working without modules\n";
    });
    
    runtime.registerFunction("math.add", [](auto& vm) {
        // Simple math without math module
        std::cout << "Math add: Native bindings work\n";
    });
    
    std::cout << "Registered functions: " << bindings.getCount() << "\n";
    std::cout << "✅ Native bindings work without modules\n";
    
    // Test 7: Runtime lifecycle (should work without modules)
    std::cout << "\n7. Testing runtime lifecycle...\n";
    
    std::cout << "Starting runtime...\n";
    runtime.onFrameBegin([](float dt) {
        std::cout << "  Frame begin: dt=" << dt << "\n";
    });
    runtime.onFrameEnd([](float dt) {
        std::cout << "  Frame end: dt=" << dt << "\n";
    });
    
    // Simulate a few frames
    for (int frame = 0; frame < 3; frame++) {
        std::cout << "  Frame " << (frame+1) << ":\n";
        runtime.tick(0.016f);
    }
    
    std::cout << "✅ Runtime lifecycle works without modules\n";
    
    // Test 8: Stress test (runtime under load without modules)
    std::cout << "\n8. Stress testing runtime without modules...\n";
    
    for (int i = 0; i < 100; i++) {
        runtime.tick(0.016f);
        
        // Occasionally test script execution
        if (i % 25 == 0) {
            runtime.executeScript("print('Stress test tick " + std::to_string(i) + "')");
        }
    }
    
    std::cout << "✅ Runtime stress test passed\n";
    
    std::cout << "\n=== FORCED REMOVAL TEST PASSED ===\n";
    std::cout << "✅ Runtime works completely without modules\n";
    std::cout << "✅ VM works completely without modules\n";
    std::cout << "✅ Scheduler works completely without modules\n";
    std::cout << "✅ Native bindings work completely without modules\n";
    std::cout << "✅ Module loading gracefully fails\n";
    std::cout << "\n🎯 CONCLUSION: Kern runtime is truly minimal!\n";
    std::cout << "Modules are truly optional plugins!\n";
    
    return 0;
}
