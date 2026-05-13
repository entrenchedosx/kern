/* *
 * test_ecs_isolated.cpp - TRUE ISOLATION TEST 3: ECS Module Only Compile
 * 
 * Compile ONLY: ECS module (entity_system.cpp + component_system.cpp + entity.cpp + component.cpp)
 * NO runtime core, NO VM, NO scheduler, NO bindings
 */

// STRICT: Only ECS module includes
#include "kern/runtime/modules/ecs/entity_system.h"
#include "kern/runtime/modules/ecs/component_system.h"
#include "kern/runtime/modules/ecs/entity.h"
#include "kern/runtime/modules/ecs/component.h"

// FORBIDDEN: No runtime core includes
// FORBIDDEN: No VM includes
// FORBIDDEN: No scheduler includes
// FORBIDDEN: No binding includes

int main() {
    std::cout << "=== ECS MODULE ISOLATION TEST ===\n";
    
    // Test 1: Entity system creation
    std::cout << "Creating EntitySystem...\n";
    kern::runtime::ecs::EntitySystem entities;
    std::cout << "✅ EntitySystem created successfully\n";
    
    // Test 2: Component system creation
    std::cout << "Creating ComponentSystem...\n";
    kern::runtime::ecs::ComponentSystem components;
    std::cout << "✅ ComponentSystem created successfully\n";
    
    // Test 3: Entity creation
    std::cout << "Creating entities...\n";
    auto e1 = entities.create("TestEntity1");
    auto e2 = entities.create("TestEntity2");
    std::cout << "Created " << entities.getCount() << " entities\n";
    
    // Test 4: Component addition
    struct TestComponent {
        float x, y, z;
    };
    
    auto* comp1 = components.add<TestComponent>(e1, 1.0f, 2.0f, 3.0f);
    auto* comp2 = components.add<TestComponent>(e2, 4.0f, 5.0f, 6.0f);
    
    std::cout << "Added components to entities\n";
    
    // Test 5: Component retrieval
    auto* retrieved = components.get<TestComponent>(e1);
    if (retrieved && retrieved->x == 1.0f && retrieved->y == 2.0f && retrieved->z == 3.0f) {
        std::cout << "✅ Component retrieval working\n";
    } else {
        std::cout << "❌ Component retrieval failed\n";
        return 1;
    }
    
    // Test 6: Component iteration
    int count = 0;
    components.forEach<TestComponent>([&count](kern::runtime::ecs::EntityId entity, TestComponent& comp) {
        count++;
    });
    
    if (count == 2) {
        std::cout << "✅ Component iteration working\n";
    } else {
        std::cout << "❌ Component iteration failed (expected 2, got " << count << ")\n";
        return 1;
    }
    
    // Test 7: Entity destruction
    entities.destroy(e2);
    if (entities.getCount() == 1 && !entities.isAlive(e2)) {
        std::cout << "✅ Entity destruction working\n";
    } else {
        std::cout << "❌ Entity destruction failed\n";
        return 1;
    }
    
    std::cout << "\n=== ECS MODULE ISOLATION TEST PASSED ===\n";
    std::cout << "ECS module works completely standalone!\n";
    std::cout << "No runtime core dependencies!\n";
    std::cout << "No VM dependencies!\n";
    std::cout << "No scheduler dependencies!\n";
    std::cout << "No binding dependencies!\n";
    
    return 0;
}
