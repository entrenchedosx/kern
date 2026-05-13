/* *
 * kern/engine/core/component.h - Component Storage System
 * 
 * DATA CONTRACT: Component Contract (STORAGE RULE)
 * - POD-only (no virtuals, no inheritance required)
 * - Stored in contiguous arrays (sparse set)
 * - Access only via World
 * 
 * Week 2 Production Implementation
 */

#pragma once

#include "entity.h"
#include "kern_engine_contracts.h"
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <cassert>
#include <memory>
#include <functional>

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT TYPE IDENTIFICATION
// ═══════════════════════════════════════════════════════════════════════════════

namespace kern::engine {

using ComponentTypeId = uint32_t;
constexpr ComponentTypeId INVALID_COMPONENT_TYPE = 0;

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT BASE (Optional - for type safety)
// ═══════════════════════════════════════════════════════════════════════════════

// Empty base class for component type identification
// Components can inherit from this for type safety, but it's not required
struct ComponentBase {
    // Components should be POD - no virtual functions needed
    // This is just for type tagging
};

// ═══════════════════════════════════════════════════════════════════════════════
// SPARSE SET COMPONENT STORAGE
// ═══════════════════════════════════════════════════════════════════════════════
//
// Core data structure for O(1) component lookup and cache-friendly iteration.
// 
// Memory layout:
//   entityToComponent_[entityIndex] = componentIndex (or -1 if no component)
//   componentToEntity_[componentIndex] = entityIndex
//   components_[componentIndex] = actual component data
//
// This gives:
// - O(1) lookup by entity: components_[entityToComponent_[entityIdx]]
// - O(1) iteration over all components: for (comp : components_)
// - O(1) check if entity has component: entityToComponent_[entityIdx] != -1

template<typename T>
class ComponentStorage {
    static_assert(std::is_standard_layout_v<T>, 
                  "Component must be standard layout (POD)");
    static_assert(std::is_trivially_destructible_v<T> || 
                  std::is_base_of_v<ComponentBase, T>,
                  "Component must be trivially destructible or inherit from ComponentBase");

public:
    // Iterator over all valid (entity, component) pairs
    class Iterator {
    public:
        using ComponentRef = T&;
        using EntityRef = EntityId;
        
        Iterator(ComponentStorage& storage, size_t index)
            : storage_(storage), index_(index) {
            advanceToValid();
        }
        
        std::pair<EntityId, T&> operator*() {
            return {
                storage_.componentToEntity_[index_],
                storage_.components_[index_]
            };
        }
        
        Iterator& operator++() {
            ++index_;
            advanceToValid();
            return *this;
        }
        
        bool operator!=(const Iterator& other) const {
            return index_ != other.index_;
        }
        
    private:
        void advanceToValid() {
            while (index_ < storage_.components_.size() && 
                   !storage_.alive_[index_]) {
                ++index_;
            }
        }
        
        ComponentStorage& storage_;
        size_t index_;
    };

    // Range for range-based for loops
    struct Range {
        ComponentStorage& storage;
        Iterator begin() { return Iterator(storage, 0); }
        Iterator end() { return Iterator(storage, storage.components_.size()); }
    };

public:
    // ═══════════════════════════════════════════════════════════════════════════
    // CORE OPERATIONS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Check if entity has this component
     * O(1) - array lookup
     */
    bool has(EntityId entity) const {
        uint32_t index = entity_id::getIndex(entity);
        if (index >= entityToComponent_.size()) {
            return false;
        }
        int32_t compIdx = entityToComponent_[index];
        if (compIdx < 0) {
            return false;
        }
        // Verify generation (entity may have been recycled)
        return storage_.alive_[static_cast<size_t>(compIdx)];
    }
    
    /**
     * Get component by entity
     * Returns nullptr if entity doesn't have component
     * O(1) - array lookup
     */
    T* get(EntityId entity) {
        uint32_t index = entity_id::getIndex(entity);
        if (index >= entityToComponent_.size()) {
            return nullptr;
        }
        int32_t compIdx = entityToComponent_[index];
        if (compIdx < 0) {
            return nullptr;
        }
        size_t ci = static_cast<size_t>(compIdx);
        if (!alive_[ci]) {
            return nullptr;
        }
        return &components_[ci];
    }
    
    const T* get(EntityId entity) const {
        return const_cast<ComponentStorage*>(this)->get(entity);
    }
    
    /**
     * Add component to entity
     * Returns pointer to newly created component (already constructed)
     * O(1) amortized
     */
    template<typename... Args>
    T* add(EntityId entity, Args&&... args) {
        uint32_t index = entity_id::getIndex(entity);
        
        // Ensure entityToComponent_ is large enough
        if (index >= entityToComponent_.size()) {
            entityToComponent_.resize(index + 1, -1);
        }
        
        // Check if already exists
        if (entityToComponent_[index] >= 0) {
            // Already has component - return existing (or could assert)
            size_t ci = static_cast<size_t>(entityToComponent_[index]);
            return &components_[ci];
        }
        
        // Allocate new component slot
        size_t compIndex;
        if (freeListHead_ >= 0) {
            // Reuse from free list
            compIndex = static_cast<size_t>(freeListHead_);
            freeListHead_ = freeListNext_[compIndex];
        } else {
            // Allocate new
            compIndex = components_.size();
            components_.emplace_back();
            componentToEntity_.push_back(INVALID_ENTITY);
            alive_.push_back(false);
            freeListNext_.push_back(-1);
        }
        
        // Initialize component
        new (&components_[compIndex]) T(std::forward<Args>(args)...);
        componentToEntity_[compIndex] = entity;
        alive_[compIndex] = true;
        
        // Link entity to component
        entityToComponent_[index] = static_cast<int32_t>(compIndex);
        
        ++size_;
        return &components_[compIndex];
    }
    
    /**
     * Remove component from entity
     * O(1) - swaps with end for dense arrays
     */
    void remove(EntityId entity) {
        uint32_t index = entity_id::getIndex(entity);
        if (index >= entityToComponent_.size()) {
            return;
        }
        
        int32_t compIdx = entityToComponent_[index];
        if (compIdx < 0) {
            return;  // No component
        }
        
        size_t ci = static_cast<size_t>(compIdx);
        if (!alive_[ci]) {
            return;  // Already dead
        }
        
        // Call destructor if needed
        if constexpr (!std::is_trivially_destructible_v<T>) {
            components_[ci].~T();
        }
        
        // Mark as dead
        alive_[ci] = false;
        entityToComponent_[index] = -1;
        
        // Add to free list
        freeListNext_[ci] = freeListHead_;
        freeListHead_ = static_cast<int32_t>(ci);
        
        --size_;
    }
    
    /**
     * Remove all components (for entity destruction)
     */
    void clear() {
        // Call destructors
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < components_.size(); ++i) {
                if (alive_[i]) {
                    components_[i].~T();
                }
            }
        }
        
        components_.clear();
        entityToComponent_.clear();
        componentToEntity_.clear();
        alive_.clear();
        freeListNext_.clear();
        freeListHead_ = -1;
        size_ = 0;
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ITERATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Iterate over all (entity, component) pairs
     * Cache-friendly: linear scan through dense array
     * Usage: for (auto [entity, comp] : storage.all()) { ... }
     */
    Range all() {
        return Range{*this};
    }
    
    /**
     * Execute function for each component
     * More efficient than range-for for simple operations
     */
    template<typename Func>
    void forEach(Func&& func) {
        static_assert(std::is_invocable_v<Func, EntityId, T&>,
                      "Func must be callable with (EntityId, T&)");
        
        for (size_t i = 0; i < components_.size(); ++i) {
            if (alive_[i]) {
                func(componentToEntity_[i], components_[i]);
            }
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // STATISTICS
    // ═══════════════════════════════════════════════════════════════════════════
    
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    
    size_t capacity() const { return components_.capacity(); }
    
    /**
     * Reserve space for N components
     */
    void reserve(size_t n) {
        components_.reserve(n);
        componentToEntity_.reserve(n);
        alive_.reserve(n);
        freeListNext_.reserve(n);
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // DEBUG
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool validate() const {
        // Check alive count consistency
        size_t actualAlive = 0;
        for (bool a : alive_) {
            if (a) ++actualAlive;
        }
        if (actualAlive != size_) {
            return false;
        }
        
        // Check entity mappings
        for (size_t i = 0; i < components_.size(); ++i) {
            if (alive_[i]) {
                EntityId e = componentToEntity_[i];
                uint32_t idx = entity_id::getIndex(e);
                if (idx >= entityToComponent_.size()) {
                    return false;
                }
                if (entityToComponent_[idx] != static_cast<int32_t>(i)) {
                    return false;
                }
            }
        }
        
        return true;
    }

private:
    // Dense component storage
    std::vector<T> components_;
    std::vector<EntityId> componentToEntity_;  // Parallel array: which entity owns this component
    std::vector<bool> alive_;                  // Parallel array: is this slot alive?
    
    // Sparse entity-to-component mapping
    std::vector<int32_t> entityToComponent_;   // -1 = no component
    
    // Free list for recycled slots
    std::vector<int32_t> freeListNext_;        // Next pointer for free list
    int32_t freeListHead_ = -1;                // Head of free list (-1 = empty)
    
    // Statistics
    size_t size_ = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE-ERASED COMPONENT STORAGE INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════
// 
// Used by World to store any component type uniformly

class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void remove(EntityId entity) = 0;
    virtual bool has(EntityId entity) const = 0;
    virtual void clear() = 0;
    virtual size_t size() const = 0;
    virtual bool validate() const = 0;
};

template<typename T>
class TypedComponentStorage : public IComponentStorage {
public:
    template<typename... Args>
    T* add(EntityId entity, Args&&... args) {
        return storage_.add(entity, std::forward<Args>(args)...);
    }
    
    T* get(EntityId entity) {
        return storage_.get(entity);
    }
    
    const T* get(EntityId entity) const {
        return storage_.get(entity);
    }
    
    void remove(EntityId entity) override {
        storage_.remove(entity);
    }
    
    bool has(EntityId entity) const override {
        return storage_.has(entity);
    }
    
    void clear() override {
        storage_.clear();
    }
    
    size_t size() const override {
        return storage_.size();
    }
    
    bool validate() const override {
        return storage_.validate();
    }
    
    ComponentStorage<T>& getStorage() { return storage_; }
    
private:
    ComponentStorage<T> storage_;
};

} // namespace kern::engine
