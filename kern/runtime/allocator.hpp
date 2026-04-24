/* *
 * kern/runtime/allocator.hpp - Memory Allocator
 * 
 * Provides:
 * - Arena allocator for bulk allocation/deallocation
 * - Pool allocator for small fixed-size objects
 * - Value stack with custom allocation
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <new>

namespace kern {

// Arena allocator - bump pointer allocation
class ArenaAllocator {
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024;  // 64KB blocks
    
    struct Block {
        std::unique_ptr<uint8_t[]> data;
        size_t size;
        size_t used;
        Block* next;
    };
    
    Block* current;
    Block* blocks;
    size_t blockSize;
    size_t totalAllocated;
    size_t totalUsed;
    
public:
    explicit ArenaAllocator(size_t blockSize = DEFAULT_BLOCK_SIZE);
    ~ArenaAllocator();
    
    // Disable copy/move
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&) = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) = delete;
    
    // Allocation
    void* allocate(size_t size, size_t alignment = 8);
    
    // Typed allocation
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    // Array allocation
    template<typename T>
    T* allocateArray(size_t count) {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }
    
    // Reset (free all allocations)
    void reset();
    
    // Stats
    size_t getTotalAllocated() const { return totalAllocated; }
    size_t getTotalUsed() const { return totalUsed; }
    size_t getWastedBytes() const { return totalAllocated - totalUsed; }
};

// Pool allocator for fixed-size objects
class PoolAllocator {
    struct FreeNode {
        FreeNode* next;
    };
    
    static constexpr size_t DEFAULT_BLOCK_SIZE = 4096;
    
    struct Block {
        std::unique_ptr<uint8_t[]> data;
        Block* next;
    };
    
    size_t objectSize;
    size_t objectsPerBlock;
    Block* blocks;
    FreeNode* freeList;
    size_t allocated;
    size_t freed;
    
public:
    PoolAllocator(size_t objSize, size_t objsPerBlock = 256);
    ~PoolAllocator();
    
    // Disable copy/move
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
    PoolAllocator& operator=(PoolAllocator&&) = delete;
    
    void* allocate();
    void free(void* ptr);
    
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        static_assert(sizeof(T) <= objectSize, "Object too large for pool");
        void* mem = allocate();
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    template<typename T>
    void destroy(T* ptr) {
        ptr->~T();
        free(ptr);
    }
    
    size_t getAllocatedCount() const { return allocated - freed; }
    size_t getTotalAllocated() const { return allocated; }
    size_t getTotalFreed() const { return freed; }
};

// Multi-pool allocator for different sizes
class MultiPoolAllocator {
    static constexpr size_t SMALL_THRESHOLD = 64;
    static constexpr size_t MEDIUM_THRESHOLD = 256;
    
    PoolAllocator smallPool;   // 64 bytes
    PoolAllocator mediumPool;  // 256 bytes
    ArenaAllocator largeArena; // > 256 bytes
    
public:
    MultiPoolAllocator();
    
    void* allocate(size_t size);
    void free(void* ptr, size_t size);
    
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        void* mem = allocate(sizeof(T));
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    template<typename T>
    void destroy(T* ptr) {
        ptr->~T();
        free(ptr, sizeof(T));
    }
};

// Main allocator interface used by VM
class Allocator {
    ArenaAllocator frameArena;      // For register windows, frames
    MultiPoolAllocator valuePool;   // For Value objects
    ArenaAllocator gcArena;         // For garbage-collected objects
    
public:
    Allocator();
    
    // Frame allocation (bulk)
    void* allocateFrame(size_t size);
    void resetFrames();  // Reset all frame allocations
    
    // Value allocation
    Value* allocateValue();
    void freeValue(Value* val);
    
    // General allocation (for collections)
    void* allocate(size_t size);
    void free(void* ptr, size_t size);
    
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        return valuePool.construct<T>(std::forward<Args>(args)...);
    }
    
    template<typename T>
    void destroy(T* ptr) {
        valuePool.destroy(ptr);
    }
    
    // Stats
    void printStats() const;
};

} // namespace kern
