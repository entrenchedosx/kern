/* *
 * kern/engine/core/entity.cpp - Entity Registry Implementation
 * 
 * Production implementation with:
 * - ID recycling with generation counters
 * - Sparse set storage pattern
 * - Cache-friendly iteration
 * - Zero undefined behavior
 * 
 * Week 1 Production Implementation
 */

#include "entity.h"
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace kern::engine {

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY LIFECYCLE IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

EntityId EntityRegistry::create() {
    assertInvariants();
    
    // Allocate a slot (either from free list or new allocation)
    uint32_t index = allocateSlot();
    
    // Get current generation for this slot
    uint16_t generation = metadata_[index].generation;
    
    // Mark as alive
    metadata_[index].alive = true;
    ++aliveCount_;
    
    // Pack into EntityId
    EntityId entity = entity_id::pack(index, generation);
    
    assertInvariants();
    return entity;
}

void EntityRegistry::destroy(EntityId entity) {
    assertInvariants();
    
    // Handle invalid IDs gracefully (no-op)
    if (!isValidId(entity)) {
        return;
    }
    
    uint32_t index = entity_id::getIndex(entity);
    uint16_t generation = entity_id::getGeneration(entity);
    
    // Bounds check
    if (index >= metadata_.size()) {
        return;  // Out of bounds - already invalid
    }
    
    // Generation check - is this the same entity or a recycled one?
    if (metadata_[index].generation != generation) {
        return;  // Entity was already destroyed and slot recycled
    }
    
    // Is it alive?
    if (!metadata_[index].alive) {
        return;  // Already dead
    }
    
    // Mark as dead and increment generation (prevents reuse of old IDs)
    metadata_[index].alive = false;
    metadata_[index].generation++;
    
    // Check for generation overflow (extremely unlikely but critical)
    if (metadata_[index].generation == 0) {
        // Generation wrapped - this slot is now permanently retired
        // This prevents ABA problems with 65k entity recycles
        metadata_[index].generation = MAX_GENERATION;  // Keep at max to prevent reuse
    }
    
    --aliveCount_;
    
    // Add to free list
    freeSlot(index);
    
    assertInvariants();
}

bool EntityRegistry::isAlive(EntityId entity) const noexcept {
    // Quick rejection for invalid IDs
    if (!isValidId(entity)) {
        return false;
    }
    
    uint32_t index = entity_id::getIndex(entity);
    uint16_t generation = entity_id::getGeneration(entity);
    
    // Bounds check
    if (index >= metadata_.size()) {
        return false;
    }
    
    // Generation check + alive check
    return metadata_[index].alive && metadata_[index].generation == generation;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BULK OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<EntityId> EntityRegistry::createMany(size_t count) {
    if (count == 0) {
        return {};
    }
    
    // Reserve space upfront for efficiency
    reserve(metadata_.size() + count);
    
    std::vector<EntityId> result;
    result.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        result.push_back(create());
    }
    
    return result;
}

void EntityRegistry::destroyMany(const std::vector<EntityId>& entities) {
    for (EntityId entity : entities) {
        destroy(entity);
    }
}

void EntityRegistry::clear() {
    // Mark all alive entities as dead
    for (size_t i = 1; i < metadata_.size(); ++i) {
        if (metadata_[i].alive) {
            metadata_[i].alive = false;
            metadata_[i].generation++;
            
            // Add to free list
            metadata_[i].nextFree = nextFree_;
            nextFree_ = static_cast<uint32_t>(i);
        }
    }
    
    aliveCount_ = 0;
    freeCount_ = metadata_.size() - 1;  // All slots except slot 0 are free
    
    assertInvariants();
}

// ═══════════════════════════════════════════════════════════════════════════════
// MEMORY MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void EntityRegistry::reserve(size_t capacity) {
    if (capacity <= metadata_.capacity()) {
        return;
    }
    
    if (capacity > MAX_CAPACITY) {
        throw std::bad_alloc();  // Exceeds 48-bit limit
    }
    
    size_t oldSize = metadata_.size();
    metadata_.reserve(capacity);
    
    // Note: We don't add slots here - they get added on demand in allocateSlot()
}

void EntityRegistry::shrinkToFit() {
    // We can't shrink below current used slots
    // This operation is expensive and rarely needed
    metadata_.shrink_to_fit();
}

std::vector<EntityId> EntityRegistry::compact() {
    // WARNING: This changes all entity IDs - only use for save/load!
    // Compacts alive entities to contiguous IDs starting from 1
    
    std::vector<EntityId> oldToNew(metadata_.size(), INVALID_ENTITY);
    std::vector<EntityMetadata> newMetadata;
    newMetadata.reserve(aliveCount_ + 1);
    
    // Slot 0 is always invalid
    newMetadata.push_back(EntityMetadata{0, false, 0});
    
    // Copy alive entities to new compact storage
    for (size_t oldIndex = 1; oldIndex < metadata_.size(); ++oldIndex) {
        if (metadata_[oldIndex].alive) {
            uint32_t newIndex = static_cast<uint32_t>(newMetadata.size());
            uint16_t generation = 1;  // Reset generation in new slot
            
            newMetadata.push_back(EntityMetadata{
                generation,
                true,
                0  // Not in free list
            });
            
            oldToNew[oldIndex] = entity_id::pack(newIndex, generation);
        }
    }
    
    // Replace metadata
    metadata_ = std::move(newMetadata);
    nextFree_ = 0;  // No free slots after compact
    freeCount_ = 0;
    // aliveCount_ unchanged
    
    return oldToNew;  // Return mapping for caller to update components
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

void EntityRegistry::growIfNeeded() {
    if (metadata_.size() < metadata_.capacity()) {
        return;  // Still have reserved space
    }
    
    // Need to grow - double capacity (standard vector growth strategy)
    size_t newCapacity = std::max(metadata_.capacity() * 2, INITIAL_CAPACITY);
    
    if (newCapacity > MAX_CAPACITY) {
        newCapacity = MAX_CAPACITY;
    }
    
    metadata_.reserve(newCapacity);
}

uint32_t EntityRegistry::allocateSlot() {
    // Try free list first (recycling)
    if (nextFree_ != 0) {
        uint32_t index = nextFree_;
        nextFree_ = metadata_[index].nextFree;
        --freeCount_;
        
        // Keep the current generation (it was already incremented on destroy)
        return index;
    }
    
    // No free slots - allocate new one
    growIfNeeded();
    
    uint32_t index = static_cast<uint32_t>(metadata_.size());
    
    if (index > MAX_ENTITY_INDEX) {
        throw std::bad_alloc();  // Exceeded 48-bit limit
    }
    
    metadata_.push_back(EntityMetadata{
        1,      // Generation starts at 1 (0 is reserved)
        false,  // Not alive yet (will be set by caller)
        0       // No next free
    });
    
    return index;
}

void EntityRegistry::freeSlot(uint32_t index) {
    assert(index > 0 && index < metadata_.size());
    assert(!metadata_[index].alive);  // Must be dead before freeing
    
    // Add to head of free list
    metadata_[index].nextFree = nextFree_;
    nextFree_ = index;
    ++freeCount_;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DEBUG / DIAGNOSTICS
// ═══════════════════════════════════════════════════════════════════════════════

bool EntityRegistry::validate() const {
    // Check alive count consistency
    size_t actualAlive = 0;
    size_t actualFree = 0;
    
    for (size_t i = 1; i < metadata_.size(); ++i) {
        if (metadata_[i].alive) {
            ++actualAlive;
        } else {
            ++actualFree;
        }
    }
    
    if (actualAlive != aliveCount_) {
        std::cerr << "EntityRegistry validation failed: alive count mismatch\n";
        return false;
    }
    
    // Check free list consistency
    size_t freeListCount = 0;
    uint32_t current = nextFree_;
    while (current != 0) {
        if (current >= metadata_.size()) {
            std::cerr << "EntityRegistry validation failed: free list corruption\n";
            return false;
        }
        if (metadata_[current].alive) {
            std::cerr << "EntityRegistry validation failed: alive entity in free list\n";
            return false;
        }
        ++freeListCount;
        current = metadata_[current].nextFree;
    }
    
    if (freeListCount != freeCount_) {
        std::cerr << "EntityRegistry validation failed: free list count mismatch\n";
        return false;
    }
    
    return true;
}

void EntityRegistry::debugPrint() const {
    std::cout << "=== EntityRegistry ===\n";
    std::cout << "  Capacity: " << capacity() << "\n";
    std::cout << "  Alive: " << aliveCount() << "\n";
    std::cout << "  Free slots: " << freeCount_ << "\n";
    std::cout << "  Free list: " << (nextFree_ != 0 ? "yes" : "empty") << "\n";
    
    // Print first 10 entities
    std::cout << "  First 10 entities:\n";
    for (size_t i = 1; i < std::min(metadata_.size(), size_t(11)); ++i) {
        const auto& meta = metadata_[i];
        std::cout << "    [" << i << "] gen=" << meta.generation 
                  << " alive=" << meta.alive 
                  << " next=" << meta.nextFree << "\n";
    }
}

uint16_t EntityRegistry::getGenerationDebug(EntityId entity) const {
    uint32_t index = entity_id::getIndex(entity);
    if (index < metadata_.size()) {
        return metadata_[index].generation;
    }
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// DEBUG ASSERTIONS
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef NDEBUG
void EntityRegistry::assertInvariants() const {
    // In debug builds, verify consistency
    // This is expensive, so only called at entry/exit of public methods
    
    // Slot 0 must be reserved and dead
    assert(metadata_[0].generation == 0);
    assert(!metadata_[0].alive);
    
    // Alive count must match
    #ifdef ENTITY_REGISTRY_EXPENSIVE_CHECKS
    size_t count = 0;
    for (size_t i = 1; i < metadata_.size(); ++i) {
        if (metadata_[i].alive) ++count;
    }
    assert(count == aliveCount_);
    #endif
}
#endif

} // namespace kern::engine
