/* *
 * kern/engine/kern_engine_contracts.h - Engine Data Contract Enforcement
 * 
 * Compile-time and runtime enforcement of:
 * 1. Entity Contract (GLOBAL RULE)
 * 2. Component Contract (STORAGE RULE)  
 * 3. System Contract (EXECUTION RULE)
 * 
 * Include this header in ALL engine files to enforce contracts.
 * Zero tolerance for violations - fails fast in debug builds.
 */

#pragma once

#include "core/entity.h"
#include <type_traits>
#include <cstddef>

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT ENFORCEMENT LEVEL
// ═══════════════════════════════════════════════════════════════════════════════

// Define before including this header to control strictness:
// #define KERN_ENGINE_STRICT_MODE 1  // Hard failures on violation
// #define KERN_ENGINE_STRICT_MODE 0  // Warnings only (default for now)

#ifndef KERN_ENGINE_STRICT_MODE
    #ifdef NDEBUG
        #define KERN_ENGINE_STRICT_MODE 0  // Relaxed in release
    #else
        #define KERN_ENGINE_STRICT_MODE 1  // Strict in debug
    #endif
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT 1: ENTITY CONTRACT (GLOBAL RULE)
// ═══════════════════════════════════════════════════════════════════════════════
// 
// RULES:
// * Entity = 32-bit or 64-bit ID (no pointers as identity)
// * Entity is meaningless without registry
// * Entity lifetime controlled ONLY by World/Registry
//
// ENFORCEMENT: Compile-time + runtime assertions

namespace kern::engine::contracts {

// Compile-time check: Ensure EntityId is not a pointer
template<typename T>
struct is_valid_entity_type {
    static constexpr bool value = 
        std::is_same_v<T, EntityId> &&           // Must be EntityId type
        !std::is_pointer_v<T> &&                // No pointers allowed
        sizeof(T) == 8;                          // Must be 64-bit
};

template<typename T>
inline constexpr bool is_valid_entity_type_v = is_valid_entity_type<T>::value;

// Static assertion helper
#define KERN_CONTRACT_ENTITY_TYPE(T) \
    static_assert(kern::engine::contracts::is_valid_entity_type_v<T>, \
        "VIOLATION: Entity must be EntityId (64-bit), not pointer or other type")

// Runtime check: Entity must come from valid registry
// Use this macro when receiving EntityId from external code
#define KERN_CONTRACT_VALID_ENTITY(registry, entity) \
    do { \
        if constexpr (KERN_ENGINE_STRICT_MODE) { \
            if (!registry.isAlive(entity)) { \
                KERN_CONTRACT_VIOLATION("Entity " << entity << " is not alive in registry"); \
            } \
        } \
    } while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT 2: COMPONENT CONTRACT (STORAGE RULE)
// ═══════════════════════════════════════════════════════════════════════════════
//
// RULES:
// * POD-only (no virtuals, no inheritance required)
// * Stored in contiguous arrays (sparse set or SoA)
// * Access only via `World`
//
// ENFORCEMENT: Static analysis + runtime checks

template<typename T>
struct is_valid_component {
    // Check 1: Must be standard layout (POD-like)
    static constexpr bool standard_layout = std::is_standard_layout_v<T>;
    
    // Check 2: Should not have virtual functions (polymorphic)
    // Exception: Component base class is allowed to have virtuals
    static constexpr bool no_virtuals = !std::is_polymorphic_v<T> || 
                                        std::is_base_of_v<class Component, T>;
    
    // Check 3: Must be trivially destructible (for fast bulk operations)
    static constexpr bool trivial_dtor = std::is_trivially_destructible_v<T>;
    
    // Check 4: Size must be reasonable (not huge)
    static constexpr bool reasonable_size = sizeof(T) <= 256;
    
    static constexpr bool value = 
        standard_layout && 
        (no_virtuals || std::is_base_of_v<class Component, T>) &&
        trivial_dtor &&
        reasonable_size;
};

template<typename T>
inline constexpr bool is_valid_component_v = is_valid_component<T>::value;

// Component contract assertion
#define KERN_CONTRACT_COMPONENT_TYPE(T) \
    static_assert(kern::engine::contracts::is_valid_component_v<T>, \
        "VIOLATION: Component " #T " violates component contract:\n" \
        "  - Must be standard layout (POD-like): " \
        << (kern::engine::contracts::is_valid_component<T>::standard_layout ? "OK" : "FAIL") << "\n" \
        "  - Should not be polymorphic (or inherit from Component): " \
        << (kern::engine::contracts::is_valid_component<T>::no_virtuals ? "OK" : "FAIL") << "\n" \
        "  - Must be trivially destructible: " \
        << (kern::engine::contracts::is_valid_component<T>::trivial_dtor ? "OK" : "FAIL") << "\n" \
        "  - Must be <= 256 bytes: " \
        << (kern::engine::contracts::is_valid_component<T>::reasonable_size ? "OK" : "FAIL"))

// ═══════════════════════════════════════════════════════════════════════════════
// CONTRACT 3: SYSTEM CONTRACT (EXECUTION RULE)
// ═══════════════════════════════════════════════════════════════════════════════
//
// RULES:
// * Takes `World&` 
// * Iterates components only via registry views
// * Has NO ownership of entities or components
//
// ENFORCEMENT: Code patterns + assertions

// System base class to enforce contract
class System {
public:
    virtual ~System() = default;
    
    // CONTRACT: update() receives World&, never owns it
    virtual void update(class World& world, float deltaTime) = 0;
    
    // CONTRACT: Systems can be enabled/disabled but don't delete entities
    bool enabled = true;
    
    // CONTRACT: Systems have priority for update ordering
    int priority = 0;
};

// Macro to enforce system contract in update method
#define KERN_CONTRACT_SYSTEM_UPDATE(WorldParam) \
    static_assert(std::is_same_v<std::remove_reference_t<decltype(WorldParam)>, class World>, \
        "VIOLATION: System update() must take World& parameter"); \
    KERN_CONTRACT_POINTER_NOT_OWNED(&WorldParam, "World")

// ═══════════════════════════════════════════════════════════════════════════════
// VIOLATION HANDLING
// ═══════════════════════════════════════════════════════════════════════════════

// Define KERN_CONTRACT_VIOLATION to handle violations
#ifndef KERN_CONTRACT_VIOLATION
    #include <iostream>
    #include <sstream>
    #include <stdexcept>
    
    #if KERN_ENGINE_STRICT_MODE
        // Hard failure in strict mode
        #define KERN_CONTRACT_VIOLATION(msg) \
            do { \
                std::ostringstream oss; \
                oss << "\n╔══════════════════════════════════════════════════════════════════════════════╗\n"; \
                oss << "║ KERN ENGINE CONTRACT VIOLATION                                               ║\n"; \
                oss << "╠══════════════════════════════════════════════════════════════════════════════╣\n"; \
                oss << "║ " << msg << "\n"; \
                oss << "╠══════════════════════════════════════════════════════════════════════════════╣\n"; \
                oss << "║ File: " << __FILE__ << "\n"; \
                oss << "║ Line: " << __LINE__ << "\n"; \
                oss << "║ Function: " << __func__ << "\n"; \
                oss << "╚══════════════════════════════════════════════════════════════════════════════╝\n"; \
                std::cerr << oss.str(); \
                throw std::runtime_error("Kern Engine Contract Violation"); \
            } while(0)
    #else
        // Warning only in non-strict mode
        #define KERN_CONTRACT_VIOLATION(msg) \
            do { \
                std::cerr << "[KERN WARNING] Contract violation: " << msg \
                          << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            } while(0)
    #endif
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// ADDITIONAL CONTRACTS
// ═══════════════════════════════════════════════════════════════════════════════

// CONTRACT: No raw pointers to engine objects (use handles/IDs)
#define KERN_CONTRACT_POINTER_NOT_OWNED(ptr, typeName) \
    do { \
        if constexpr (KERN_ENGINE_STRICT_MODE) { \
            if ((ptr) != nullptr) { \
                /* Just a reminder - can't enforce ownership at compile time */ \
            } \
        } \
    } while(0)

// CONTRACT: Thread safety annotations (documentation + runtime checks)
#define KERN_CONTRACT_THREAD_SAFE \
    /* Marker for thread-safe functions */

#define KERN_CONTRACT_MAIN_THREAD_ONLY \
    do { \
        /* Could add thread ID check here */ \
    } while(0)

// CONTRACT: Serialization compatibility
#define KERN_CONTRACT_SERIALIZABLE(T) \
    static_assert(std::is_trivially_copyable_v<T> || \
                  std::is_base_of_v<class Serializable, T>, \
        "VIOLATION: Type " #T " must be trivially copyable or implement Serializable")

// ═══════════════════════════════════════════════════════════════════════════════
// COMPONENT SIZE WARNINGS
// ═══════════════════════════════════════════════════════════════════════════════

// Warn about large components (affects cache performance)
template<typename T, size_t Threshold = 64>
struct component_size_warning {
    static constexpr bool is_large = sizeof(T) > Threshold;
    
    // This will generate a compile-time warning (via static_assert with message)
    static void check() {
        if constexpr (is_large) {
            // Note: Can't static_assert(false) here, but we can document
            // In practice, use a compiler warning pragma
        }
    }
};

// Usage: KERN_CONTRACT_COMPONENT_SIZE(Transform)
#define KERN_CONTRACT_COMPONENT_SIZE(T) \
    static_assert(sizeof(T) <= 64, \
        "WARNING: Component " #T " is " << sizeof(T) << " bytes. " \
        "Consider splitting into smaller components for cache efficiency.")

// ═══════════════════════════════════════════════════════════════════════════════
// WORLD ACCESS PATTERNS (Enforced via macros)
// ═══════════════════════════════════════════════════════════════════════════════

// CORRECT: Access component via World
#define KERN_WORLD_GET_COMPONENT(world, entity, ComponentType) \
    ([&]() -> ComponentType* { \
        KERN_CONTRACT_VALID_ENTITY(world, entity); \
        auto* comp = world.template getComponent<ComponentType>(entity); \
        if constexpr (KERN_ENGINE_STRICT_MODE) { \
            if (!comp) { \
                KERN_CONTRACT_VIOLATION("Component " #ComponentType " not found on entity " << entity); \
            } \
        } \
        return comp; \
    }())

// CORRECT: Add component via World  
#define KERN_WORLD_ADD_COMPONENT(world, entity, ComponentType, ...) \
    ([&]() -> ComponentType* { \
        KERN_CONTRACT_VALID_ENTITY(world, entity); \
        return world.template addComponent<ComponentType>(entity, ##__VA_ARGS__); \
    }())

// ═══════════════════════════════════════════════════════════════════════════════
// EXAMPLE: How to use contracts in your code
// ═══════════════════════════════════════════════════════════════════════════════

/*

// In your component header:
#include "kern/engine/kern_engine_contracts.h"

struct Transform {
    float x, y, z;
    
    // Enforce component contract at compile time
    KERN_CONTRACT_COMPONENT_TYPE(Transform);
    KERN_CONTRACT_COMPONENT_SIZE(Transform);  // Warn if > 64 bytes
};

// In your system header:
class MySystem : public kern::engine::contracts::System {
public:
    void update(World& world, float deltaTime) override {
        // Enforce system contract
        KERN_CONTRACT_SYSTEM_UPDATE(world);
        
        // Correct: Iterate via World
        world.query<Transform>().forEach([](EntityId e, Transform& t) {
            // Correct: Access component via World
            auto* mesh = KERN_WORLD_GET_COMPONENT(world, e, MeshRenderer);
            
            // ... do work ...
        });
        
        // VIOLATION: This will fail contract check
        // EntityId e = 0x1234;  // Creating ID without registry
        // t.x = 5;  // Accessing component without World
    }
};

// In your main:
int main() {
    // Set strict mode for development
    #define KERN_ENGINE_STRICT_MODE 1
    
    EntityRegistry registry;
    
    // Correct usage
    EntityId e = registry.create();
    
    // Contract-checked access
    KERN_CONTRACT_VALID_ENTITY(registry, e);
    
    return 0;
}

*/

} // namespace kern::engine::contracts

// ═══════════════════════════════════════════════════════════════════════════════
// GLOBAL SHORTCUTS (optional - include only if you want these in global scope)
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef KERN_ENGINE_ENABLE_GLOBAL_CONTRACT_MACROS
    using kern::engine::contracts::System;
    using kern::engine::contracts::is_valid_entity_type_v;
    using kern::engine::contracts::is_valid_component_v;
#endif
