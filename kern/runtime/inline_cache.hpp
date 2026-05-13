/* *
 * kern/runtime/inline_cache.hpp - Inline Caching
 * 
 * Monomorphic inline caches for:
 * - Global variable lookups
 * - Function call targets
 * - Property accesses
 * 
 * Avoids repeated hashmap lookups in hot code.
 */
#pragma once

#include "../core/value_refactored.hpp"
#include <unordered_map>
#include <cstdint>

namespace kern {

// Cache entry for monomorphic inline cache
struct InlineCacheEntry {
    // For global variables
    uint32_t nameHash;      // Hash of variable name (for validation)
    Value* valuePtr;      // Direct pointer to value
    uint64_t version;       // Version counter for invalidation
    
    // For function calls
    void* functionPtr;      // Cached function pointer
    uint32_t callCount;     // Counter for polymorphic promotion
    
    bool valid;
    
    InlineCacheEntry() 
        : nameHash(0), valuePtr(nullptr), version(0),
          functionPtr(nullptr), callCount(0), valid(false) {}
};

// Monomorphic inline cache (1-entry cache)
class MonomorphicCache {
    InlineCacheEntry entry;
    
public:
    MonomorphicCache() = default;
    
    // Try to get cached value
    bool tryGet(uint32_t hash, uint64_t currentVersion, Value*& outPtr) {
        if (!entry.valid) return false;
        if (entry.nameHash != hash) return false;
        if (entry.version != currentVersion) return false;
        
        outPtr = entry.valuePtr;
        return true;
    }
    
    // Update cache
    void set(uint32_t hash, Value* ptr, uint64_t version) {
        entry.nameHash = hash;
        entry.valuePtr = ptr;
        entry.version = version;
        entry.valid = true;
    }
    
    void invalidate() {
        entry.valid = false;
    }
    
    bool isValid() const { return entry.valid; }
};

// Polymorphic inline cache (2-4 entries)
// Used when monomorphic cache keeps missing
template<int N>
class PolymorphicCache {
    static_assert(N >= 2 && N <= 4, "Polymorphic cache size must be 2-4");
    
    InlineCacheEntry entries[N];
    int nextSlot;
    
public:
    PolymorphicCache() : nextSlot(0) {}
    
    bool tryGet(uint32_t hash, uint64_t currentVersion, Value*& outPtr) {
        for (int i = 0; i < N; i++) {
            if (entries[i].valid && 
                entries[i].nameHash == hash &&
                entries[i].version == currentVersion) {
                outPtr = entries[i].valuePtr;
                return true;
            }
        }
        return false;
    }
    
    void set(uint32_t hash, Value* ptr, uint64_t version) {
        // Find existing or use next slot (round-robin)
        for (int i = 0; i < N; i++) {
            if (!entries[i].valid || entries[i].nameHash == hash) {
                entries[i].nameHash = hash;
                entries[i].valuePtr = ptr;
                entries[i].version = version;
                entries[i].valid = true;
                return;
            }
        }
        
        // Use round-robin replacement
        entries[nextSlot].nameHash = hash;
        entries[nextSlot].valuePtr = ptr;
        entries[nextSlot].version = version;
        entries[nextSlot].valid = true;
        nextSlot = (nextSlot + 1) % N;
    }
    
    void invalidate() {
        for (int i = 0; i < N; i++) {
            entries[i].valid = false;
        }
    }
};

// Global variable table with versioning for cache invalidation
class VersionedGlobals {
    std::unordered_map<std::string, Value> values;
    uint64_t version;
    
public:
    VersionedGlobals() : version(1) {}
    
    Value get(const std::string& name) {
        auto it = values.find(name);
        if (it != values.end()) {
            return it->second;
        }
        return Value::nil();
    }
    
    void set(const std::string& name, const Value& val) {
        values[name] = val;
        version++;  // Invalidate caches
    }
    
    bool exists(const std::string& name) const {
        return values.count(name) > 0;
    }
    
    Value* getPtr(const std::string& name) {
        auto it = values.find(name);
        if (it != values.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    uint32_t hashName(const std::string& name) const {
        // Simple hash for cache indexing
        uint32_t hash = 0;
        for (char c : name) {
            hash = hash * 31 + c;
        }
        return hash;
    }
    
    uint64_t getVersion() const { return version; }
};

// VM with inline caching
class CachedVM : public LimitedVM {
    VersionedGlobals globals;
    
    // Per-instruction inline caches
    // In real implementation, would be allocated with bytecode
    std::vector<MonomorphicCache> globalCaches;
    
public:
    Result<Value> executeCached(const Bytecode& code,
                                const std::vector<std::string>& constants) {
        // Initialize caches
        globalCaches.resize(code.size());
        
        // Execute with caching
        pc = 0;
        sp = 0;
        frameCount = 0;
        
        callStack[0] = {0, 0, 0, 0};
        frameCount = 1;
        
        while (pc < code.size()) {
            const auto& inst = code[pc];
            
            switch (inst.op) {
                case Instruction::LOAD_GLOBAL: {
                    // Try inline cache first
                    MonomorphicCache& cache = globalCaches[pc];
                    uint32_t hash = globals.hashName(constants[inst.operand]);
                    Value* cachedPtr = nullptr;
                    
                    if (cache.tryGet(hash, globals.getVersion(), cachedPtr)) {
                        // Cache hit - direct pointer access
                        stack[sp++] = *cachedPtr;
                    } else {
                        // Cache miss - slow path
                        Value* ptr = globals.getPtr(constants[inst.operand]);
                        if (ptr) {
                            stack[sp++] = *ptr;
                            cache.set(hash, ptr, globals.getVersion());
                        } else {
                            stack[sp++] = Value::nil();
                        }
                    }
                    pc++;
                    break;
                }
                    
                case Instruction::STORE_GLOBAL: {
                    // Store always invalidates cache
                    globals.set(constants[inst.operand], stack[--sp]);
                    
                    // Invalidate all caches (simplified)
                    for (auto& c : globalCaches) {
                        c.invalidate();
                    }
                    pc++;
                    break;
                }
                    
                // Other instructions same as base VM
                default:
                    // Fall through to base implementation
                    // (would dispatch to base class method)
                    pc++;
                    break;
            }
        }
        
        return Result<Value>(Value::nil());
    }
    
    // Get cache statistics
    struct CacheStats {
        size_t totalLookups;
        size_t cacheHits;
        size_t cacheMisses;
        double hitRate;
    };
    
    CacheStats getCacheStats() const {
        // Would track in real implementation
        return {0, 0, 0, 0.0};
    }
};

// Statistics for inline cache analysis
class InlineCacheStats {
    size_t totalLookups;
    size_t hits;
    size_t misses;
    size_t promotions;  // Mono -> Poly
    
public:
    InlineCacheStats() 
        : totalLookups(0), hits(0), misses(0), promotions(0) {}
    
    void recordHit() { hits++; totalLookups++; }
    void recordMiss() { misses++; totalLookups++; }
    void recordPromotion() { promotions++; }
    
    double hitRate() const {
        return totalLookups > 0 ? (100.0 * hits / totalLookups) : 0;
    }
    
    void print() const {
        std::cout << "\n=== Inline Cache Statistics ===\n";
        std::cout << "Total lookups:  " << totalLookups << "\n";
        std::cout << "Cache hits:     " << hits << "\n";
        std::cout << "Cache misses:   " << misses << "\n";
        std::cout << "Hit rate:       " << std::fixed << std::setprecision(1) 
                  << hitRate() << "%\n";
        std::cout << "Promotions:     " << promotions << "\n";
    }
};

} // namespace kern
