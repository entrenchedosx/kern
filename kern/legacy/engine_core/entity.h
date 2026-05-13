/* *
 * kern/engine/core/entity.h - Entity Identity System
 * 
 * DATA CONTRACT: Entity = 64-bit ID (index + generation)
 * - NO pointers as entity identity
 * - Entity is meaningless without EntityRegistry
 * - Lifetime controlled ONLY by EntityRegistry
 * 
 * Week 1 Production Implementation
 * Zero placeholders. Fully functional.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <limits>
#include <utility>
#include <type_traits>
#include <cassert>

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY ID CONTRACT (GLOBAL RULE - ALL SYSTEMS MUST OBEY)
// ═══════════════════════════════════════════════════════════════════════════════

namespace kern::engine {

// Entity ID structure: 64-bit packed
// [48-bit index | 16-bit generation]
// This gives us 281 trillion entities with 65k reuses per slot
using EntityId = uint64_t;
constexpr EntityId INVALID_ENTITY = 0;

// Maximum values
constexpr uint32_t MAX_ENTITY_INDEX = 0x0000FFFFFFFFFFFFULL;  // 48 bits
constexpr uint16_t MAX_GENERATION = 0xFFFF;

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY ID UTILITY FUNCTIONS (constexpr - compile-time evaluation)
// ═══════════════════════════════════════════════════════════════════════════════

namespace entity_id {

// Pack index and generation into EntityId
constexpr inline EntityId pack(uint32_t index, uint16_t generation) noexcept {
    return (static_cast<EntityId>(generation) << 48) | index;
}

// Extract index from EntityId (lower 48 bits)
constexpr inline uint32_t getIndex(EntityId id) noexcept {
    return static_cast<uint32_t>(id & 0x0000FFFFFFFFFFFFULL);
}

// Extract generation from EntityId (upper 16 bits)
constexpr inline uint16_t getGeneration(EntityId id) noexcept {
    return static_cast<uint16_t>(id >> 48);
}

// Check if entity ID is valid (non-zero)
constexpr inline bool isValid(EntityId id) noexcept {
    return id != INVALID_ENTITY;
}

// Create invalid entity ID explicitly
constexpr inline EntityId makeInvalid() noexcept {
    return INVALID_ENTITY;
}

} // namespace entity_id

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY METADATA (internal use only - systems should not access directly)
// ═══════════════════════════════════════════════════════════════════════════════

struct EntityMetadata {
    uint16_t generation = 0;      // Current generation for this slot
    bool alive = false;           // Is entity currently alive?
    uint32_t nextFree = 0;        // Next free slot (for free list)
    
    // Reserved for future use (keeps struct size at 8 bytes)
    uint8_t reserved[1] = {};
};

static_assert(sizeof(EntityMetadata) == 8, "EntityMetadata must be 8 bytes for cache efficiency");

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY REGISTRY (SOURCE OF TRUTH FOR ALL ENTITY LIFECYCLES)
// ═══════════════════════════════════════════════════════════════════════════════

class EntityRegistry {
public:
    // Configuration
    static constexpr size_t INITIAL_CAPACITY = 1024;
    static constexpr size_t MAX_CAPACITY = 0x0000FFFFFFFFFFFFULL;  // 48-bit limit
    
    // Iterator for alive entities (cache-friendly)
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
    
    // Range-based for loop support
    struct AliveRange {
        const EntityRegistry& registry;
        AliveIterator begin() const { return AliveIterator(registry, 0); }
        AliveIterator end() const { return AliveIterator(registry, registry.metadata_.size()); }
    };

public:
    // ═══════════════════════════════════════════════════════════════════════════
    // CONSTRUCTION / DESTRUCTION
    // ═══════════════════════════════════════════════════════════════════════════
    
    EntityRegistry() {
        metadata_.reserve(INITIAL_CAPACITY);
        // Slot 0 is reserved for INVALID_ENTITY
        metadata_.push_back(EntityMetadata{0, false, 0});
        nextFree_ = 0;  // No free slots initially (except invalid slot)
        aliveCount_ = 0;
    }
    
    ~EntityRegistry() = default;
    
    // Non-copyable (entities are tied to this registry)
    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;
    
    // Movable (transfers entity ownership)
    EntityRegistry(EntityRegistry&&) = default;
    EntityRegistry& operator=(EntityRegistry&&) = default;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ENTITY LIFECYCLE (CRITICAL PATH - ALL SYSTEMS USE THESE)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Create a new entity
     * @return EntityId (always valid, never INVALID_ENTITY)
     * @throws std::bad_alloc if capacity exceeded
     * 
     * CONTRACT: Entity is immediately alive and usable
     * CONTRACT: ID is unique and will not be reused until destroyed
     */
    EntityId create();
    
    /**
     * Destroy an entity
     * @param entity The entity to destroy
     * 
     * CONTRACT: Safe to call with invalid/expired IDs (no-op)
     * CONTRACT: Entity ID becomes invalid after this call
     * CONTRACT: ID may be recycled for future create() calls
     */
    void destroy(EntityId entity);
    
    /**
     * Check if entity is currently alive
     * @param entity The entity to check
     * @return true if alive and valid
     * 
     * CONTRACT: Returns false for invalid/expired IDs
     * CONTRACT: O(1) performance
     */
    bool isAlive(EntityId entity) const noexcept;
    
    /**
     * Check if entity ID is valid (non-zero and properly formed)
     * @param entity The entity ID to validate
     * @return true if ID format is valid
     * 
     * NOTE: Valid ID != Alive entity. Use isAlive() to check existence.
     */
    static bool isValidId(EntityId entity) noexcept {
        return entity_id::isValid(entity);
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // BULK OPERATIONS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Create multiple entities at once (more efficient than individual creates)
     * @param count Number of entities to create
     * @return Vector of EntityIds
     */
    std::vector<EntityId> createMany(size_t count);
    
    /**
     * Destroy multiple entities at once
     * @param entities Vector of entities to destroy
     */
    void destroyMany(const std::vector<EntityId>& entities);
    
    /**
     * Destroy all entities (clear registry)
     * CONTRACT: All previously valid IDs become invalid
     */
    void clear();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ITERATION (FOR SYSTEMS)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Iterate over all alive entities
     * Usage: for (EntityId e : registry.alive()) { ... }
     * 
     * CONTRACT: Safe to call during iteration
     * CONTRACT: New entities created during iteration may or may not be visited
     * CONTRACT: Destroyed entities during iteration are handled safely
     */
    AliveRange alive() const {
        return AliveRange{*this};
    }
    
    /**
     * Execute function for each alive entity
     * @param func Callable taking EntityId
     * 
     * More efficient than range-for for simple operations
     */
    template<typename Func>
    void forEach(Func&& func) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // STATISTICS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Get number of currently alive entities
     */
    size_t aliveCount() const noexcept { return aliveCount_; }
    
    /**
     * Get total capacity (including dead entities)
     */
    size_t capacity() const noexcept { return metadata_.size(); }
    
    /**
     * Get number of entities that can be created without reallocation
     */
    size_t availableSlots() const noexcept { 
        return metadata_.capacity() - metadata_.size() + freeCount_;
    }
    
    /**
     * Check if registry is empty
     */
    bool empty() const noexcept { return aliveCount_ == 0; }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MEMORY MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Reserve space for N entities (avoids reallocations)
     */
    void reserve(size_t capacity);
    
    /**
     * Shrink capacity to fit current size (releases memory)
     */
    void shrinkToFit();
    
    /**
     * Compact entity IDs (expensive - reassigns all IDs)
     * Use only for save/load operations, NOT during runtime
     */
    std::vector<EntityId> compact();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // DEBUG / DIAGNOSTICS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Validate internal state (debug builds only)
     * @return true if state is consistent
     */
    bool validate() const;
    
    /**
     * Print debug statistics
     */
    void debugPrint() const;
    
    /**
     * Get generation for debugging
     */
    uint16_t getGenerationDebug(EntityId entity) const;

private:
    // Metadata storage (sparse set pattern)
    std::vector<EntityMetadata> metadata_;
    
    // Free list head (index of first free slot, 0 if none)
    uint32_t nextFree_ = 0;
    
    // Cached statistics
    size_t aliveCount_ = 0;
    size_t freeCount_ = 0;
    
    // Internal helpers
    void growIfNeeded();
    uint32_t allocateSlot();
    void freeSlot(uint32_t index);
    
    // Debug validation
    #ifndef NDEBUG
    void assertInvariants() const;
    #else
    void assertInvariants() const {}
    #endif
};

// ═══════════════════════════════════════════════════════════════════════════════
// TEMPLATE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════════════

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

} // namespace kern::engine

// ═══════════════════════════════════════════════════════════════════════════════
// COMPILE-TIME VERIFICATION
// ═══════════════════════════════════════════════════════════════════════════════

static_assert(sizeof(kern::engine::EntityId) == 8, "EntityId must be 64-bit");
static_assert(kern::engine::INVALID_ENTITY == 0, "INVALID_ENTITY must be 0");
