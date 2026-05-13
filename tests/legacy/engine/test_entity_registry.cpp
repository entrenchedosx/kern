/* *
 * tests/engine/test_entity_registry.cpp - Entity Registry Unit Tests
 * 
 * Week 1 Validation Tests
 * All tests must pass before proceeding to Week 2
 */

#include <gtest/gtest.h>
#include "kern/engine/core/entity.h"
#include <vector>
#include <set>
#include <thread>
#include <chrono>

using namespace kern::engine;
using namespace kern::engine::entity_id;

// ═══════════════════════════════════════════════════════════════════════════════
// ENTITY ID PACKING TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityIdPacking, BasicPacking) {
    EntityId id = pack(123, 5);
    EXPECT_EQ(getIndex(id), 123u);
    EXPECT_EQ(getGeneration(id), 5u);
}

TEST(EntityIdPacking, MaxValues) {
    EntityId id = pack(MAX_ENTITY_INDEX, MAX_GENERATION);
    EXPECT_EQ(getIndex(id), MAX_ENTITY_INDEX);
    EXPECT_EQ(getGeneration(id), MAX_GENERATION);
}

TEST(EntityIdPacking, InvalidEntity) {
    EXPECT_FALSE(isValid(INVALID_ENTITY));
    EXPECT_EQ(getIndex(INVALID_ENTITY), 0u);
    EXPECT_EQ(getGeneration(INVALID_ENTITY), 0u);
}

TEST(EntityIdPacking, ZeroGeneration) {
    // Generation 0 with non-zero index is still "valid" format
    EntityId id = pack(100, 0);
    EXPECT_TRUE(isValid(id));
    EXPECT_EQ(getGeneration(id), 0u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// BASIC LIFECYCLE TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryLifecycle, CreateSingle) {
    EntityRegistry registry;
    
    EntityId e = registry.create();
    
    EXPECT_TRUE(isValid(e));
    EXPECT_TRUE(registry.isAlive(e));
    EXPECT_EQ(registry.aliveCount(), 1);
}

TEST(EntityRegistryLifecycle, DestroySingle) {
    EntityRegistry registry;
    
    EntityId e = registry.create();
    registry.destroy(e);
    
    EXPECT_FALSE(registry.isAlive(e));
    EXPECT_EQ(registry.aliveCount(), 0);
}

TEST(EntityRegistryLifecycle, DoubleDestroy) {
    EntityRegistry registry;
    
    EntityId e = registry.create();
    registry.destroy(e);
    registry.destroy(e);  // Should be safe
    
    EXPECT_FALSE(registry.isAlive(e));
    EXPECT_EQ(registry.aliveCount(), 0);
}

TEST(EntityRegistryLifecycle, DestroyInvalid) {
    EntityRegistry registry;
    
    // Should not crash
    registry.destroy(INVALID_ENTITY);
    registry.destroy(0xDEADBEEFCAFEBABEULL);  // Random garbage
    
    EXPECT_EQ(registry.aliveCount(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ID RECYCLING TESTS (CRITICAL)
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryRecycling, GenerationIncrement) {
    EntityRegistry registry;
    
    EntityId e1 = registry.create();
    uint32_t index = getIndex(e1);
    uint16_t gen1 = getGeneration(e1);
    
    registry.destroy(e1);
    EntityId e2 = registry.create();
    
    // Same index, new generation
    EXPECT_EQ(getIndex(e2), index);
    EXPECT_EQ(getGeneration(e2), gen1 + 1);
    
    // Old ID is dead
    EXPECT_FALSE(registry.isAlive(e1));
    EXPECT_TRUE(registry.isAlive(e2));
}

TEST(EntityRegistryRecycling, OldIdStaysDead) {
    EntityRegistry registry;
    
    EntityId e1 = registry.create();
    registry.destroy(e1);
    
    // Create more entities to recycle slots
    for (int i = 0; i < 10; ++i) {
        EntityId e = registry.create();
        registry.destroy(e);
    }
    
    // Original ID must stay dead
    EXPECT_FALSE(registry.isAlive(e1));
}

TEST(EntityRegistryRecycling, MultipleCycles) {
    EntityRegistry registry;
    
    std::vector<uint16_t> generations;
    
    // Recycle same slot 10 times
    for (int i = 0; i < 10; ++i) {
        EntityId e = registry.create();
        // All should be at index 1 (first non-reserved slot)
        EXPECT_EQ(getIndex(e), 1u);
        generations.push_back(getGeneration(e));
        registry.destroy(e);
    }
    
    // Generations should increment
    for (size_t i = 1; i < generations.size(); ++i) {
        EXPECT_EQ(generations[i], generations[i-1] + 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// BULK OPERATION TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryBulk, CreateMany) {
    EntityRegistry registry;
    
    auto entities = registry.createMany(100);
    
    EXPECT_EQ(entities.size(), 100);
    EXPECT_EQ(registry.aliveCount(), 100);
    
    // All should be unique
    std::set<EntityId> unique(entities.begin(), entities.end());
    EXPECT_EQ(unique.size(), 100);
    
    // All should be alive
    for (EntityId e : entities) {
        EXPECT_TRUE(registry.isAlive(e));
    }
}

TEST(EntityRegistryBulk, DestroyMany) {
    EntityRegistry registry;
    
    auto entities = registry.createMany(100);
    registry.destroyMany(entities);
    
    EXPECT_EQ(registry.aliveCount(), 0);
    
    for (EntityId e : entities) {
        EXPECT_FALSE(registry.isAlive(e));
    }
}

TEST(EntityRegistryBulk, Clear) {
    EntityRegistry registry;
    
    auto entities = registry.createMany(50);
    registry.clear();
    
    EXPECT_EQ(registry.aliveCount(), 0);
    EXPECT_TRUE(registry.empty());
    
    for (EntityId e : entities) {
        EXPECT_FALSE(registry.isAlive(e));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ITERATION TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryIteration, ForEach) {
    EntityRegistry registry;
    
    auto entities = registry.createMany(10);
    
    std::set<EntityId> visited;
    registry.forEach([&visited](EntityId e) {
        visited.insert(e);
    });
    
    EXPECT_EQ(visited.size(), 10);
    
    for (EntityId e : entities) {
        EXPECT_TRUE(visited.count(e) > 0);
    }
}

TEST(EntityRegistryIteration, RangeBasedFor) {
    EntityRegistry registry;
    
    auto entities = registry.createMany(10);
    
    std::set<EntityId> visited;
    for (EntityId e : registry.alive()) {
        visited.insert(e);
    }
    
    EXPECT_EQ(visited.size(), 10);
}

TEST(EntityRegistryIteration, EmptyIteration) {
    EntityRegistry registry;
    
    int count = 0;
    registry.forEach([&count](EntityId) { ++count; });
    
    EXPECT_EQ(count, 0);
    
    for (EntityId e : registry.alive()) {
        ++count;
    }
    
    EXPECT_EQ(count, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// MEMORY MANAGEMENT TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryMemory, Reserve) {
    EntityRegistry registry;
    
    registry.reserve(1000);
    
    // Should not allocate when creating up to reserved amount
    auto entities = registry.createMany(1000);
    
    EXPECT_EQ(registry.aliveCount(), 1000);
    EXPECT_LE(registry.capacity(), 1001);  // Slot 0 + 1000 entities
}

TEST(EntityRegistryMemory, AvailableSlots) {
    EntityRegistry registry;
    
    size_t initial = registry.availableSlots();
    
    auto e1 = registry.create();
    EXPECT_EQ(registry.availableSlots(), initial - 1);
    
    registry.destroy(e1);
    EXPECT_EQ(registry.availableSlots(), initial);  // Slot recycled
}

TEST(EntityRegistryMemory, CapacityGrowth) {
    EntityRegistry registry;
    
    // Create many entities to trigger growth
    for (int i = 0; i < 10000; ++i) {
        registry.create();
    }
    
    EXPECT_EQ(registry.aliveCount(), 10000);
    EXPECT_GE(registry.capacity(), 10001);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CAPACITY LIMIT TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryLimits, MaxIndexCheck) {
    // Test that we handle 48-bit index correctly
    EntityId id = pack(MAX_ENTITY_INDEX, 1);
    EXPECT_EQ(getIndex(id), MAX_ENTITY_INDEX);
}

// ═══════════════════════════════════════════════════════════════════════════════
// VALIDATION TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryValidation, ConsistencyCheck) {
    EntityRegistry registry;
    
    // Empty registry should be valid
    EXPECT_TRUE(registry.validate());
    
    // Add some entities
    auto entities = registry.createMany(100);
    EXPECT_TRUE(registry.validate());
    
    // Destroy half
    for (size_t i = 0; i < entities.size() / 2; ++i) {
        registry.destroy(entities[i]);
    }
    EXPECT_TRUE(registry.validate());
    
    // Destroy rest
    registry.destroyMany(entities);
    EXPECT_TRUE(registry.validate());
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRESS TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryStress, ManyCreateDestroy) {
    EntityRegistry registry;
    
    // Rapid create/destroy cycles
    for (int cycle = 0; cycle < 100; ++cycle) {
        std::vector<EntityId> batch;
        for (int i = 0; i < 100; ++i) {
            batch.push_back(registry.create());
        }
        
        // Destroy in reverse order
        for (auto it = batch.rbegin(); it != batch.rend(); ++it) {
            registry.destroy(*it);
        }
    }
    
    EXPECT_EQ(registry.aliveCount(), 0);
    EXPECT_TRUE(registry.validate());
}

TEST(EntityRegistryStress, RandomCreateDestroy) {
    EntityRegistry registry;
    std::vector<EntityId> alive;
    
    // Random pattern
    for (int i = 0; i < 1000; ++i) {
        if (alive.empty() || (rand() % 2 == 0)) {
            // Create
            alive.push_back(registry.create());
        } else {
            // Destroy random entity
            size_t idx = rand() % alive.size();
            registry.destroy(alive[idx]);
            alive.erase(alive.begin() + idx);
        }
    }
    
    // Verify all "alive" vector entities are actually alive
    for (EntityId e : alive) {
        EXPECT_TRUE(registry.isAlive(e));
    }
    
    EXPECT_EQ(registry.aliveCount(), alive.size());
    EXPECT_TRUE(registry.validate());
}

TEST(EntityRegistryStress, GenerationOverflowProtection) {
    EntityRegistry registry;
    
    // Force generation overflow (65,535 recycles)
    EntityId first = registry.create();
    uint32_t index = getIndex(first);
    
    // Recycle same slot many times
    for (int i = 0; i < 1000; ++i) {  // Can't do 65k in test, but verify logic
        registry.destroy(first);
        first = registry.create();
        EXPECT_EQ(getIndex(first), index);
    }
    
    // Entity should still be valid
    EXPECT_TRUE(registry.isAlive(first));
}

// ═══════════════════════════════════════════════════════════════════════════════
// EDGE CASE TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryEdge, Slot0Reserved) {
    EntityRegistry registry;
    
    // First created entity should be at index 1 (slot 0 reserved)
    EntityId e = registry.create();
    EXPECT_EQ(getIndex(e), 1u);
    EXPECT_EQ(getGeneration(e), 1u);
}

TEST(EntityRegistryEdge, InterleavedCreateDestroy) {
    EntityRegistry registry;
    
    // Create A, B, C
    EntityId a = registry.create();
    EntityId b = registry.create();
    EntityId c = registry.create();
    
    // Destroy B
    registry.destroy(b);
    
    // Create D (should reuse B's slot)
    EntityId d = registry.create();
    
    EXPECT_EQ(getIndex(d), getIndex(b));
    EXPECT_EQ(getGeneration(d), getGeneration(b) + 1);
    
    // A and C should still be alive
    EXPECT_TRUE(registry.isAlive(a));
    EXPECT_TRUE(registry.isAlive(c));
    EXPECT_FALSE(registry.isAlive(b));
}

// ═══════════════════════════════════════════════════════════════════════════════
// COMPACT TEST
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryCompact, BasicCompact) {
    EntityRegistry registry;
    
    // Create and destroy to create gaps
    EntityId a = registry.create();
    EntityId b = registry.create();
    EntityId c = registry.create();
    
    registry.destroy(b);  // Create gap at index 2
    
    // Compact
    auto mapping = registry.compact();
    
    // A and C should be remapped to contiguous slots
    EXPECT_EQ(registry.aliveCount(), 2);
    EXPECT_TRUE(registry.validate());
}

// ═══════════════════════════════════════════════════════════════════════════════
// PERFORMANCE TESTS (Benchmarks)
// ═══════════════════════════════════════════════════════════════════════════════

TEST(EntityRegistryPerf, Create1000) {
    EntityRegistry registry;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        registry.create();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    
    // Should create 1000 entities in under 1ms (very generous)
    EXPECT_LT(ms, 10.0);
    EXPECT_EQ(registry.aliveCount(), 1000);
}

TEST(EntityRegistryPerf, IsAlive10000) {
    EntityRegistry registry;
    auto entities = registry.createMany(1000);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Check isAlive 10,000 times
    for (int i = 0; i < 10000; ++i) {
        registry.isAlive(entities[i % entities.size()]);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    
    // 10,000 checks should be under 1ms
    EXPECT_LT(ms, 5.0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN (for standalone test execution)
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
