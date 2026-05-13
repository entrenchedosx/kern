/* *
 * kern/runtime/modules/ecs/component.h - Self-Contained Component System
 * 
 * Self-contained component system for ECS module.
 * No dependency on old engine core - fully independent.
 */

#pragma once
#include "entity.h"
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <cassert>
#include <memory>
#include <functional>

namespace kern::runtime::ecs {

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT BASE (Optional)
// ═══════════════════════════════════════════════════════════════════════════════

struct ComponentBase {
    // Empty base class for type identification
};

// ═══════════════════════════════════════════════════════════════════════════════
// SPARSE SET COMPONENT STORAGE
// ═══════════════════════════════════════════════════════════════════════════════

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

    struct Range {
        ComponentStorage& storage;
        Iterator begin() { return Iterator(storage, 0); }
        Iterator end() { return Iterator(storage, storage.components_.size()); }
    };

public:
    // Core operations
    bool has(EntityId entity) const {
        uint32_t index = entity_id::getIndex(entity);
        if (index >= entityToComponent_.size()) {
            return false;
        }
        int32_t compIdx = entityToComponent_[index];
        if (compIdx < 0) {
            return false;
        }
        size_t ci = static_cast<size_t>(compIdx);
        return alive_[ci];
    }
    
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
    
    template<typename... Args>
    T* add(EntityId entity, Args&&... args) {
        uint32_t index = entity_id::getIndex(entity);
        
        if (index >= entityToComponent_.size()) {
            entityToComponent_.resize(index + 1, -1);
        }
        
        if (entityToComponent_[index] >= 0) {
            size_t ci = static_cast<size_t>(entityToComponent_[index]);
            return &components_[ci];
        }
        
        size_t compIndex;
        if (freeListHead_ >= 0) {
            compIndex = static_cast<size_t>(freeListHead_);
            freeListHead_ = freeListNext_[compIndex];
        } else {
            compIndex = components_.size();
            components_.emplace_back();
            componentToEntity_.push_back(INVALID_ENTITY);
            alive_.push_back(false);
            freeListNext_.push_back(-1);
        }
        
        new (&components_[compIndex]) T(std::forward<Args>(args)...);
        componentToEntity_[compIndex] = entity;
        alive_[compIndex] = true;
        
        entityToComponent_[index] = static_cast<int32_t>(compIndex);
        
        ++size_;
        return &components_[compIndex];
    }
    
    void remove(EntityId entity) {
        uint32_t index = entity_id::getIndex(entity);
        if (index >= entityToComponent_.size()) {
            return;
        }
        
        int32_t compIdx = entityToComponent_[index];
        if (compIdx < 0) {
            return;
        }
        
        size_t ci = static_cast<size_t>(compIdx);
        if (!alive_[ci]) {
            return;
        }
        
        if constexpr (!std::is_trivially_destructible_v<T>) {
            components_[ci].~T();
        }
        
        alive_[ci] = false;
        entityToComponent_[index] = -1;
        
        freeListNext_[ci] = freeListHead_;
        freeListHead_ = static_cast<int32_t>(ci);
        
        --size_;
    }
    
    void clear() {
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
    
    // Iteration
    Range all() {
        return Range{*this};
    }
    
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
    
    // Statistics
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    size_t capacity() const { return components_.capacity(); }
    void reserve(size_t n) {
        components_.reserve(n);
        componentToEntity_.reserve(n);
        alive_.reserve(n);
        freeListNext_.reserve(n);
    }
    
    // Debug
    bool validate() const {
        size_t actualAlive = 0;
        for (bool a : alive_) {
            if (a) ++actualAlive;
        }
        if (actualAlive != size_) {
            return false;
        }
        
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
    std::vector<T> components_;
    std::vector<EntityId> componentToEntity_;
    std::vector<bool> alive_;
    std::vector<int32_t> entityToComponent_;
    std::vector<int32_t> freeListNext_;
    int32_t freeListHead_ = -1;
    size_t size_ = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE-ERASED COMPONENT STORAGE INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

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

} // namespace kern::runtime::ecs
