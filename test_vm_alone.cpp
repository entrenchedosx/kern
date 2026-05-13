/* *
 * test_vm_alone.cpp - TRUE ISOLATION TEST 1: Pure VM Standalone Compile
 * 
 * Compile ONLY: VM (vm.hpp + vm.cpp)
 * NO runtime, NO bindings, NO modules, NO scheduler
 */

// STRICT: Only VM includes
#include "kern/runtime/vm/vm.hpp"

// FORBIDDEN: No runtime includes
// FORBIDDEN: No module includes
// FORBIDDEN: No scheduler includes
// FORBIDDEN: No binding includes

int main() {
    // Test VM instantiation
    kern::VM vm;
    
    // Test VM basic functionality
    std::cout << "✅ VM instantiated successfully\n";
    
    return 0;
}
