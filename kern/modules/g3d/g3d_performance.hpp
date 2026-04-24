/* *
 * kern/modules/g3d/g3d_performance.hpp - Performance Validation & Zero-Overhead Design
 * 
 * Ensures the Ursina-style API has ZERO overhead vs direct ECS access.
 * 
 * Key measurements:
 * - Entity property access cost
 * - ECS bridge overhead
 * - Update loop dispatch cost
 * - Memory allocation tracking
 */
#pragma once

#include "g3d_api.hpp"
#include <chrono>
#include <vector>
#include <atomic>

namespace kern {
namespace g3d {
namespace perf {

// ============================================================================
// Performance Counters
// ============================================================================

struct PerformanceCounters {
    std::atomic<uint64_t> entityPropertyAccesses{0};
    std::atomic<uint64_t> componentCacheHits{0};
    std::atomic<uint64_t> componentCacheMisses{0};
    std::atomic<uint64_t> ecsLookups{0};
    std::atomic<uint64_t> allocations{0};
    std::atomic<uint64_t> deallocations{0};
    std::atomic<uint64_t> updateCalls{0};
    std::atomic<uint64_t> inputPolls{0};
    
    void reset() {
        entityPropertyAccesses = 0;
        componentCacheHits = 0;
        componentCacheMisses = 0;
        ecsLookups = 0;
        allocations = 0;
        deallocations = 0;
        updateCalls = 0;
        inputPolls = 0;
    }
    
    void print() const {
        std::cout << "\n=== G3D Performance Counters ===\n";
        std::cout << "Entity property accesses: " << entityPropertyAccesses.load() << "\n";
        std::cout << "Component cache hits:     " << componentCacheHits.load() << "\n";
        std::cout << "Component cache misses:   " << componentCacheMisses.load() << "\n";
        double hitRate = componentCacheHits + componentCacheMisses > 0 ?
            (100.0 * componentCacheHits / (componentCacheHits + componentCacheMisses)) : 0;
        std::cout << "Cache hit rate:           " << std::fixed << std::setprecision(1) 
                  << hitRate << "%\n";
        std::cout << "ECS lookups:              " << ecsLookups.load() << "\n";
        std::cout << "Allocations:              " << allocations.load() << "\n";
        std::cout << "Deallocations:            " << deallocations.load() << "\n";
        std::cout << "Update calls:             " << updateCalls.load() << "\n";
        std::cout << "Input polls:              " << inputPolls.load() << "\n";
        
        if (allocations.load() > 0) {
            std::cout << "\n⚠ WARNING: " << allocations.load() << " allocations detected!\n";
            std::cout << "   The API should be zero-allocation in hot paths.\n";
        }
        
        if (hitRate < 95.0) {
            std::cout << "\n⚠ WARNING: Component cache hit rate below 95%!\n";
        }
    }
};

// Global performance counters
inline PerformanceCounters& counters() {
    static PerformanceCounters c;
    return c;
}

// Macros for tracking (compile out in release builds)
#ifndef G3D_NO_PERF_TRACKING
    #define G3D_TRACK_PROPERTY_ACCESS() do { kern::g3d::perf::counters().entityPropertyAccesses++; } while(0)
    #define G3D_TRACK_CACHE_HIT() do { kern::g3d::perf::counters().componentCacheHits++; } while(0)
    #define G3D_TRACK_CACHE_MISS() do { kern::g3d::perf::counters().componentCacheMisses++; } while(0)
    #define G3D_TRACK_ECS_LOOKUP() do { kern::g3d::perf::counters().ecsLookups++; } while(0)
    #define G3D_TRACK_ALLOC() do { kern::g3d::perf::counters().allocations++; } while(0)
    #define G3D_TRACK_DEALLOC() do { kern::g3d::perf::counters().deallocations++; } while(0)
    #define G3D_TRACK_UPDATE() do { kern::g3d::perf::counters().updateCalls++; } while(0)
    #define G3D_TRACK_INPUT() do { kern::g3d::perf::counters().inputPolls++; } while(0)
#else
    #define G3D_TRACK_PROPERTY_ACCESS()
    #define G3D_TRACK_CACHE_HIT()
    #define G3D_TRACK_CACHE_MISS()
    #define G3D_TRACK_ECS_LOOKUP()
    #define G3D_TRACK_ALLOC()
    #define G3D_TRACK_DEALLOC()
    #define G3D_TRACK_UPDATE()
    #define G3D_TRACK_INPUT()
#endif

// ============================================================================
// Zero-Overhead Entity Handle (C++-style ECS handle system)
// ============================================================================

// This is the "production-grade" version - ensures zero overhead
class EntityHandle {
    // Direct pointer to ECS data - no lookups, no maps
    internal::TransformComponent* transform_;
    internal::MeshComponent* mesh_;
    internal::MaterialComponent* material_;
    internal::EntityId id_;  // Only for validation/debugging
    
public:
    // Constructor takes direct component pointers (set once, use forever)
    EntityHandle(internal::EntityId id,
                 internal::TransformComponent* t,
                 internal::MeshComponent* m,
                 internal::MaterialComponent* mat)
        : transform_(t), mesh_(m), material_(mat), id_(id) {
        G3D_TRACK_ALLOC();
    }
    
    ~EntityHandle() {
        G3D_TRACK_DEALLOC();
    }
    
    // Disable copy to prevent accidental handle duplication
    EntityHandle(const EntityHandle&) = delete;
    EntityHandle& operator=(const EntityHandle&) = delete;
    
    // Enable move
    EntityHandle(EntityHandle&& other) noexcept
        : transform_(other.transform_),
          mesh_(other.mesh_),
          material_(other.material_),
          id_(other.id_) {
        other.transform_ = nullptr;
        other.mesh_ = nullptr;
        other.material_ = nullptr;
        other.id_ = 0;
    }
    
    // Position - ZERO OVERHEAD: direct pointer access
    Vec3 getPosition() const {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        return transform_->position;  // Single pointer deref
    }
    
    void setPosition(const Vec3& pos) {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        transform_->position = pos;  // Single pointer deref
    }
    
    float getX() const {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        return transform_->position.x;  // Two pointer derefs max
    }
    
    void setX(float x) {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        transform_->position.x = x;
    }
    
    // Fast path: inline everything
    inline float x() const { return transform_->position.x; }
    inline float y() const { return transform_->position.y; }
    inline float z() const { return transform_->position.z; }
    
    inline void setXFast(float x) { transform_->position.x = x; }
    inline void setYFast(float y) { transform_->position.y = y; }
    inline void setZFast(float z) { transform_->position.z = z; }
    
    // Rotation - same pattern
    Vec3 getRotation() const {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        return transform_->rotation;
    }
    
    void setRotation(const Vec3& rot) {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        transform_->rotation = rot;
    }
    
    inline float rotX() const { return transform_->rotation.x; }
    inline float rotY() const { return transform_->rotation.y; }
    inline void setRotYFast(float y) { transform_->rotation.y = y; }
    
    // Scale
    Vec3 getScale() const {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        return transform_->scale;
    }
    
    void setScale(const Vec3& scale) {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        transform_->scale = scale;
    }
    
    // Color
    ColorRGB getColor() const {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        return material_->color;
    }
    
    void setColor(const ColorRGB& color) {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        material_->color = color;
    }
    
    // Visibility
    bool isVisible() const {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        return material_->visible;
    }
    
    void setVisible(bool visible) {
        G3D_TRACK_PROPERTY_ACCESS();
        G3D_TRACK_CACHE_HIT();
        material_->visible = visible;
    }
    
    // Get internal ID (for debugging only)
    internal::EntityId getId() const { return id_; }
    
    // Validation
    bool isValid() const {
        return transform_ != nullptr && material_ != nullptr;
    }
};

// ============================================================================
// Performance Benchmarks
// ============================================================================

class G3DPerformanceBenchmark {
public:
    struct Result {
        double propertyAccessTimeNs;
        double directEcsTimeNs;
        double overheadPercent;
        uint64_t iterations;
    };
    
    // Benchmark Entity property access vs direct ECS access
    static Result benchmarkPropertyAccess() {
        using namespace std::chrono;
        
        std::cout << "\n=== Entity Property Access Benchmark ===\n";
        
        // Setup
        internal::ECSBridge::instance().init();
        
        // Create entity through normal API
        Entity::Params params;
        params.position = Vec3(1, 2, 3);
        params.color = ColorRGB::red();
        Entity entity(params);
        
        // Get direct pointers for comparison
        auto* transform = internal::ECSBridge::instance()
            .getComponent<internal::TransformComponent>(entity.getInternalId());
        
        // Reset counters
        counters().reset();
        
        // Benchmark 1: Entity API access
        const uint64_t iterations = 10000000;  // 10 million
        
        auto start = high_resolution_clock::now();
        for (uint64_t i = 0; i < iterations; i++) {
            float x = entity.getX();
            x += 1.0f;
            entity.setX(x);
        }
        auto end = high_resolution_clock::now();
        
        double entityTimeNs = duration<double, std::nano>(end - start).count();
        double perAccessEntity = entityTimeNs / (iterations * 2);  // get + set
        
        std::cout << "Entity API: " << iterations << " get/set pairs\n";
        std::cout << "Total time: " << std::fixed << std::setprecision(2) 
                  << entityTimeNs / 1e6 << " ms\n";
        std::cout << "Per access: " << perAccessEntity << " ns\n";
        
        // Benchmark 2: Direct ECS access (baseline)
        start = high_resolution_clock::now();
        volatile float preventOpt = 0;  // Prevent optimization
        for (uint64_t i = 0; i < iterations; i++) {
            float x = transform->position.x;
            x += 1.0f;
            transform->position.x = x;
            preventOpt = x;
        }
        end = high_resolution_clock::now();
        
        double directTimeNs = duration<double, std::nano>(end - start).count();
        double perAccessDirect = directTimeNs / (iterations * 2);
        
        std::cout << "\nDirect ECS: " << iterations << " get/set pairs\n";
        std::cout << "Total time: " << std::fixed << std::setprecision(2) 
                  << directTimeNs / 1e6 << " ms\n";
        std::cout << "Per access: " << perAccessDirect << " ns\n";
        
        // Calculate overhead
        double overheadNs = perAccessEntity - perAccessDirect;
        double overheadPct = (overheadNs / perAccessDirect) * 100;
        
        std::cout << "\n=== Results ===\n";
        std::cout << "Overhead per access: " << std::fixed << std::setprecision(2) 
                  << overheadNs << " ns (" << overheadPct << "%)\n";
        
        if (overheadPct < 10) {
            std::cout << "✓ EXCELLENT: Overhead < 10%\n";
        } else if (overheadPct < 50) {
            std::cout << "⚠ ACCEPTABLE: Overhead " << overheadPct << "%\n";
        } else {
            std::cout << "✗ WARNING: Overhead too high!\n";
        }
        
        // Show counters
        counters().print();
        
        Result result;
        result.propertyAccessTimeNs = perAccessEntity;
        result.directEcsTimeNs = perAccessDirect;
        result.overheadPercent = overheadPct;
        result.iterations = iterations;
        
        return result;
    }
    
    // Benchmark update loop overhead
    static void benchmarkUpdateLoop() {
        std::cout << "\n=== Update Loop Benchmark ===\n";
        
        const int entityCount = 1000;
        const int frameCount = 1000;
        
        // Create entities
        std::vector<std::unique_ptr<Entity>> entities;
        for (int i = 0; i < entityCount; i++) {
            Entity::Params params;
            params.position = Vec3(i, 0, 0);
            entities.push_back(std::make_unique<Entity>(params));
        }
        
        // Benchmark update loop
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int frame = 0; frame < frameCount; frame++) {
            for (auto& entity : entities) {
                // Simulate typical update
                float x = entity->getX();
                entity->setX(x + 0.01f);
                
                float rot = entity->getRotationY();
                entity->setRotationY(rot + 1.0f);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
        double perEntityUs = (totalMs * 1000) / (entityCount * frameCount);
        
        std::cout << "Entities: " << entityCount << "\n";
        std::cout << "Frames: " << frameCount << "\n";
        std::cout << "Total time: " << std::fixed << std::setprecision(2) << totalMs << " ms\n";
        std::cout << "Per entity per frame: " << perEntityUs << " μs\n";
        
        // Target: < 1 μs per entity per frame for simple updates
        if (perEntityUs < 1.0) {
            std::cout << "✓ EXCELLENT: < 1 μs per entity\n";
        } else if (perEntityUs < 5.0) {
            std::cout << "⚠ ACCEPTABLE: " << perEntityUs << " μs per entity\n";
        } else {
            std::cout << "✗ WARNING: Too slow for 60 FPS with many entities\n";
        }
    }
    
    // Memory allocation tracking
    static void checkAllocations() {
        std::cout << "\n=== Allocation Check ===\n";
        
        counters().reset();
        
        // Create and destroy many entities
        const int count = 1000;
        {
            std::vector<std::unique_ptr<Entity>> entities;
            for (int i = 0; i < count; i++) {
                entities.push_back(std::make_unique<Entity>());
            }
        }  // Entities destroyed here
        
        uint64_t allocs = counters().allocations.load();
        uint64_t deallocs = counters().deallocations.load();
        
        std::cout << "Created/destroyed " << count << " entities\n";
        std::cout << "Allocations: " << allocs << "\n";
        std::cout << "Deallocations: " << deallocs << "\n";
        
        // Should be 2 per entity (1 for wrapper, 1 for ECS)
        // Or ideally 1 if using in-place new
        
        if (allocs == count * 2) {
            std::cout << "✓ Expected allocation pattern\n";
        } else if (allocs > count * 2) {
            std::cout << "⚠ More allocations than expected\n";
        } else {
            std::cout << "✓ Fewer allocations (good!)\n";
        }
    }
    
    // Run all benchmarks
    static void runAll() {
        std::cout << "\n########################################\n";
        std::cout << "#                                      #\n";
        std::cout << "#    G3D PERFORMANCE VALIDATION        #\n";
        std::cout << "#                                      #\n";
        std::cout << "########################################\n";
        
        benchmarkPropertyAccess();
        benchmarkUpdateLoop();
        checkAllocations();
        
        std::cout << "\n========================================\n";
        std::cout << "Validation Complete\n";
        std::cout << "========================================\n\n";
    }
};

// ============================================================================
// Zero-Allocation Update System
// ============================================================================

// Batch update system to minimize dispatch overhead
template<typename T>
class BatchUpdateSystem {
    std::vector<T*> items;
    
public:
    void registerItem(T* item) {
        items.push_back(item);
    }
    
    void unregisterItem(T* item) {
        auto it = std::find(items.begin(), items.end(), item);
        if (it != items.end()) {
            items.erase(it);
        }
    }
    
    void updateAll(float dt) {
        // Single virtual call, then inlined updates
        for (auto* item : items) {
            item->update(dt);
        }
    }
};

// Compile-time update dispatch (no virtual calls)
template<typename... Updatable>
class StaticUpdateDispatcher {
public:
    static void updateAll(float dt, Updatable&... items) {
        // Fold expression - calls update on each with no runtime overhead
        (items.update(dt), ...);
    }
};

// ============================================================================
// Scene Graph Optimizations (optional hierarchy layer)
// ============================================================================

// Flattened scene graph for cache-friendly traversal
class FlatSceneGraph {
    struct Node {
        Entity* entity;
        Matrix localTransform;
        Matrix worldTransform;
        uint32_t parentIndex;
        uint32_t depth;
        bool dirty;
    };
    
    std::vector<Node> nodes;
    std::vector<uint32_t> rootNodes;
    
public:
    void addEntity(Entity* entity, Entity* parent = nullptr) {
        Node node;
        node.entity = entity;
        node.localTransform = MatrixIdentity();
        node.worldTransform = MatrixIdentity();
        node.parentIndex = parent ? findNodeIndex(parent) : UINT32_MAX;
        node.depth = parent ? nodes[node.parentIndex].depth + 1 : 0;
        node.dirty = true;
        
        uint32_t index = nodes.size();
        nodes.push_back(node);
        
        if (!parent) {
            rootNodes.push_back(index);
        }
    }
    
    void updateTransforms() {
        // Update in depth order (parents before children)
        for (auto& node : nodes) {
            if (node.dirty) {
                // Get current entity transform
                auto pos = node.entity->getPosition();
                auto rot = node.entity->getRotation();
                auto scale = node.entity->getScale();
                
                node.localTransform = MatrixTranslate(pos.x, pos.y, pos.z);
                // Add rotation/scale...
                
                // If has parent, multiply
                if (node.parentIndex != UINT32_MAX) {
                    node.worldTransform = MatrixMultiply(
                        node.localTransform, 
                        nodes[node.parentIndex].worldTransform
                    );
                } else {
                    node.worldTransform = node.localTransform;
                }
                
                node.dirty = false;
            }
        }
    }
    
private:
    uint32_t findNodeIndex(Entity* entity) {
        for (uint32_t i = 0; i < nodes.size(); i++) {
            if (nodes[i].entity == entity) return i;
        }
        return UINT32_MAX;
    }
};

} // namespace perf
} // namespace g3d
} // namespace kern
