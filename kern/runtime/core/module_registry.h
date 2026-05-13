/* *
 * kern/runtime/core/module_registry.h - Module Registry
 * 
 * Manages loaded modules, their lifecycle, and update scheduling.
 */

#pragma once

#include "../modules/module.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

namespace kern {
    class KernRuntime;
}

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE REGISTRY
// ═══════════════════════════════════════════════════════════════════════════════
//
// Central registry for all loaded modules.
// Handles loading, initialization, update scheduling, and shutdown.

class ModuleRegistry {
public:
    ModuleRegistry();
    ~ModuleRegistry();
    
    // Non-copyable (owns module instances)
    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MODULE LOADING
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Load a module by name
    /// Module must be registered via KERN_REGISTER_MODULE macro
    /// @return true if loaded and initialized successfully
    bool load(const std::string& name, KernRuntime* runtime);
    
    /// Load a module by factory function
    /// @return true if loaded and initialized successfully
    bool load(std::unique_ptr<IModule> module, KernRuntime* runtime);
    
    /// Unload a module by name
    void unload(const std::string& name);
    
    /// Unload all modules
    void unloadAll();
    
    /// Check if module is loaded
    bool isLoaded(const std::string& name) const;
    
    /// Get loaded module
    /// @return nullptr if not loaded
    IModule* get(const std::string& name) const;
    
    /// Get loaded module (typed)
    template<typename T>
    T* get() const {
        for (const auto& [name, module] : modules_) {
            T* casted = dynamic_cast<T*>(module.get());
            if (casted) return casted;
        }
        return nullptr;
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MODULE FACTORIES (Registration)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Register a module factory (called by KERN_REGISTER_MODULE macro)
    static void registerFactory(const std::string& name, ModuleFactory factory);
    
    /// Check if a module factory is registered
    static bool isFactoryRegistered(const std::string& name);
    
    /// Create module from registered factory
    static std::unique_ptr<IModule> createFromFactory(const std::string& name);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UPDATE SCHEDULING
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Update all loaded modules
    /// Called by scheduler every frame
    void updateAll(float deltaTime);
    
    /// Fixed update all loaded modules
    void fixedUpdateAll(float fixedDeltaTime);
    
    /// Render all loaded modules
    void renderAll();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // QUERY
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get list of loaded module names
    std::vector<std::string> getLoadedModuleNames() const;
    
    /// Get count of loaded modules
    size_t getLoadedCount() const { return modules_.size(); }
    
    /// Iterate over all loaded modules
    template<typename Func>
    void forEach(Func&& func) const;

private:
    // Loaded modules (name → module instance)
    std::unordered_map<std::string, std::unique_ptr<IModule>> modules_;
    
    // Update order (sorted by priority)
    std::vector<IModule*> updateOrder_;
    std::vector<IModule*> renderOrder_;
    
    // Static registry of module factories
    static std::unordered_map<std::string, ModuleFactory>& getFactoryRegistry();
    
    // Rebuild update/render order when modules change
    void rebuildOrder();
    
    // Resolve dependencies
    bool resolveDependencies(const std::string& name, std::vector<std::string>& outOrder);
};

// Template implementation
#include "module_registry.inl"

} // namespace kern::runtime
