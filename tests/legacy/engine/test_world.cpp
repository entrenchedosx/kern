/* *
 * tests/engine/test_world.cpp - World & Component System Tests
 * 
 * Week 2 Validation Tests
 */

#include <gtest/gtest.h>
#include "kern/engine/core/world.h"
#include <glm/glm.hpp>

using namespace kern::engine;

// ═══════════════════════════════════════════════════════════════════════════════
// BASIC WORLD TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(WorldBasic, CreateEntity) {
    World world;
    
    EntityId e = world.createEntity("Test");
    
    EXPECT_TRUE(world.isAlive(e));
    EXPECT_EQ(world.entityCount(), 1);
    
    // Every entity has Transform
    Transform* trans = world.getComponent<Transform>(e);
    EXPECT_NE(trans, nullptr);
}

TEST(WorldBasic, DestroyEntity) {
    World world;
    
    EntityId e = world.createEntity();
    world.destroyEntity(e);
    
    EXPECT_FALSE(world.isAlive(e));
    EXPECT_EQ(world.entityCount(), 0);
}

TEST(WorldBasic, EntityHasTransformByDefault) {
    World world;
    
    EntityId e = world.createEntity();
    
    EXPECT_TRUE(world.hasComponent<Transform>(e));
    
    Transform* trans = world.getComponent<Transform>(e);
    EXPECT_NE(trans, nullptr);
    
    // Default values
    EXPECT_EQ(trans->localPosition, glm::vec3(0));
    EXPECT_EQ(trans->localScale, glm::vec3(1));
}

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT TESTS
// ═══════════════════════════════════════════════════════════════════════════════

struct TestComponent {
    int value = 42;
    float data = 3.14f;
};

TEST(WorldComponent, AddComponent) {
    World world;
    EntityId e = world.createEntity();
    
    TestComponent* comp = world.addComponent<TestComponent>(e);
    
    EXPECT_NE(comp, nullptr);
    EXPECT_EQ(comp->value, 42);
    EXPECT_TRUE(world.hasComponent<TestComponent>(e));
}

TEST(WorldComponent, GetComponent) {
    World world;
    EntityId e = world.createEntity();
    
    TestComponent* added = world.addComponent<TestComponent>(e, 100, 2.5f);
    TestComponent* retrieved = world.getComponent<TestComponent>(e);
    
    EXPECT_EQ(added, retrieved);
    EXPECT_EQ(retrieved->value, 100);
    EXPECT_FLOAT_EQ(retrieved->data, 2.5f);
}

TEST(WorldComponent, RemoveComponent) {
    World world;
    EntityId e = world.createEntity();
    
    world.addComponent<TestComponent>(e);
    EXPECT_TRUE(world.hasComponent<TestComponent>(e));
    
    world.removeComponent<TestComponent>(e);
    EXPECT_FALSE(world.hasComponent<TestComponent>(e));
    EXPECT_EQ(world.getComponent<TestComponent>(e), nullptr);
}

TEST(WorldComponent, GetNonExistent) {
    World world;
    EntityId e = world.createEntity();
    
    TestComponent* comp = world.getComponent<TestComponent>(e);
    EXPECT_EQ(comp, nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT ITERATION TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(WorldIteration, QuerySingleComponent) {
    World world;
    
    EntityId e1 = world.createEntity();
    EntityId e2 = world.createEntity();
    EntityId e3 = world.createEntity();
    
    world.addComponent<TestComponent>(e1, 10);
    world.addComponent<TestComponent>(e2, 20);
    // e3 has no TestComponent
    
    int count = 0;
    int sum = 0;
    
    for (auto [entity, comp] : world.query<TestComponent>()) {
        count++;
        sum += comp.value;
    }
    
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sum, 30);
}

TEST(WorldIteration, ForEach) {
    World world;
    
    world.createEntity();
    world.createEntity();
    world.createEntity();
    
    int count = 0;
    world.forEach<Transform>([&count](EntityId, Transform&) {
        count++;
    });
    
    EXPECT_EQ(count, 3);
}

// ═══════════════════════════════════════════════════════════════════════════════
// HIERARCHY TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(WorldHierarchy, SetParent) {
    World world;
    
    EntityId parent = world.createEntity("Parent");
    EntityId child = world.createEntity("Child");
    
    world.setParent(child, parent);
    
    EXPECT_EQ(world.getParent(child), parent);
    
    const auto& children = world.getChildren(parent);
    EXPECT_EQ(children.size(), 1);
    EXPECT_EQ(children[0], child);
}

TEST(WorldHierarchy, Unparent) {
    World world;
    
    EntityId parent = world.createEntity();
    EntityId child = world.createEntity();
    
    world.setParent(child, parent);
    world.unparent(child);
    
    EXPECT_EQ(world.getParent(child), INVALID_ENTITY);
    EXPECT_TRUE(world.getChildren(parent).empty());
}

TEST(WorldHierarchy, TransformPropagation) {
    World world;
    
    EntityId parent = world.createEntity();
    EntityId child = world.createEntity();
    
    Transform* pTrans = world.getComponent<Transform>(parent);
    Transform* cTrans = world.getComponent<Transform>(child);
    
    // Set parent position
    pTrans->setLocalPosition(glm::vec3(10, 0, 0));
    
    // Parent child
    world.setParent(child, parent);
    
    // Update transforms
    world.updateTransforms();
    
    // Child should be at parent's position
    EXPECT_EQ(cTrans->getWorldPosition(), glm::vec3(10, 0, 0));
}

TEST(WorldHierarchy, CascadeTransform) {
    World world;
    
    // Grandparent → Parent → Child
    EntityId grandparent = world.createEntity();
    EntityId parent = world.createEntity();
    EntityId child = world.createEntity();
    
    Transform* gTrans = world.getComponent<Transform>(grandparent);
    gTrans->setLocalPosition(glm::vec3(10, 0, 0));
    
    world.setParent(parent, grandparent);
    world.setParent(child, parent);
    
    Transform* pTrans = world.getComponent<Transform>(parent);
    pTrans->setLocalPosition(glm::vec3(5, 0, 0));
    
    world.updateTransforms();
    
    Transform* cTrans = world.getComponent<Transform>(child);
    // Child world position = grandparent(10) + parent(5) = 15
    EXPECT_EQ(cTrans->getWorldPosition(), glm::vec3(15, 0, 0));
}

TEST(WorldHierarchy, PreventCircular) {
    World world;
    
    EntityId a = world.createEntity();
    EntityId b = world.createEntity();
    EntityId c = world.createEntity();
    
    world.setParent(b, a);
    world.setParent(c, b);
    
    // This should be prevented - can't parent A to C (would make cycle)
    world.setParent(a, c);
    
    // A should still have no parent
    EXPECT_EQ(world.getParent(a), INVALID_ENTITY);
}

TEST(WorldHierarchy, IsDescendantOf) {
    World world;
    
    EntityId grandparent = world.createEntity();
    EntityId parent = world.createEntity();
    EntityId child = world.createEntity();
    EntityId sibling = world.createEntity();
    
    world.setParent(parent, grandparent);
    world.setParent(child, parent);
    
    EXPECT_TRUE(world.isDescendantOf(child, grandparent));
    EXPECT_TRUE(world.isDescendantOf(child, parent));
    EXPECT_TRUE(world.isDescendantOf(parent, grandparent));
    
    EXPECT_FALSE(world.isDescendantOf(grandparent, child));
    EXPECT_FALSE(world.isDescendantOf(child, sibling));
    EXPECT_FALSE(world.isDescendantOf(parent, parent));  // Self
}

TEST(WorldHierarchy, DestroyParentDestroysChildren) {
    World world;
    
    EntityId parent = world.createEntity();
    EntityId child = world.createEntity();
    EntityId grandchild = world.createEntity();
    
    world.setParent(child, parent);
    world.setParent(grandchild, child);
    
    world.destroyEntity(parent);
    
    EXPECT_FALSE(world.isAlive(parent));
    EXPECT_FALSE(world.isAlive(child));
    EXPECT_FALSE(world.isAlive(grandchild));
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSFORM WORLD/LOCAL TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(WorldTransform, SetWorldPositionMaintainsWorldPos) {
    World world;
    
    EntityId parent = world.createEntity();
    EntityId child = world.createEntity();
    
    Transform* pTrans = world.getComponent<Transform>(parent);
    pTrans->setLocalPosition(glm::vec3(10, 0, 0));
    
    world.setParent(child, parent);
    world.updateTransforms();
    
    // Set child's world position to (20, 0, 0)
    world.setWorldPosition(child, glm::vec3(20, 0, 0));
    
    // World position should be exactly what we set
    Transform* cTrans = world.getComponent<Transform>(child);
    EXPECT_EQ(cTrans->getWorldPosition(), glm::vec3(20, 0, 0));
    
    // Local should be (10, 0, 0) to achieve world (20, 0, 0) when parent is at (10, 0, 0)
    EXPECT_EQ(cTrans->localPosition, glm::vec3(10, 0, 0));
}

TEST(WorldTransform, GetWorldPosition) {
    World world;
    
    EntityId e = world.createEntity();
    Transform* trans = world.getComponent<Transform>(e);
    
    trans->setLocalPosition(glm::vec3(5, 10, 15));
    world.updateTransforms();
    
    EXPECT_EQ(world.getWorldPosition(e), glm::vec3(5, 10, 15));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SYSTEM TESTS
// ═══════════════════════════════════════════════════════════════════════════════

class TestSystem : public System {
public:
    int updateCount = 0;
    float lastDeltaTime = 0;
    
    void update(World& world, float deltaTime) override {
        updateCount++;
        lastDeltaTime = deltaTime;
    }
};

TEST(WorldSystem, AddAndUpdate) {
    World world;
    
    auto system = std::make_unique<TestSystem>();
    TestSystem* sysPtr = system.get();
    
    world.addSystem(std::move(system));
    
    world.start();
    world.update(0.016f);
    
    EXPECT_EQ(sysPtr->updateCount, 1);
    EXPECT_FLOAT_EQ(sysPtr->lastDeltaTime, 0.016f);
}

TEST(WorldSystem, PriorityOrder) {
    World world;
    
    std::vector<int> order;
    
    struct OrderedSystem : public System {
        std::vector<int>& order;
        int id;
        OrderedSystem(std::vector<int>& o, int i, int p) : order(o), id(i) {
            priority = p;
        }
        void update(World&, float) override {
            order.push_back(id);
        }
    };
    
    world.addSystem(std::make_unique<OrderedSystem>(order, 1, 10));
    world.addSystem(std::make_unique<OrderedSystem>(order, 2, 5));
    world.addSystem(std::make_unique<OrderedSystem>(order, 3, 20));
    
    world.start();
    world.update(0.016f);
    
    // Should be ordered by priority: 2 (5), 1 (10), 3 (20)
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(order[2], 3);
}

// ═══════════════════════════════════════════════════════════════════════════════
// MULTI-COMPONENT QUERY TESTS
// ═══════════════════════════════════════════════════════════════════════════════

struct Position {
    float x, y, z;
};

struct Velocity {
    float vx, vy, vz;
};

TEST(WorldQuery, QueryMultipleComponents) {
    World world;
    
    // Entity with both Position and Velocity
    EntityId moving = world.createEntity();
    world.addComponent<Position>(moving, 1.0f, 2.0f, 3.0f);
    world.addComponent<Velocity>(moving, 0.1f, 0.2f, 0.3f);
    
    // Entity with only Position
    EntityId stationary = world.createEntity();
    world.addComponent<Position>(stationary, 10.0f, 20.0f, 30.0f);
    
    // Entity with neither
    world.createEntity();
    
    // Query entities with both Position and Velocity
    auto results = world.queryEntities<Position, Velocity>();
    
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], moving);
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRESS TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(WorldStress, ManyEntities) {
    World world;
    
    std::vector<EntityId> entities;
    entities.reserve(1000);
    
    for (int i = 0; i < 1000; i++) {
        entities.push_back(world.createEntity());
    }
    
    EXPECT_EQ(world.entityCount(), 1000);
    
    // Add components to half
    for (int i = 0; i < 500; i++) {
        world.addComponent<TestComponent>(entities[i]);
    }
    
    // Query all with component
    int count = 0;
    for (auto [e, comp] : world.query<TestComponent>()) {
        count++;
    }
    EXPECT_EQ(count, 500);
}

TEST(WorldStress, DeepHierarchy) {
    World world;
    
    // Create chain: 1 → 2 → 3 → ... → 100
    EntityId root = world.createEntity();
    EntityId current = root;
    
    for (int i = 0; i < 99; i++) {
        EntityId child = world.createEntity();
        world.setParent(child, current);
        current = child;
    }
    
    // Update transforms (should handle deep recursion)
    world.updateTransforms();
    
    // All should have valid transforms
    world.forEach<Transform>([](EntityId, Transform& t) {
        EXPECT_FALSE(t.isDirty());
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
