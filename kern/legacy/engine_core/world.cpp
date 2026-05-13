/* *
 * kern/engine/core/world.cpp - World Implementation
 * 
 * Week 2 Production Implementation
 */

#include "world.h"
#include <algorithm>
#include <cassert>

namespace kern::engine {

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTION / DESTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

World::World(const std::string& name) : name_(name) {
    // Nothing special needed - default construction handles everything
}

World::~World() {
    // Clean shutdown
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

EntityId World::createEntity(const std::string& name) {
    // Create entity in registry
    EntityId entity = entities_.create();
    
    // Initialize entity data
    entityData_[entity] = EntityData{
        name,
        INVALID_ENTITY,  // No parent
        {}              // No children
    };
    
    // Add Transform component (every entity has one)
    addComponent<Transform>(entity);
    
    return entity;
}

void World::destroyEntity(EntityId entity) {
    if (!entities_.isAlive(entity)) {
        return;
    }
    
    // Remove from hierarchy first
    unparent(entity);
    
    // Destroy all children recursively
    auto it = entityData_.find(entity);
    if (it != entityData_.end()) {
        // Copy children list because we'll be modifying it
        std::vector<EntityId> children = it->second.children;
        for (EntityId child : children) {
            destroyEntity(child);
        }
    }
    
    // Remove all components
    for (auto& [typeId, storage] : componentStorages_) {
        storage->remove(entity);
    }
    
    // Remove entity data
    entityData_.erase(entity);
    
    // Destroy entity in registry
    entities_.destroy(entity);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SCENE GRAPH HIERARCHY
// ═══════════════════════════════════════════════════════════════════════════════

void World::setParent(EntityId child, EntityId parent) {
    // Validate entities
    if (!entities_.isAlive(child)) {
        return;
    }
    if (!entities_.isAlive(parent)) {
        return;
    }
    
    // Prevent self-parenting
    if (child == parent) {
        return;
    }
    
    // Prevent circular relationships
    if (isDescendantOf(parent, child)) {
        return;  // Cannot parent to a descendant
    }
    
    // Get current parent
    auto childIt = entityData_.find(child);
    if (childIt == entityData_.end()) {
        return;
    }
    
    EntityId oldParent = childIt->second.parent;
    
    // If already has this parent, nothing to do
    if (oldParent == parent) {
        return;
    }
    
    // Remove from old parent
    if (entities_.isAlive(oldParent)) {
        removeChildFromParent(child, oldParent);
    }
    
    // Add to new parent
    addChildToParent(child, parent);
    
    // Update parent reference
    childIt->second.parent = parent;
    
    // Get transforms
    Transform* childTrans = getComponent<Transform>(child);
    Transform* parentTrans = getComponent<Transform>(parent);
    
    if (childTrans && parentTrans) {
        // Save current world position
        glm::vec3 worldPos = childTrans->getWorldPosition();
        glm::quat worldRot = childTrans->getWorldRotation();
        
        // Set parent in transform
        childTrans->parent = parent;
        
        // Mark dirty so it recalculates
        childTrans->markDirty();
        
        // Calculate new local position to maintain world position
        // local = inverse(parentWorld) * world
        glm::mat4 invParentWorld = glm::inverse(parentTrans->worldMatrix);
        childTrans->localPosition = glm::vec3(invParentWorld * glm::vec4(worldPos, 1.0f));
        
        // Calculate new local rotation
        childTrans->localRotation = glm::inverse(parentTrans->getWorldRotation()) * worldRot;
    }
}

void World::unparent(EntityId child) {
    if (!entities_.isAlive(child)) {
        return;
    }
    
    auto childIt = entityData_.find(child);
    if (childIt == entityData_.end()) {
        return;
    }
    
    EntityId oldParent = childIt->second.parent;
    if (!entities_.isAlive(oldParent)) {
        childIt->second.parent = INVALID_ENTITY;
        return;
    }
    
    Transform* childTrans = getComponent<Transform>(child);
    Transform* parentTrans = getComponent<Transform>(oldParent);
    
    if (childTrans) {
        // Save current world transform
        glm::vec3 worldPos = childTrans->getWorldPosition();
        glm::quat worldRot = childTrans->getWorldRotation();
        
        // Remove from parent
        removeChildFromParent(child, oldParent);
        childIt->second.parent = INVALID_ENTITY;
        childTrans->parent = INVALID_ENTITY;
        
        // Set local to match previous world (now it's root)
        childTrans->localPosition = worldPos;
        childTrans->localRotation = worldRot;
        childTrans->markDirty();
    }
}

EntityId World::getParent(EntityId entity) const {
    auto it = entityData_.find(entity);
    if (it != entityData_.end()) {
        return it->second.parent;
    }
    return INVALID_ENTITY;
}

const std::vector<EntityId>& World::getChildren(EntityId entity) const {
    static const std::vector<EntityId> empty;
    
    auto it = entityData_.find(entity);
    if (it != entityData_.end()) {
        return it->second.children;
    }
    return empty;
}

bool World::isDescendantOf(EntityId potentialDescendant, EntityId ancestor) const {
    if (potentialDescendant == ancestor) {
        return false;
    }
    
    EntityId current = getParent(potentialDescendant);
    while (entities_.isAlive(current)) {
        if (current == ancestor) {
            return true;
        }
        current = getParent(current);
    }
    
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HIERARCHY HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void World::addChildToParent(EntityId child, EntityId parent) {
    auto parentIt = entityData_.find(parent);
    if (parentIt != entityData_.end()) {
        parentIt->second.children.push_back(child);
    }
}

void World::removeChildFromParent(EntityId child, EntityId parent) {
    auto parentIt = entityData_.find(parent);
    if (parentIt != entityData_.end()) {
        auto& children = parentIt->second.children;
        auto it = std::find(children.begin(), children.end(), child);
        if (it != children.end()) {
            children.erase(it);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSFORM OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

void World::setWorldPosition(EntityId entity, const glm::vec3& position) {
    Transform* trans = getComponent<Transform>(entity);
    if (!trans) {
        return;
    }
    
    EntityId parent = getParent(entity);
    if (!entities_.isAlive(parent)) {
        // No parent - local = world
        trans->setLocalPosition(position);
    } else {
        Transform* parentTrans = getComponent<Transform>(parent);
        if (parentTrans) {
            trans->setWorldPosition(position, parentTrans->worldMatrix);
        }
    }
}

void World::setWorldRotation(EntityId entity, const glm::quat& rotation) {
    Transform* trans = getComponent<Transform>(entity);
    if (!trans) {
        return;
    }
    
    EntityId parent = getParent(entity);
    if (!entities_.isAlive(parent)) {
        // No parent - local = world
        trans->setLocalRotation(rotation);
    } else {
        Transform* parentTrans = getComponent<Transform>(parent);
        if (parentTrans) {
            trans->setWorldRotation(rotation, parentTrans->worldMatrix);
        }
    }
}

glm::vec3 World::getWorldPosition(EntityId entity) const {
    const Transform* trans = getComponent<Transform>(entity);
    if (trans) {
        return trans->getWorldPosition();
    }
    return glm::vec3(0.0f);
}

glm::quat World::getWorldRotation(EntityId entity) const {
    const Transform* trans = getComponent<Transform>(entity);
    if (trans) {
        return trans->getWorldRotation();
    }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
// WORLD UPDATE
// ═══════════════════════════════════════════════════════════════════════════════

void World::start() {
    // Initialize all systems
    for (auto& system : systems_) {
        if (system->enabled) {
            system->onStart(*this);
        }
    }
    
    // Initial transform update
    updateTransforms();
}

void World::update(float deltaTime) {
    // Update transform hierarchy
    updateTransforms();
    
    // Sort systems by priority
    std::stable_sort(systems_.begin(), systems_.end(),
        [](const auto& a, const auto& b) {
            return a->priority < b->priority;
        });
    
    // Update all systems
    for (auto& system : systems_) {
        if (system->enabled) {
            system->update(*this, deltaTime);
        }
    }
}

void World::shutdown() {
    // Stop all systems
    for (auto& system : systems_) {
        system->onStop(*this);
    }
    systems_.clear();
    
    // Destroy all entities (clears components too)
    entities_.clear();
    entityData_.clear();
    componentStorages_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSFORM UPDATE (HIERARCHY PROPAGATION)
// ═══════════════════════════════════════════════════════════════════════════════

void World::updateTransforms() {
    // Find all root entities (no parent) and update recursively
    entities_.forEach([this](EntityId entity) {
        auto it = entityData_.find(entity);
        if (it != entityData_.end() && it->second.parent == INVALID_ENTITY) {
            // This is a root entity
            updateTransformRecursive(entity, glm::mat4(1.0f));
        }
    });
}

void World::updateTransformRecursive(EntityId entity, const glm::mat4& parentWorldMatrix) {
    Transform* trans = getComponent<Transform>(entity);
    if (!trans) {
        return;
    }
    
    // Update this entity's world matrix
    if (trans->isDirty() || trans->parent != INVALID_ENTITY) {
        trans->updateWorldMatrix(parentWorldMatrix);
    }
    
    // Recursively update children
    auto it = entityData_.find(entity);
    if (it != entityData_.end()) {
        for (EntityId child : it->second.children) {
            updateTransformRecursive(child, trans->worldMatrix);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SYSTEMS
// ═══════════════════════════════════════════════════════════════════════════════

void World::addSystem(std::unique_ptr<System> system) {
    systems_.push_back(std::move(system));
}

void World::clearSystems() {
    for (auto& system : systems_) {
        system->onStop(*this);
    }
    systems_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// VALIDATION
// ═══════════════════════════════════════════════════════════════════════════════

void World::validateHierarchy() const {
    #ifndef NDEBUG
    for (const auto& [entity, data] : entityData_) {
        // Validate parent exists
        if (data.parent != INVALID_ENTITY) {
            if (!entities_.isAlive(data.parent)) {
                assert(false && "Orphaned entity - parent is dead");
            }
            
            // Validate we're in parent's children list
            auto parentIt = entityData_.find(data.parent);
            if (parentIt != entityData_.end()) {
                const auto& siblings = parentIt->second.children;
                bool found = std::find(siblings.begin(), siblings.end(), entity) != siblings.end();
                assert(found && "Entity not in parent's children list");
            }
        }
        
        // Validate all children exist
        for (EntityId child : data.children) {
            if (!entities_.isAlive(child)) {
                assert(false && "Dead child in children list");
            }
            
            auto childIt = entityData_.find(child);
            if (childIt != entityData_.end()) {
                assert(childIt->second.parent == entity && "Child has wrong parent");
            }
        }
    }
    #endif
}

} // namespace kern::engine
