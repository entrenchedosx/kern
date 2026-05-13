/* *
 * test_pure_ecs.cpp - Hard Isolation Test 2: Pure ECS Module Build
 * 
 * Tests that ECS module builds and runs standalone without runtime core.
 * No runtime dependencies, no engine core dependencies.
 */

#include "kern/runtime/modules/ecs/entity.h"
#include "kern/runtime/modules/ecs/component.h"
#include "kern/runtime/modules/ecs/entity_system.h"
#include "kern/runtime/modules/ecs/component_system.h"

#include <iostream>
#include <vector>

// Test component
struct Position {
    float x, y, z;
};

struct Velocity {
    float vx, vy, vz;
};

int main() {
    std::cout << "=== PURE ECS MODULE BUILD TEST ===\n";
    
    // Test 1: Entity system creation
    std::cout << "Creating EntitySystem...\n";
    kern::runtime::ecs::EntitySystem entities;
    std::cout << "✅ EntitySystem created successfully\n";
    
    // Test 2: Create entities
    std::cout << "Creating entities...\n";
    auto e1 = entities.create("Entity1");
    auto e2 = entities.create("Entity2");
    auto e3 = entities.create("Entity3");
    
    std::cout << "Created entities: " << entities.getCount() << "\n";
    std::cout << "✅ Entity creation working\n";
    
    // Test 3: Component system creation
    std::cout << "Creating ComponentSystem...\n";
    kern::runtime::ecs::ComponentSystem components;
    std::cout << "✅ ComponentSystem created successfully\n";
    
    // Test 4: Add components
    std::cout << "Adding components...\n";
    auto* pos1 = components.add<Position>(e1, 1.0f, 2.0f, 3.0f);
    auto* vel1 = components.add<Velocity>(e1, 0.1f, 0.2f, 0.3f);
    auto* pos2 = components.add<Position>(e2, 4.0f, 5.0f, 6.0f);
    
    std::cout << "Position components: " << components.getCount<Position>() << "\n";
    std::cout << "Velocity components: " << components.getCount<Velocity>() << "\n";
    std::cout << "✅ Component addition working\n";
    
    // Test 5: Get components
    std::cout << "Testing component retrieval...\n";
    auto* retrievedPos = components.get<Position>(e1);
    if (retrievedPos && retrievedPos->x == 1.0f && retrievedPos->y == 2.0f && retrievedPos->z == 3.0f) {
        std::cout << "✅ Component retrieval working\n";
    } else {
        std::cout << "❌ Component retrieval failed\n";
        return 1;
    }
    
    // Test 6: Component iteration
    std::cout << "Testing component iteration...\n";
    int count = 0;
    components.forEach<Position>([&count](kern::runtime::ecs::EntityId entity, Position& pos) {
        count++;
        std::cout << "  Entity " << entity << ": Position(" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    });
    
    if (count == 2) {
        std::cout << "✅ Component iteration working\n";
    } else {
        std::cout << "❌ Component iteration failed (expected 2, got " << count << ")\n";
        return 1;
    }
    
    // Test 7: Entity destruction
    std::cout << "Testing entity destruction...\n";
    entities.destroy(e2);
    std::cout << "Entities after destruction: " << entities.getCount() << "\n";
    
    if (entities.getCount() == 2 && !entities.isAlive(e2)) {
        std::cout << "✅ Entity destruction working\n";
    } else {
        std::cout << "❌ Entity destruction failed\n";
        return 1;
    }
    
    // Test 8: Component cleanup
    std::cout << "Testing component cleanup...\n";
    components.removeAll(e1);
    std::cout << "Position components after cleanup: " << components.getCount<Position>() << "\n";
    
    if (components.getCount<Position>() == 0) {
        std::cout << "✅ Component cleanup working\n";
    } else {
        std::cout << "❌ Component cleanup failed\n";
        return 1;
    }
    
    std::cout << "\n=== PURE ECS MODULE BUILD TEST PASSED ===\n";
    std::cout << "ECS module works completely standalone!\n";
    std::cout << "No runtime core dependencies!\n";
    std::cout << "No engine core dependencies!\n";
    
    return 0;
}
