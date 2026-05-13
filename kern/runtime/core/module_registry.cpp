/* *
 * kern/runtime/core/module_registry.cpp - Module Registry Implementation
 */

#include "module_registry.h"
#include <algorithm>
#include <iostream>

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE REGISTRY IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

ModuleRegistry::ModuleRegistry() {
    // Nothing needed
}

ModuleRegistry::~ModuleRegistry() {
    unloadAll();
}

bool ModuleRegistry::load(const std::string& name, KernRuntime* runtime) {
    if (isLoaded(name)) {
        return true;  // Already loaded
    }
    
    // Check if factory exists
    if (!isFactoryRegistered(name)) {
        std::cerr << "Module factory not found: " << name << "\n";
        return false;
    }
    
    // Create module instance
    auto module = createFromFactory(name);
    if (!module) {
        std::cerr << "Failed to create module: " << name << "\n";
        return false;
    }
    
    return load(std::move(module), runtime);
}

bool ModuleRegistry::load(std::unique_ptr<IModule> module, KernRuntime* runtime) {
    if (!module) {
        return false;
    }
    
    std::string name = module->getName();
    
    // Check dependencies
    std::vector<std::string> loadOrder;
    if (!resolveDependencies(name, loadOrder)) {
        std::cerr << "Failed to resolve dependencies for: " << name << "\n";
        return false;
    }
    
    // Load dependencies first
    for (const std::string& dep : loadOrder) {
        if (!isLoaded(dep)) {
            if (!load(dep, runtime)) {
                std::cerr << "Failed to load dependency: " << dep << "\n";
                return false;
            }
        }
    }
    
    // Initialize module
    if (!module->initialize(runtime)) {
        std::cerr << "Failed to initialize module: " << name << "\n";
        return false;
    }
    
    // Add to loaded modules
    modules_[name] = std::move(module);
    
    // Rebuild update order
    rebuildOrder();
    
    std::cout << "Loaded module: " << name << "\n";
    return true;
}

void ModuleRegistry::unload(const std::string& name) {
    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return;  // Not loaded
    }
    
    // Shutdown module
    it->second->shutdown();
    
    // Remove from modules
    modules_.erase(it);
    
    // Rebuild update order
    rebuildOrder();
    
    std::cout << "Unloaded module: " << name << "\n";
}

void ModuleRegistry::unloadAll() {
    // Shutdown in reverse order
    for (auto& [name, module] : modules_) {
        module->shutdown();
    }
    
    modules_.clear();
    updateOrder_.clear();
    renderOrder_.clear();
}

bool ModuleRegistry::isLoaded(const std::string& name) const {
    return modules_.find(name) != modules_.end();
}

IModule* ModuleRegistry::get(const std::string& name) const {
    auto it = modules_.find(name);
    if (it != modules_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ModuleRegistry::updateAll(float deltaTime) {
    for (IModule* module : updateOrder_) {
        if (module) {
            module->update(deltaTime);
        }
    }
}

void ModuleRegistry::fixedUpdateAll(float fixedDeltaTime) {
    for (IModule* module : updateOrder_) {
        if (module) {
            module->fixedUpdate(fixedDeltaTime);
        }
    }
}

void ModuleRegistry::renderAll() {
    for (IModule* module : renderOrder_) {
        if (module) {
            module->render();
        }
    }
}

std::vector<std::string> ModuleRegistry::getLoadedModuleNames() const {
    std::vector<std::string> names;
    names.reserve(modules_.size());
    
    for (const auto& [name, module] : modules_) {
        names.push_back(name);
    }
    
    return names;
}


bool ModuleRegistry::isFactoryRegistered(const std::string& name) {
    auto& registry = getFactoryRegistry();
    return registry.find(name) != registry.end();
}

std::unique_ptr<IModule> ModuleRegistry::createFromFactory(const std::string& name) {
    auto& registry = getFactoryRegistry();
    auto it = registry.find(name);
    if (it != registry.end()) {
        return it->second();
    }
    return nullptr;
}

std::unordered_map<std::string, ModuleFactory>& ModuleRegistry::getFactoryRegistry() {
    static std::unordered_map<std::string, ModuleFactory> registry;
    return registry;
}

void ModuleRegistry::rebuildOrder() {
    // Clear current order
    updateOrder_.clear();
    renderOrder_.clear();
    
    // Collect all modules
    std::vector<IModule*> modules;
    modules.reserve(modules_.size());
    
    for (const auto& [name, module] : modules_) {
        modules.push_back(module.get());
    }
    
    // Sort by update priority
    std::stable_sort(modules.begin(), modules.end(),
        [](const IModule* a, const IModule* b) {
            return a->getUpdatePriority() < b->getUpdatePriority();
        });
    
    updateOrder_ = modules;
    
    // Sort by render priority
    std::stable_sort(modules.begin(), modules.end(),
        [](const IModule* a, const IModule* b) {
            return a->getRenderPriority() < b->getRenderPriority();
        });
    
    renderOrder_ = modules;
}

bool ModuleRegistry::resolveDependencies(const std::string& name, std::vector<std::string>& outOrder) {
    // Simple dependency resolution - could be enhanced with proper graph algorithm
    // For now, just check if dependencies are available
    
    if (!isFactoryRegistered(name)) {
        return false;
    }
    
    auto module = createFromFactory(name);
    if (!module) {
        return false;
    }
    
    const char* deps = module->getDependencies();
    if (!deps) {
        outOrder.clear();
        return true;  // No dependencies
    }
    
    // Parse comma-separated dependencies
    std::string depStr(deps);
    std::vector<std::string> depsList;
    
    size_t start = 0;
    size_t end = depStr.find(',');
    while (end != std::string::npos) {
        depsList.push_back(depStr.substr(start, end - start));
        start = end + 1;
        end = depStr.find(',', start);
    }
    depsList.push_back(depStr.substr(start));
    
    // Check all dependencies are available
    for (const std::string& dep : depsList) {
        if (!isFactoryRegistered(dep)) {
            std::cerr << "Dependency not available: " << dep << "\n";
            return false;
        }
    }
    
    outOrder = depsList;
    return true;
}

} // namespace kern::runtime
