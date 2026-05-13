/* *
 * kern/runtime/modules/ecs/ecs_module.h - ECS Runtime Module
 * 
 * ECS (Entity Component System) as a runtime module.
 * Moved from core to module - now optional, not required.
 */

#pragma once

#include "../module.h"
#include "entity_system.h"
#include "component_system.h"
#include "transform_system.h"
#include <memory>

namespace kern::runtime {
    class KernRuntime;
}

namespace kern::runtime::ecs {

// ═══════════════════════════════════════════════════════════════════════════════
// ECS MODULE
// ═══════════════════════════════════════════════════════════════════════════════
//
// Optional module providing ECS functionality.
// Can be loaded/unloaded at runtime.

class ECSModule : public IModule {
public:
    ECSModule();
    ~ECSModule() override;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // IModule Interface
    // ═══════════════════════════════════════════════════════════════════════════
    
    const char* getName() const override { return "ecs"; }
    const char* getVersion() const override { return "1.0.0"; }
    const char* getDependencies() const override { return "math"; }  // Requires math module
    
    bool initialize(KernRuntime* runtime) override;
    void shutdown() override;
    
    void update(float deltaTime) override;
    void fixedUpdate(float fixedDeltaTime) override;
    
    int getUpdatePriority() const override { return 50; }  // Early update
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ECS API
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Create an entity
    EntityId createEntity(const std::string& name = "Entity");
    
    /// Destroy an entity
    void destroyEntity(EntityId entity);
    
    /// Check if entity is alive
    bool isAlive(EntityId entity) const;
    
    /// Add component to entity
    template<typename T, typename... Args>
    T* addComponent(EntityId entity, Args&&... args);
    
    /// Remove component from entity
    template<typename T>
    void removeComponent(EntityId entity);
    
    /// Get component from entity
    template<typename T>
    T* getComponent(EntityId entity);
    
    /// Check if entity has component
    template<typename T>
    bool hasComponent(EntityId entity) const;
    
    /// Set entity parent
    void setParent(EntityId child, EntityId parent);
    
    /// Remove entity from parent
    void unparent(EntityId child);
    
    /// Get entity parent
    EntityId getParent(EntityId entity) const;
    
    /// Get entity children
    const std::vector<EntityId>& getChildren(EntityId entity) const;
    
    /// Set world position (maintains world space)
    void setWorldPosition(EntityId entity, float x, float y, float z);
    
    /// Set world rotation
    void setWorldRotation(EntityId entity, float x, float y, float z, float w);
    
    /// Get world position
    void getWorldPosition(EntityId entity, float& x, float& y, float& z) const;
    
    /// Query entities with component
    template<typename T, typename Func>
    void forEach(Func&& func);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // INTERNAL ACCESS
    // ═══════════════════════════════════════════════════════════════════════════
    
    EntitySystem& getEntitySystem() { return *entitySystem_; }
    TransformSystem& getTransformSystem() { return *transformSystem_; }

private:
    std::unique_ptr<EntitySystem> entitySystem_;
    std::unique_ptr<ComponentSystem> componentSystem_;
    std::unique_ptr<TransformSystem> transformSystem_;
    
    KernRuntime* runtime_ = nullptr;
    
    // Native binding registration
    void registerBindings();
    void unregisterBindings();
};

// Template implementations
#include "ecs_module.inl"

} // namespace kern::runtime::ecs
