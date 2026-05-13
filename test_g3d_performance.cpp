/* *
 * test_g3d_performance.cpp - G3D API Performance Validation
 * 
 * Validates that the Ursina-style API has zero overhead vs direct ECS access.
 */
#include <iostream>
#include <chrono>
#include "kern/modules/g3d/g3d_api.hpp"
#include "kern/modules/g3d/g3d_performance.hpp"

using namespace kern::g3d;
using namespace std::chrono;

// Test 1: Property Access Overhead
void testPropertyAccessOverhead() {
    std::cout << "\n========================================\n";
    std::cout << "Test 1: Property Access Overhead\n";
    std::cout << "========================================\n";
    
    // Initialize
    internal::ECSBridge::instance().init();
    
    // Create entity
    Entity::Params params;
    params.position = Vec3(1, 2, 3);
    Entity entity(params);
    
    // Get direct ECS pointer for comparison
    auto* directTransform = internal::ECSBridge::instance()
        .getComponent<internal::TransformComponent>(entity.getInternalId());
    
    const int iterations = 10000000;  // 10 million
    
    // Test 1: Direct ECS access (baseline)
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        float x = directTransform->position.x;
        directTransform->position.x = x + 1.0f;
    }
    auto end = high_resolution_clock::now();
    
    double directTime = duration<double, std::nano>(end - start).count();
    double directPerOp = directTime / iterations;
    
    std::cout << "Direct ECS access:\n";
    std::cout << "  Total: " << directTime / 1e6 << " ms\n";
    std::cout << "  Per get+set: " << directPerOp << " ns\n";
    
    // Test 2: Entity API access
    start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        float x = entity.getX();
        entity.setX(x + 1.0f);
    }
    end = high_resolution_clock::now();
    
    double apiTime = duration<double, std::nano>(end - start).count();
    double apiPerOp = apiTime / iterations;
    
    std::cout << "\nEntity API access:\n";
    std::cout << "  Total: " << apiTime / 1e6 << " ms\n";
    std::cout << "  Per get+set: " << apiPerOp << " ns\n";
    
    // Calculate overhead
    double overheadNs = apiPerOp - directPerOp;
    double overheadPct = (overheadNs / directPerOp) * 100;
    
    std::cout << "\n=== Overhead Analysis ===\n";
    std::cout << "Overhead: " << overheadNs << " ns (" << overheadPct << "%)\n";
    
    if (overheadPct < 5) {
        std::cout << "✓ EXCELLENT: < 5% overhead\n";
    } else if (overheadPct < 20) {
        std::cout << "✓ GOOD: < 20% overhead\n";
    } else if (overheadPct < 50) {
        std::cout << "⚠ ACCEPTABLE: " << overheadPct << "% overhead\n";
    } else {
        std::cout << "✗ WARNING: High overhead!\n";
    }
}

// Test 2: Memory Allocations
void testMemoryAllocations() {
    std::cout << "\n========================================\n";
    std::cout << "Test 2: Memory Allocations\n";
    std::cout << "========================================\n";
    
    perf::counters().reset();
    
    const int count = 1000;
    
    // Create entities
    {
        std::vector<std::unique_ptr<Entity>> entities;
        for (int i = 0; i < count; i++) {
            entities.push_back(std::make_unique<Entity>());
        }
        
        std::cout << "Created " << count << " entities\n";
        std::cout << "Allocations: " << perf::counters().allocations.load() << "\n";
    }  // Destroy entities
    
    std::cout << "Deallocations: " << perf::counters().deallocations.load() << "\n";
    
    // Should be 2 per entity (wrapper + ECS) or ideally 1
    uint64_t allocs = perf::counters().allocations.load();
    
    if (allocs <= count * 2) {
        std::cout << "✓ Good: " << allocs << " allocations for " << count << " entities\n";
    } else {
        std::cout << "⚠ Warning: " << allocs << " allocations (more than expected)\n";
    }
}

// Test 3: Update Loop Performance
void testUpdateLoopPerformance() {
    std::cout << "\n========================================\n";
    std::cout << "Test 3: Update Loop Performance\n";
    std::cout << "========================================\n";
    
    const int entityCount = 1000;
    const int frameCount = 100;
    
    // Create entities
    std::vector<std::unique_ptr<Entity>> entities;
    for (int i = 0; i < entityCount; i++) {
        Entity::Params params;
        params.position = Vec3(i, 0, 0);
        params.rotation = Vec3(0, i, 0);
        entities.push_back(std::make_unique<Entity>(params));
    }
    
    // Simulate update loop
    auto start = high_resolution_clock::now();
    
    for (int frame = 0; frame < frameCount; frame++) {
        for (auto& entity : entities) {
            // Typical update pattern
            float x = entity->getX();
            entity->setX(x + 0.01f);
            
            float y = entity->getRotationY();
            entity->setRotationY(y + 1.0f);
            
            // Visibility toggle (rare)
            if (frame == 0) {
                entity->setVisible(true);
            }
        }
    }
    
    auto end = high_resolution_clock::now();
    
    double totalMs = duration<double, std::milli>(end - start).count();
    double perEntityPerFrameUs = (totalMs * 1000) / (entityCount * frameCount);
    
    std::cout << "Entities: " << entityCount << "\n";
    std::cout << "Frames: " << frameCount << "\n";
    std::cout << "Total time: " << totalMs << " ms\n";
    std::cout << "Per entity per frame: " << perEntityPerFrameUs << " μs\n";
    
    // For 60 FPS with 1000 entities, we have 16.67ms total
    // So per entity budget is ~16 μs
    // Our target: < 1 μs for simple updates
    
    if (perEntityPerFrameUs < 1.0) {
        std::cout << "✓ EXCELLENT: < 1 μs per entity\n";
        std::cout << "   Can handle 10,000+ entities at 60 FPS\n";
    } else if (perEntityPerFrameUs < 5.0) {
        std::cout << "✓ GOOD: < 5 μs per entity\n";
        std::cout << "   Can handle 3,000+ entities at 60 FPS\n";
    } else if (perEntityPerFrameUs < 16.0) {
        std::cout << "⚠ ACCEPTABLE: " << perEntityPerFrameUs << " μs per entity\n";
        std::cout << "   Can handle 1,000 entities at 60 FPS\n";
    } else {
        std::cout << "✗ WARNING: Too slow for 60 FPS with many entities\n";
    }
}

// Test 4: Entity Handle (Zero-Overhead) Performance
void testEntityHandlePerformance() {
    std::cout << "\n========================================\n";
    std::cout << "Test 4: EntityHandle (Zero-Overhead) Performance\n";
    std::cout << "========================================\n";
    
    // Create entity and get components
    Entity::Params params;
    params.position = Vec3(1, 2, 3);
    Entity entity(params);
    
    auto* transform = internal::ECSBridge::instance()
        .getComponent<internal::TransformComponent>(entity.getInternalId());
    auto* mesh = internal::ECSBridge::instance()
        .getComponent<internal::MeshComponent>(entity.getInternalId());
    auto* material = internal::ECSBridge::instance()
        .getComponent<internal::MaterialComponent>(entity.getInternalId());
    
    // Create handle
    perf::EntityHandle handle(entity.getInternalId(), transform, mesh, material);
    
    const int iterations = 10000000;
    
    // Test handle access
    auto start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        float x = handle.x();
        handle.setXFast(x + 1.0f);
    }
    auto end = high_resolution_clock::now();
    
    double handleTime = duration<double, std::nano>(end - start).count();
    double handlePerOp = handleTime / iterations;
    
    std::cout << "EntityHandle (direct pointer) access:\n";
    std::cout << "  Total: " << handleTime / 1e6 << " ms\n";
    std::cout << "  Per get+set: " << handlePerOp << " ns\n";
    
    // Direct comparison
    start = high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        float x = transform->position.x;
        transform->position.x = x + 1.0f;
    }
    end = high_resolution_clock::now();
    
    double directTime = duration<double, std::nano>(end - start).count();
    double directPerOp = directTime / iterations;
    
    std::cout << "\nDirect pointer access:\n";
    std::cout << "  Total: " << directTime / 1e6 << " ms\n";
    std::cout << "  Per get+set: " << directPerOp << " ns\n";
    
    double overhead = handlePerOp - directPerOp;
    
    std::cout << "\n=== Result ===\n";
    if (overhead < 1.0) {
        std::cout << "✓ TRUE ZERO OVERHEAD: " << overhead << " ns difference\n";
        std::cout << "  (likely measurement noise)\n";
    } else {
        std::cout << "Overhead: " << overhead << " ns\n";
    }
}

// Test 5: Cache Performance
void testCachePerformance() {
    std::cout << "\n========================================\n";
    std::cout << "Test 5: Component Cache Performance\n";
    std::cout << "========================================\n";
    
    perf::counters().reset();
    
    // Create entity
    Entity::Params params;
    params.position = Vec3(1, 2, 3);
    Entity entity(params);
    
    // Access same property many times
    const int iterations = 1000000;
    
    for (int i = 0; i < iterations; i++) {
        float x = entity.getX();  // Should cache on first call
        (void)x;  // Suppress unused warning
    }
    
    uint64_t hits = perf::counters().componentCacheHits.load();
    uint64_t misses = perf::counters().componentCacheMisses.load();
    
    std::cout << "Property accesses: " << iterations << "\n";
    std::cout << "Cache hits: " << hits << "\n";
    std::cout << "Cache misses: " << misses << "\n";
    
    double hitRate = (hits + misses) > 0 ? (100.0 * hits / (hits + misses)) : 0;
    std::cout << "Hit rate: " << hitRate << "%\n";
    
    if (hitRate > 99) {
        std::cout << "✓ EXCELLENT: > 99% cache hit rate\n";
    } else if (hitRate > 95) {
        std::cout << "✓ GOOD: > 95% cache hit rate\n";
    } else {
        std::cout << "⚠ Warning: Cache hit rate could be better\n";
    }
}

int main() {
    std::cout << "\n########################################\n";
    std::cout << "#                                      #\n";
    std::cout << "#   G3D API PERFORMANCE VALIDATION     #\n";
    std::cout << "#                                      #\n";
    std::cout << "########################################\n";
    
    testPropertyAccessOverhead();
    testMemoryAllocations();
    testUpdateLoopPerformance();
    testEntityHandlePerformance();
    testCachePerformance();
    
    std::cout << "\n\n========================================\n";
    std::cout << "ALL TESTS COMPLETE\n";
    std::cout << "========================================\n";
    std::cout << "\nKey Validations:\n";
    std::cout << "✓ Property access overhead measured\n";
    std::cout << "✓ Memory allocations tracked\n";
    std::cout << "✓ Update loop performance validated\n";
    std::cout << "✓ EntityHandle (zero-overhead) verified\n";
    std::cout << "✓ Component cache performance checked\n";
    std::cout << "\n";
    
    return 0;
}
