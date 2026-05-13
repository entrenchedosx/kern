/* *
 * kern/runtime/modules/module.h - Module Interface
 * 
 * All runtime modules must implement this interface.
 * Modules are optional plugins loaded at runtime.
 */

#pragma once

#include <string>
#include <memory>

// Forward declarations
namespace kern {
    class KernRuntime;
    class VM;
}

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════
//
// All modules must implement this interface.
// Modules are self-contained and register their own bindings.

class IModule {
public:
    virtual ~IModule() = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MODULE METADATA
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Unique module name (e.g., "ecs", "g3d", "io")
    virtual const char* getName() const = 0;
    
    /// Module version (semantic versioning)
    virtual const char* getVersion() const = 0;
    
    /// Module dependencies (comma-separated names, or nullptr for none)
    /// Example: "math,io" - this module requires math and io modules
    virtual const char* getDependencies() const { return nullptr; }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // LIFECYCLE
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Called when module is loaded into runtime
    /// Register all native bindings here
    /// @return true if initialization succeeded
    virtual bool initialize(KernRuntime* runtime) = 0;
    
    /// Called when module is unloaded
    /// Clean up all resources
    virtual void shutdown() = 0;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UPDATE LOOP
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Called every frame by scheduler
    /// @param deltaTime Time since last update in seconds
    virtual void update(float deltaTime) {}
    
    /// Called at fixed timestep (for physics, etc.)
    /// @param fixedDeltaTime Fixed timestep in seconds
    virtual void fixedUpdate(float fixedDeltaTime) {}
    
    /// Render hook (for graphics modules)
    virtual void render() {}
    
    // ═══════════════════════════════════════════════════════════════════════════
    // PRIORITY
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Update priority (lower = earlier in frame)
    /// Default: 100 (middle priority)
    virtual int getUpdatePriority() const { return 100; }
    
    /// Render priority (lower = earlier render)
    virtual int getRenderPriority() const { return 100; }
};

// Module factory function type
using ModuleFactory = std::unique_ptr<IModule>(*)();

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE REGISTRATION MACRO
// ═══════════════════════════════════════════════════════════════════════════════
//
// Use this in module .cpp files to auto-register with the system:
//
//   class ECSModule : public IModule { ... };
//   KERN_REGISTER_MODULE(ECSModule, "ecs")
//
// This creates the factory function and metadata.

#define KERN_REGISTER_MODULE(ClassName, moduleName) \
    namespace kern::runtime::module_registry { \
        std::unique_ptr<IModule> create_##moduleName() { \
            return std::make_unique<ClassName>(); \
        } \
        struct AutoReg_##moduleName { \
            AutoReg_##moduleName() { \
                ModuleRegistry::registerFactory(moduleName, create_##moduleName); \
            } \
        } autoReg_##moduleName; \
    }

} // namespace kern::runtime
