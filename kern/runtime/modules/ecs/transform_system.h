/* *
 * kern/runtime/modules/ecs/transform_system.h - Transform System for ECS Module
 * 
 * Moved from engine core to ECS module.
 * Self-contained transform hierarchy system.
 */

#pragma once
#include "../../../engine/scene_graph/transform.h"
#include <unordered_map>
#include <vector>

namespace kern::runtime::ecs {

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSFORM SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════
//
// Manages transform hierarchy for ECS module.
// No dependency on core runtime.

class TransformSystem {
public:
    TransformSystem();
    ~TransformSystem() = default;
    
    // Non-copyable
    TransformSystem(const TransformSystem&) = delete;
    TransformSystem& operator=(const TransformSystem&) = delete;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // HIERARCHY MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Set parent of entity
    void setParent(EntityId child, EntityId parent);
    
    /// Remove entity from parent
    void unparent(EntityId child);
    
    /// Get parent entity
    EntityId getParent(EntityId entity) const;
    
    /// Get children of entity
    const std::vector<EntityId>& getChildren(EntityId entity) const;
    
    /// Check if entity is descendant of another
    bool isDescendantOf(EntityId potentialDescendant, EntityId ancestor) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // TRANSFORM OPERATIONS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Set world position (maintains world space)
    void setWorldPosition(EntityId entity, float x, float y, float z);
    
    /// Set world rotation
    void setWorldRotation(EntityId entity, float x, float y, float z, float w);
    
    /// Get world position
    void getWorldPosition(EntityId entity, float& x, float& y, float& z) const;
    
    /// Get world rotation
    void getWorldRotation(EntityId entity, float& x, float& y, float& z, float& w) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UPDATE
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Update all transforms
    /// Called by ECS module update
    void update();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // INTEGRATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Add transform component to entity
    /// Called by EntitySystem when entity is created
    kern::engine::Transform* addTransform(EntityId entity);
    
    /// Remove transform component
    /// Called by EntitySystem when entity is destroyed
    void removeTransform(EntityId entity);
    
    /// Get transform component
    kern::engine::Transform* getTransform(EntityId entity);
    
    const kern::engine::Transform* getTransform(EntityId entity) const;

private:
    // Transform components (owned by this system)
    std::unordered_map<EntityId, std::unique_ptr<kern::engine::Transform>> transforms_;
    
    // Hierarchy data
    struct HierarchyData {
        EntityId parent = kern::engine::INVALID_ENTITY;
        std::vector<EntityId> children;
    };
    std::unordered_map<EntityId, HierarchyData> hierarchy_;
    
    // Internal helpers
    void addChildToParent(EntityId child, EntityId parent);
    void removeChildFromParent(EntityId child, EntityId parent);
    void updateTransformRecursive(EntityId entity, const glm::mat4& parentWorldMatrix);
    
    // Validation
    void validateHierarchy() const;
};

} // namespace kern::runtime::ecs
