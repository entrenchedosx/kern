/* *
 * kern/runtime/modules/ecs/component_system.h - Component System for ECS Module
 * 
 * Moved from engine core to ECS module.
 * Self-contained component storage system.
 */

#pragma once
#include "component.h"
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace kern::runtime::ecs {

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════
//
// Manages all component types for ECS module.
// Uses the same sparse set storage as the original.

class ComponentSystem {
public:
    ComponentSystem();
    ~ComponentSystem() = default;
    
    // Non-copyable
    ComponentSystem(const ComponentSystem&) = delete;
    ComponentSystem& operator=(const ComponentSystem&) = delete;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COMPONENT MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Add component to entity
    template<typename T, typename... Args>
    T* add(EntityId entity, Args&&... args);
    
    /// Remove component from entity
    template<typename T>
    void remove(EntityId entity);
    
    /// Get component from entity
    template<typename T>
    T* get(EntityId entity);
    
    template<typename T>
    const T* get(EntityId entity) const;
    
    /// Check if entity has component
    template<typename T>
    bool has(EntityId entity) const;
    
    /// Remove all components from entity
    void removeAll(EntityId entity);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COMPONENT ITERATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Iterate over all entities with component T
    template<typename T, typename Func>
    void forEach(Func&& func);
    
    template<typename T, typename Func>
    void forEach(Func&& func) const;
    
    /// Query entities with multiple components
    template<typename... Components>
    std::vector<EntityId> query();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // STATISTICS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get count of components of type T
    template<typename T>
    size_t getCount() const;
    
    /// Get total component count across all types
    size_t getTotalCount() const;
    
    /// Check if empty
    bool empty() const;

private:
    // Component storages (type-erased)
    std::unordered_map<std::type_index, std::unique_ptr<kern::engine::IComponentStorage>> storages_;
    
    // Get or create storage for type T
    template<typename T>
    kern::engine::TypedComponentStorage<T>* getOrCreateStorage();
    
    template<typename T>
    const kern::engine::TypedComponentStorage<T>* getStorage() const;
};

// Template implementations
#include "component_system.inl"

} // namespace kern::runtime::ecs
