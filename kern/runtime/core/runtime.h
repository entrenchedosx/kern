/* *
 * kern/runtime/core/runtime.h - Kern Runtime Core
 * 
 * The central runtime container.
 * Owns: VM, Module Registry, Scheduler, Native Binding Layer
 * Does NOT own: ECS, graphics, or engine systems (those are modules)
 */

#pragma once

#include "module_registry.h"
#include "scheduler.h"
#include "../bindings/native_bindings.h"
#include "../../runtime/vm/vm.hpp"
#include <memory>
#include <string>
#include <functional>

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// KERN RUNTIME CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

struct RuntimeConfig {
    // VM Configuration
    size_t vmStackSize = 1024 * 1024;        // 1MB default stack
    size_t vmHeapSize = 64 * 1024 * 1024;    // 64MB default heap
    
    // Scheduler Configuration
    float targetFrameRate = 60.0f;           // Target FPS
    bool useFixedTimestep = false;            // Use fixed update
    float fixedTimestep = 1.0f / 60.0f;        // Fixed update interval
    
    // Debug Configuration
    bool enableDebug = false;                 // Enable debug hooks
    bool enableProfiling = false;             // Enable performance profiling
};

// ═══════════════════════════════════════════════════════════════════════════════
// KERN RUNTIME
// ═══════════════════════════════════════════════════════════════════════════════
//
// The central runtime instance.
// Minimal core - everything else is loaded as modules.

class KernRuntime {
public:
    // ═══════════════════════════════════════════════════════════════════════════
    // CONSTRUCTION / DESTRUCTION
    // ═══════════════════════════════════════════════════════════════════════════
    
    explicit KernRuntime(const RuntimeConfig& config = {});
    ~KernRuntime();
    
    // Non-copyable (owns VM and resources)
    KernRuntime(const KernRuntime&) = delete;
    KernRuntime& operator=(const KernRuntime&) = delete;
    
    // Movable (transfers VM ownership)
    KernRuntime(KernRuntime&&) = default;
    KernRuntime& operator=(KernRuntime&&) = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // VM ACCESS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get the central VM instance
    /// All execution goes through this VM
    VM& getVM() { return *vm_; }
    const VM& getVM() const { return *vm_; }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MODULE MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get module registry
    ModuleRegistry& getModules() { return modules_; }
    const ModuleRegistry& getModules() const { return modules_; }
    
    /// Load a module by name
    /// Convenience wrapper around ModuleRegistry::load
    bool loadModule(const std::string& name) {
        return modules_.load(name, this);
    }
    
    /// Load a module by instance
    bool loadModule(std::unique_ptr<IModule> module) {
        return modules_.load(std::move(module), this);
    }
    
    /// Get loaded module (typed)
    template<typename T>
    T* getModule() const {
        return modules_.get<T>();
    }
    
    /// Check if module is loaded
    bool isModuleLoaded(const std::string& name) const {
        return modules_.isLoaded(name);
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // NATIVE BINDINGS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get native binding layer
    NativeBindingLayer& getBindings() { return bindings_; }
    const NativeBindingLayer& getBindings() const { return bindings_; }
    
    /// Convenience: Register native function
    /// Example: runtime.registerFunction("math.sin", std::sin);
    template<typename Func>
    void registerFunction(const std::string& name, Func&& func) {
        bindings_.registerFunction(name, std::forward<Func>(func));
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // EXECUTION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Execute bytecode in VM
    /// @return Execution result
    VM::Result execute(const CodeObject& code);
    
    /// Execute script from source (compile then run)
    /// @return Execution result
    VM::Result executeScript(const std::string& source);
    
    /// Run REPL (interactive mode)
    void runREPL();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MAIN LOOP
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Run the runtime main loop
    /// Blocks until stop() is called
    void run();
    
    /// Stop the main loop
    void stop();
    
    /// Check if running
    bool isRunning() const { return running_; }
    
    /// Process single frame (for custom main loops)
    void tick(float deltaTime);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Set callback for frame begin
    void onFrameBegin(std::function<void(float)> callback);
    
    /// Set callback for frame end
    void onFrameEnd(std::function<void(float)> callback);

private:
    // Core components (minimal)
    std::unique_ptr<VM> vm_;                          // Central VM
    ModuleRegistry modules_;                          // Loaded modules
    Scheduler scheduler_;                             // Update loop
    NativeBindingLayer bindings_;                     // C++ interop
    
    // Configuration
    RuntimeConfig config_;
    
    // State
    bool running_ = false;
    bool initialized_ = false;
    
    // Callbacks
    std::function<void(float)> onFrameBegin_;
    std::function<void(float)> onFrameEnd_;
    
    // Internal
    void initialize();
    void shutdown();
};

} // namespace kern::runtime
