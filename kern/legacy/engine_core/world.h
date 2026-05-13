/* *
 * kern/engine/core/world.h - World Container
 * 
 * Week 2 Production Implementation
 * 
 * The World is the OWNER of all entities and components.
 * It provides:
 * - Entity lifecycle management
 * - Component storage and access
 * - Scene graph hierarchy
 * - Transform propagation
 * 
 * DATA CONTRACT: World is the SINGLE SOURCE OF TRUTH
 */

#pragma once

#include "entity.h"
#include "component.h"
#include "../scene_graph/transform.h"
#include "../kern_engine_contracts.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <typeindex>

namespace kern::engine {

// ═══════════════════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════════

class World;
class System;

// ═══════════════════════════════════════════════════════════════════════════════
// WORLD CLASS - Scene Container
// ═══════════════════════════════════════════════════════════════════════════════

class World {
public:
    // ═══════════════════════════════════════════════════════════════════════════
    // CONSTRUCTION / DESTRUCTION
    // ═══════════════════════════════════════════════════════════════════════════
    
    explicit World(const std::string& name = "Untitled");
    ~World();
    
    // Non-copyable (entities are tied to this world)
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    
    // Movable (transfers entity ownership)
    World(World&&) = default;
    World& operator=(World&&) = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ENTITY LIFECYCLE (Delegation to EntityRegistry)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Create a new entity in this world
     * Automatically adds Transform component
     */
    EntityId createEntity(const std::string& name = "Entity");
    
    /**
     * Destroy an entity and all its components
     * Automatically unparents and removes from hierarchy
     */
    void destroyEntity(EntityId entity);
    
    /**
     * Check if entity is alive in this world
     */
    bool isAlive(EntityId entity) const {
        return entities_.isAlive(entity);
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COMPONENT MANAGEMENT (Template-based)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Add component to entity
     * Returns pointer to newly created component
     * O(1) amortized
     */
    template<typename T, typename... Args>
    T* addComponent(EntityId entity, Args&&... args);
    
    /**
     * Remove component from entity
     * O(1)
     */
    template<typename T>
    void removeComponent(EntityId entity);
    
    /**
     * Get component from entity
     * Returns nullptr if entity doesn't have component
     * O(1)
     */
    template<typename T>
    T* getComponent(EntityId entity);
    
    template<typename T>
    const T* getComponent(EntityId entity) const;
    
    /**
     * Check if entity has component
     * O(1)
     */
    template<typename T>
    bool hasComponent(EntityId entity) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COMPONENT ITERATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Iterate over all entities with Component T
     * Usage: for (auto [entity, comp] : world.query<Transform>()) { ... }
     */
    template<typename T>
    typename ComponentStorage<T>::Range query();
    
    template<typename T>
    typename ComponentStorage<T>::Range query() const;
    
    /**
     * Execute function for each entity with Component T
     */
    template<typename T, typename Func>
    void forEach(Func&& func);
    
    /**
     * Query multiple components (inner join)
     * Returns entities that have ALL specified components
     */
    template<typename... Components>
    std::vector<EntityId> queryEntities();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // SCENE GRAPH HIERARCHY
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Set parent of an entity
     * Automatically updates transform to maintain world position
     */
    void setParent(EntityId child, EntityId parent);
    
    /**
     * Remove entity from its parent
     * Maintains world position
     */
    void unparent(EntityId child);
    
    /**
     * Get parent entity
     */
    EntityId getParent(EntityId entity) const;
    
    /**
     * Get children of entity
     */
    const std::vector<EntityId>& getChildren(EntityId entity) const;
    
    /**
     * Check if entity is a descendant of another
     * (Prevents circular parent relationships)
     */
    bool isDescendantOf(EntityId potentialDescendant, EntityId ancestor) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // TRANSFORM OPERATIONS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Set world position (automatically calculates local position)
     */
    void setWorldPosition(EntityId entity, const glm::vec3& position);
    
    /**
     * Set world rotation (automatically calculates local rotation)
     */
    void setWorldRotation(EntityId entity, const glm::quat& rotation);
    
    /**
     * Get world position
     */
    glm::vec3 getWorldPosition(EntityId entity) const;
    
    /**
     * Get world rotation
     */
    glm::quat getWorldRotation(EntityId entity) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // WORLD UPDATE (Per-frame)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Initialize world (called once on load)
     */
    void start();
    
    /**
     * Update world (call once per frame)
     * - Updates transform hierarchy
     * - Updates all systems
     */
    void update(float deltaTime);
    
    /**
     * Shutdown world (cleanup)
     */
    void shutdown();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // SYSTEMS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Add a system to the world
     * Systems are updated in priority order
     */
    void addSystem(std::unique_ptr<System> system);
    
    /**
     * Remove all systems
     */
    void clearSystems();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ACCESSORS
    // ═══════════════════════════════════════════════════════════════════════════
    
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    
    EntityRegistry& getEntityRegistry() { return entities_; }
    const EntityRegistry& getEntityRegistry() const { return entities_; }
    
    size_t entityCount() const { return entities_.aliveCount(); }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // INTERNAL - Transform Update
    // ═══════════════════════════════════════════════════════════════════════════
    
    void updateTransforms();

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // INTERNAL DATA
    // ═══════════════════════════════════════════════════════════════════════════
    
    std::string name_;
    
    // Entity ownership
    EntityRegistry entities_;
    
    // Component storages (type-erased)
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> componentStorages_;
    
    // Entity metadata (names, hierarchy)
    struct EntityData {
        std::string name;
        EntityId parent = INVALID_ENTITY;
        std::vector<EntityId> children;
    };
    std::unordered_map<EntityId, EntityData> entityData_;
    
    // Systems
    std::vector<std::unique_ptr<System>> systems_;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // INTERNAL HELPERS
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Get or create component storage for type T
    template<typename T>
    TypedComponentStorage<T>* getOrCreateStorage();
    
    template<typename T>
    const TypedComponentStorage<T>* getStorage() const;
    
    // Hierarchy helpers
    void addChildToParent(EntityId child, EntityId parent);
    void removeChildFromParent(EntityId child, EntityId parent);
    
    // Transform helpers
    void updateTransformRecursive(EntityId entity, const glm::mat4& parentWorldMatrix);
    
    // Validation
    void validateHierarchy() const;
};

// ═══════════════════════════════════════════════════════════════════════════════
// SYSTEM BASE CLASS
// ═══════════════════════════════════════════════════════════════════════════════

class System {
public:
    virtual ~System() = default;
    
    // Called once when system is added to world
    virtual void onStart(World& world) {}
    
    // Called every frame
    virtual void update(World& world, float deltaTime) = 0;
    
    // Called when system is removed
    virtual void onStop(World& world) {}
    
    // Priority for update order (lower = earlier)
    int priority = 0;
    
    // Is system enabled?
    bool enabled = true;
};

// ═══════════════════════════════════════════════════════════════════════════════
// TEMPLATE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════════════

// Include template implementations
#include "world.inl"

} // namespace kern::engine
