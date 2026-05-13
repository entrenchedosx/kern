/* *
 * kern/runtime/modules/ecs/entity_system.h - Entity System for ECS Module
 * 
 * Moved from engine core to ECS module.
 * Now self-contained, no dependency on runtime core.
 */

#pragma once
#include "entity.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace kern::runtime::ecs {

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════
//
// Wrapper around EntityRegistry for ECS module.
// Provides entity lifecycle management.

class EntitySystem {
public:
    EntitySystem();
    ~EntitySystem() = default;
    
    // Non-copyable
    EntitySystem(const EntitySystem&) = delete;
    EntitySystem& operator=(const EntitySystem&) = delete;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ENTITY LIFECYCLE
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Create a new entity
    EntityId create(const std::string& name = "Entity");
    
    /// Destroy an entity
    void destroy(EntityId entity);
    
    /// Check if entity is alive
    bool isAlive(EntityId entity) const;
    
    /// Get entity name
    const std::string& getName(EntityId entity) const;
    
    /// Set entity name
    void setName(EntityId entity, const std::string& name);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ITERATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Iterate over all alive entities
    template<typename Func>
    void forEach(Func&& func) const;
    
    /// Get count of alive entities
    size_t getCount() const { return registry_.aliveCount(); }
    
    /// Check if empty
    bool empty() const { return registry_.empty(); }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // INTERNAL ACCESS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get underlying registry (for component system)
    kern::engine::EntityRegistry& getRegistry() { return registry_; }
    const kern::engine::EntityRegistry& getRegistry() const { return registry_; }

private:
    kern::engine::EntityRegistry registry_;
    
    // Entity metadata (names)
    struct EntityData {
        std::string name;
    };
    std::unordered_map<EntityId, EntityData> entityData_;
};

// Template implementation
template<typename Func>
void EntitySystem::forEach(Func&& func) const {
    registry_.forEach([this, &func](EntityId entity) {
        func(entity);
    });
}

} // namespace kern::runtime::ecs
