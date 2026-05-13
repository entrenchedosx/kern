/* *
 * kern/runtime/modules/ecs/entity.h - Self-Contained Entity System
 * 
 * Self-contained entity system for ECS module.
 * No dependency on old engine core - fully independent.
 */

#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include <utility>
#include <type_traits>
#include <cassert>

namespace kern::runtime::ecs {

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY ID (Self-Contained)
// ═══════════════════════════════════════════════════════════════════════════════

using EntityId = uint64_t;
constexpr EntityId INVALID_ENTITY = 0;

constexpr uint32_t MAX_ENTITY_INDEX = 0x0000FFFFFFFFFFFFULL;  // 48 bits
constexpr uint16_t MAX_GENERATION = 0xFFFF;

namespace entity_id {
    constexpr inline EntityId pack(uint32_t index, uint16_t generation) noexcept {
        return (static_cast<EntityId>(generation) << 48) | index;
    }
    constexpr inline uint32_t getIndex(EntityId id) noexcept {
        return static_cast<uint32_t>(id & 0x0000FFFFFFFFFFFFULL);
    }
    constexpr inline uint16_t getGeneration(EntityId id) noexcept {
        return static_cast<uint16_t>(id >> 48);
    }
    constexpr inline bool isValid(EntityId id) noexcept {
        return id != INVALID_ENTITY;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY METADATA
// ═══════════════════════════════════════════════════════════════════════════════

struct EntityMetadata {
    uint16_t generation = 0;
    bool alive = false;
    uint32_t nextFree = 0;
    uint8_t reserved[1] = {};
};

static_assert(sizeof(EntityMetadata) == 8, "EntityMetadata must be 8 bytes");

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY REGISTRY
// ═══════════════════════════════════════════════════════════════════════════════

class EntityRegistry {
public:
    // Configuration
    static constexpr size_t INITIAL_CAPACITY = 1024;
    static constexpr size_t MAX_CAPACITY = 0x0000FFFFFFFFFFFFULL;  // 48-bit limit

    // Iterator for alive entities
    class AliveIterator {
    public:
        AliveIterator(const EntityRegistry& registry, size_t index)
            : registry_(registry), index_(index) {
            advanceToAlive();
        }
        
        EntityId operator*() const {
            return entity_id::pack(static_cast<uint32_t>(index_), 
                                 registry_.metadata_[index_].generation);
        }
        
        AliveIterator& operator++() {
            ++index_;
            advanceToAlive();
            return *this;
        }
        
        bool operator!=(const AliveIterator& other) const {
            return index_ != other.index_;
        }
        
    private:
        void advanceToAlive() {
            while (index_ < registry_.metadata_.size() && 
                   !registry_.metadata_[index_].alive) {
                ++index_;
            }
        }
        
        const EntityRegistry& registry_;
        size_t index_;
    };

    struct AliveRange {
        const EntityRegistry& registry;
        AliveIterator begin() const { return AliveIterator(registry, 0); }
        AliveIterator end() const { return AliveIterator(registry, registry.metadata_.size()); }
    };

public:
    EntityRegistry();
    ~EntityRegistry() = default;
    
    // Non-copyable
    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;
    
    // Movable
    EntityRegistry(EntityRegistry&&) = default;
    EntityRegistry& operator=(EntityRegistry&&) = default;
    
    // Entity lifecycle
    EntityId create();
    void destroy(EntityId entity);
    bool isAlive(EntityId entity) const noexcept;
    
    // Bulk operations
    std::vector<EntityId> createMany(size_t count);
    void destroyMany(const std::vector<EntityId>& entities);
    void clear();
    
    // Iteration
    AliveRange alive() const {
        return AliveRange{*this};
    }
    
    template<typename Func>
    void forEach(Func&& func) const;
    
    // Statistics
    size_t aliveCount() const noexcept { return aliveCount_; }
    size_t capacity() const noexcept { return metadata_.size(); }
    size_t availableSlots() const noexcept { 
        return metadata_.capacity() - metadata_.size() + freeCount_;
    }
    bool empty() const noexcept { return aliveCount_ == 0; }
    
    // Memory management
    void reserve(size_t capacity);
    void shrinkToFit();
    std::vector<EntityId> compact();
    
    // Debug
    bool validate() const;
    void debugPrint() const;
    uint16_t getGenerationDebug(EntityId entity) const;

private:
    std::vector<EntityMetadata> metadata_;
    uint32_t nextFree_ = 0;
    size_t aliveCount_ = 0;
    size_t freeCount_ = 0;
    
    void growIfNeeded();
    uint32_t allocateSlot();
    void freeSlot(uint32_t index);
    
    #ifndef NDEBUG
    void assertInvariants() const;
    #else
    void assertInvariants() const {}
    #endif
};

// Template implementation
template<typename Func>
void EntityRegistry::forEach(Func&& func) const {
    static_assert(std::is_invocable_v<Func, EntityId>, 
                  "Func must be callable with EntityId");
    
    for (size_t i = 1; i < metadata_.size(); ++i) {
        if (metadata_[i].alive) {
            func(entity_id::pack(static_cast<uint32_t>(i), metadata_[i].generation));
        }
    }
}

} // namespace kern::runtime::ecs
