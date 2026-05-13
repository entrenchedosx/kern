# Kern Engine Phase 1 Implementation Blueprint
## Foundation Fix: Scene Graph + ECS + Serialization

**Status:** Critical Path - Must Complete First  
**Estimated Duration:** 4-6 weeks (full-time)  
**Risk Level:** HIGH (touches all core systems)  

---

## 📁 1. New Folder Structure

```
kern/engine/                    # NEW: Core engine layer
├── core/                       # Engine fundamentals
│   ├── entity.h / .cpp         # Entity ID management
│   ├── component.h             # Component base class
│   ├── world.h / .cpp          # World/Scene container
│   └── type_id.h               # Component type registration
├── scene_graph/                # Hierarchy system
│   ├── transform.h / .cpp      # Local/world matrices
│   ├── hierarchy.h / .cpp      # Parent-child relationships
│   └── traversal.h             # Scene graph iteration
├── serialization/              # Save/Load system
│   ├── scene_format.h          # .scene file format
│   ├── entity_serializer.h     # Entity → JSON/Binary
│   └── component_serializer.h  # Component serialization
└── reflection/                 # Runtime type info
    ├── type_registry.h         # Component registration
    └── property_reflection.h   # Inspector support

kern/modules/g3d_v2/            # NEW: Rebuilt 3D renderer
├── renderer/                   # Render pipeline
│   ├── render_pass.h           # Pass abstraction
│   ├── material.h / .cpp       # Material system
│   └── shader.h / .cpp         # Shader pipeline
├── scene/                      # Scene integration
│   ├── mesh_renderer.h         # MeshRenderer component
│   ├── camera.h                # Camera component
│   └── light.h                 # Light component
└── gpu/                        # GPU abstraction
    ├── buffer.h                # GPU buffers
    └── texture.h               # Texture management

lib/kern/engine/                # NEW: Engine scripting API
├── entity.kn                   # Entity scripting wrapper
├── transform.kn                # Transform component API
├── component.kn                # Component base class
└── world.kn                    # Scene management API

editor/                         # NEW: Editor application (separate)
├── editor_core/                # Editor fundamentals
│   ├── editor_world.h          # Editor scene wrapper
│   └── selection.h             # Entity selection system
├── gui/                        # Editor UI
│   ├── hierarchy_panel.h       # Scene tree view
│   ├── inspector_panel.h       # Property editor
│   └── viewport.h              # 3D scene view
└── tools/                      # Editor tools
    ├── gizmo.h                 # Transform gizmos
    └── scene_serializer.h      # Editor save/load
```

---

## 🔧 2. CMake Restructuring

### 2.1 New CMakeLists.txt Additions

```cmake
# ═══════════════════════════════════════════════════════════════════════════════
# KERN ENGINE MODULES (Phase 1)
# ═══════════════════════════════════════════════════════════════════════════════

# Engine Core Library
set(KERN_ENGINE_CORE_SOURCES
    kern/engine/core/entity.cpp
    kern/engine/core/component.cpp
    kern/engine/core/world.cpp
    kern/engine/scene_graph/transform.cpp
    kern/engine/scene_graph/hierarchy.cpp
    kern/engine/serialization/scene_format.cpp
    kern/engine/serialization/entity_serializer.cpp
    kern/engine/reflection/type_registry.cpp
)

add_library(kern_engine_core STATIC ${KERN_ENGINE_CORE_SOURCES})
target_include_directories(kern_engine_core PUBLIC 
    kern/engine
    kern/engine/core
    kern/engine/scene_graph
    kern/engine/serialization
)
target_link_libraries(kern_engine_core PUBLIC kern_core)

# g3d_v2 Module (Phase 1: Materials + Components only)
set(KERN_G3D_V2_SOURCES
    kern/modules/g3d_v2/renderer/material.cpp
    kern/modules/g3d_v2/renderer/shader.cpp
    kern/modules/g3d_v2/scene/mesh_renderer.cpp
    kern/modules/g3d_v2/scene/camera.cpp
    kern/modules/g3d_v2/gpu/buffer.cpp
    kern/modules/g3d_v2/gpu/texture.cpp
)

add_library(kern_g3d_v2 STATIC ${KERN_G3D_V2_SOURCES})
target_include_directories(kern_g3d_v2 PUBLIC kern/modules/g3d_v2)
target_link_libraries(kern_g3d_v2 PUBLIC kern_engine_core kern_raylib)

# Editor Application (optional build target)
if(KERN_BUILD_EDITOR)
    set(KERN_EDITOR_SOURCES
        editor/editor_core/editor_world.cpp
        editor/gui/hierarchy_panel.cpp
        editor/gui/inspector_panel.cpp
        editor/gui/viewport.cpp
        editor/tools/gizmo.cpp
    )
    
    add_executable(kern_editor ${KERN_EDITOR_SOURCES})
    target_link_libraries(kern_editor 
        kern_engine_core 
        kern_g3d_v2 
        imgui  # Add Dear ImGui as dependency
    )
endif()
```

---

## 🏗️ 3. Core Implementation Skeleton

### 3.1 Entity System (kern/engine/core/entity.h)

```cpp
#pragma once
#include <cstdint>
#include <functional>

namespace kern::engine {

// Lightweight entity identifier (16-bit generation + 48-bit index)
using EntityId = uint64_t;
constexpr EntityId INVALID_ENTITY = 0;

class EntityRegistry {
public:
    // Create/destroy entities
    EntityId create();
    void destroy(EntityId id);
    bool isAlive(EntityId id) const;
    
    // Iteration
    template<typename Func>
    void forEach(Func&& func) const;
    
    // Validation
    static uint32_t getIndex(EntityId id);
    static uint16_t getGeneration(EntityId id);
    
private:
    std::vector<uint32_t> freeIndices_;
    std::vector<uint16_t> generations_;
    std::vector<bool> alive_;
    uint32_t nextIndex_ = 1; // 0 reserved for INVALID_ENTITY
};

} // namespace kern::engine
```

### 3.2 Component Base (kern/engine/core/component.h)

```cpp
#pragma once
#include "entity.h"
#include <typeindex>

namespace kern::engine {

class Component {
public:
    virtual ~Component() = default;
    
    // Each component type must implement
    virtual std::type_index getType() const = 0;
    virtual void onAttach(EntityId entity) {}
    virtual void onDetach(EntityId entity) {}
    
    EntityId getOwner() const { return owner_; }
    
protected:
    EntityId owner_ = INVALID_ENTITY;
    friend class World;
};

// Component storage (sparse set for O(1) lookup)
template<typename T>
class ComponentStorage {
public:
    T* get(EntityId entity);
    const T* get(EntityId entity) const;
    T* add(EntityId entity);
    void remove(EntityId entity);
    bool has(EntityId entity) const;
    
    // Iteration
    template<typename Func>
    void forEach(Func&& func);
    
private:
    std::vector<T> components_;
    std::vector<EntityId> entityToComponent_;
    std::vector<EntityId> componentToEntity_;
};

} // namespace kern::engine
```

### 3.3 Transform Component (kern/engine/scene_graph/transform.h)

```cpp
#pragma once
#include "core/component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace kern::engine {

struct Transform : public Component {
    // Local space
    glm::vec3 localPosition = glm::vec3(0.0f);
    glm::quat localRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 localScale = glm::vec3(1.0f);
    
    // World space (cached, updated by hierarchy system)
    glm::mat4 worldMatrix = glm::mat4(1.0f);
    bool worldDirty = true;
    
    // Hierarchy links
    EntityId parent = INVALID_ENTITY;
    std::vector<EntityId> children;
    
    // Accessors
    glm::vec3 getWorldPosition() const;
    glm::quat getWorldRotation() const;
    glm::vec3 getWorldScale() const;
    
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    
    void setWorldPosition(const glm::vec3& pos);
    void setWorldRotation(const glm::quat& rot);
    
    // Component interface
    std::type_index getType() const override { return typeid(Transform); }
};

} // namespace kern::engine
```

### 3.4 World/Scene Container (kern/engine/core/world.h)

```cpp
#pragma once
#include "entity.h"
#include "component.h"
#include "scene_graph/transform.h"
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace kern::engine {

class World {
public:
    World(const std::string& name = "Untitled");
    ~World();
    
    // Entity management
    EntityId createEntity(const std::string& name = "Entity");
    void destroyEntity(EntityId entity);
    bool isAlive(EntityId entity) const;
    std::string getEntityName(EntityId entity) const;
    void setEntityName(EntityId entity, const std::string& name);
    
    // Component management (template-based)
    template<typename T, typename... Args>
    T* addComponent(EntityId entity, Args&&... args);
    
    template<typename T>
    void removeComponent(EntityId entity);
    
    template<typename T>
    T* getComponent(EntityId entity);
    
    template<typename T>
    bool hasComponent(EntityId entity) const;
    
    // Component queries
    template<typename... Components>
    std::vector<EntityId> query();
    
    // Hierarchy
    void setParent(EntityId child, EntityId parent);
    void unparent(EntityId child);
    EntityId getParent(EntityId entity) const;
    std::vector<EntityId> getChildren(EntityId entity) const;
    
    // Scene graph update (call once per frame)
    void updateTransforms();
    
    // Lifecycle
    void start();  // Called when scene loads
    void update(float deltaTime);  // Per-frame update
    void shutdown();  // Cleanup
    
    // Access
    const std::string& getName() const { return name_; }
    EntityRegistry& getEntityRegistry() { return entities_; }
    
private:
    std::string name_;
    EntityRegistry entities_;
    
    // Component storages (type-erased)
    std::unordered_map<std::type_index, std::unique_ptr<void, void(*)(void*)>> storages_;
    
    // Entity metadata
    struct EntityData {
        std::string name;
        EntityId parent = INVALID_ENTITY;
        std::vector<EntityId> children;
    };
    std::unordered_map<EntityId, EntityData> entityData_;
    
    // Template helpers
    template<typename T>
    ComponentStorage<T>* getStorage();
};

// Template implementations
#include "world.inl"

} // namespace kern::engine
```

---

## 📄 4. Serialization Format Specification

### 4.1 Scene File Format (.kscene - JSON-based)

```json
{
  "format_version": "1.0.0",
  "scene_name": "Test Scene",
  "scene_guid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  
  "entities": [
    {
      "guid": "ent-001-uuid",
      "name": "Player",
      "parent": null,
      "components": {
        "Transform": {
          "local_position": [0, 0, 0],
          "local_rotation": [0, 0, 0, 1],
          "local_scale": [1, 1, 1]
        },
        "MeshRenderer": {
          "mesh": "assets/models/player.obj",
          "material": "assets/materials/player.mat"
        },
        "Rigidbody": {
          "mass": 70,
          "use_gravity": true
        }
      }
    },
    {
      "guid": "ent-002-uuid",
      "name": "Main Camera",
      "parent": null,
      "components": {
        "Transform": {
          "local_position": [0, 5, -10],
          "local_rotation": [0.259, 0, 0, 0.966],
          "local_scale": [1, 1, 1]
        },
        "Camera": {
          "fov": 60,
          "near_plane": 0.1,
          "far_plane": 1000,
          "clear_color": [0.2, 0.2, 0.2]
        }
      }
    },
    {
      "guid": "ent-003-uuid",
      "name": "Player Hand",
      "parent": "ent-001-uuid",
      "components": {
        "Transform": {
          "local_position": [0.5, 1.2, 0.3],
          "local_rotation": [0, 0, 0, 1],
          "local_scale": [0.8, 0.8, 0.8]
        }
      }
    }
  ],
  
  "prefab_instances": [
    {
      "prefab_guid": "prefab-tree-uuid",
      "instance_guid": "inst-tree-001",
      "parent": null,
      "position": [10, 0, 5],
      "rotation": [0, 0.707, 0, 0.707],
      "scale": [1, 1, 1],
      "overrides": {
        "Transform.local_scale": [1.5, 1.5, 1.5]
      }
    }
  ],
  
  "metadata": {
    "created_by": "Kern Editor 2.0",
    "created_at": "2026-05-10T10:30:00Z",
    "modified_at": "2026-05-10T10:30:00Z"
  }
}
```

### 4.2 Binary Format (.ksceneb - Runtime optimized)

```cpp
struct SceneBinaryHeader {
    char magic[4] = {'K', 'S', 'C', 'N'};  // "KSCN"
    uint32_t version = 1;
    uint32_t entityCount;
    uint32_t componentTypeCount;
    uint64_t stringTableOffset;
    uint64_t dataOffset;
};

struct EntityBinary {
    uint64_t guidHash;           // FNV-1a of GUID string
    uint32_t nameStringIndex;    // Index into string table
    uint32_t parentIndex;        // 0xFFFFFFFF = no parent
    uint32_t componentMask;      // Bitmask of components
    uint32_t componentDataOffset;// Offset to component data blob
};
```

---

## 🔄 5. Implementation Order (Week-by-Week)

### Week 1: Entity + Component Core

**Files to create:**
1. `kern/engine/core/entity.h/.cpp` - Registry implementation
2. `kern/engine/core/component.h` - Base class + storage template
3. `kern/engine/core/world.h/.cpp` - Basic entity creation/destruction
4. Tests: `tests/engine/test_entity.cpp`

**Deliverable:** Can create entities, add/remove components

### Week 2: Transform + Hierarchy

**Files to create:**
1. `kern/engine/scene_graph/transform.h/.cpp` - Transform component
2. `kern/engine/scene_graph/hierarchy.h/.cpp` - Parent-child logic
3. `kern/engine/core/world.cpp` - Update transforms with propagation
4. Tests: Hierarchy stress test (1000 nested entities)

**Deliverable:** Moving parent moves children correctly

### Week 3: Serialization Foundation

**Files to create:**
1. `kern/engine/serialization/scene_format.h/.cpp` - JSON read/write
2. `kern/engine/reflection/type_registry.h/.cpp` - Component registration
3. `kern/engine/reflection/property_reflection.h` - Property introspection
4. Tools: `tools/scene_converter.cpp` - Convert old format

**Deliverable:** Can save/load scene to JSON

### Week 4: Material + Shader System (g3d_v2)

**Files to create:**
1. `kern/modules/g3d_v2/renderer/shader.h/.cpp` - Shader loading
2. `kern/modules/g3d_v2/renderer/material.h/.cpp` - Material parameters
3. `kern/modules/g3d_v2/scene/mesh_renderer.h/.cpp` - Component
4. Assets: Basic shader files (unlit.vert/unlit.frag)

**Deliverable:** Render cube with custom shader

### Week 5: Integration + Testing

**Tasks:**
1. Integrate g3d_v2 with engine core
2. Convert 3 example scenes to new format
3. Performance test (1000 entities, 60fps)
4. Memory leak testing

**Deliverable:** Working demo with new architecture

### Week 6: Editor Foundation (Optional but Recommended)

**Files to create:**
1. `editor/editor_core/editor_world.h/.cpp` - Editor scene wrapper
2. `editor/gui/hierarchy_panel.h/.cpp` - Entity tree
3. `editor/gui/inspector_panel.h/.cpp` - Property editing
4. `editor/tools/gizmo.h/.cpp` - Transform manipulation

**Deliverable:** Can edit scene visually

---

## ⚠️ 6. Migration Strategy (Critical)

### Old g3d → New g3d_v2 Migration Path

```cpp
// Phase 1: Dual Runtime (Backwards Compatible)
// Keep old g3d working while building g3d_v2

// Old code continues to work:
let g3 = import("g3d")  // Old API (Raylib-based)
g3.createWindow(...)

// New code uses engine:
let world = import("engine.world")
let Entity = import("engine.entity")
let MeshRenderer = import("engine.mesh_renderer")

// Phase 2: Deprecation (Month 2-3)
// Mark g3d as deprecated, migrate examples

// Phase 3: Removal (Month 6+)
// Remove old g3d, g3d_v2 becomes g3d
```

### VM Compatibility

```cpp
// Script API wrapper (maintains compatibility)
// lib/kern/engine/entity.kn

module Entity

def create(name = "Entity") {
    // Calls into C++ engine
    return __engine_entity_create(name)
}

def destroy(entity) {
    __engine_entity_destroy(entity.__id)
}

def add_component(entity, component_type) {
    return component_type.__attach(entity.__id)
}
```

---

## 🧪 7. Testing Strategy

### Unit Tests Required

```cpp
// tests/engine/test_entity.cpp
TEST(EntityRegistry, CreateDestroy) {
    EntityRegistry registry;
    EntityId e1 = registry.create();
    EntityId e2 = registry.create();
    
    EXPECT_NE(e1, e2);
    EXPECT_TRUE(registry.isAlive(e1));
    
    registry.destroy(e1);
    EXPECT_FALSE(registry.isAlive(e1));
    
    // Test ID reuse
    EntityId e3 = registry.create();
    EXPECT_EQ(EntityRegistry::getIndex(e3), EntityRegistry::getIndex(e1));
    EXPECT_GT(EntityRegistry::getGeneration(e3), EntityRegistry::getGeneration(e1));
}

// tests/engine/test_transform_hierarchy.cpp
TEST(TransformHierarchy, ParentChildPropagation) {
    World world;
    EntityId parent = world.createEntity("Parent");
    EntityId child = world.createEntity("Child");
    
    Transform* pTrans = world.addComponent<Transform>(parent);
    Transform* cTrans = world.addComponent<Transform>(child);
    
    pTrans->localPosition = glm::vec3(10, 0, 0);
    world.setParent(child, parent);
    world.updateTransforms();
    
    // Child should be at (10,0,0) in world space
    EXPECT_EQ(cTrans->getWorldPosition(), glm::vec3(10, 0, 0));
    
    // Move parent
    pTrans->localPosition = glm::vec3(20, 0, 0);
    world.updateTransforms();
    
    // Child should follow
    EXPECT_EQ(cTrans->getWorldPosition(), glm::vec3(20, 0, 0));
}
```

### Stress Tests

```cpp
// 1000 entities, 10 components each
// 100 frame average must be > 60fps
TEST(Performance, ThousandEntities) {
    World world;
    
    for (int i = 0; i < 1000; i++) {
        EntityId e = world.createEntity();
        world.addComponent<Transform>(e);
        world.addComponent<MeshRenderer>(e);
        // ... add more components
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int frame = 0; frame < 100; frame++) {
        world.update(0.016f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    float avgMs = std::chrono::duration<float>(end - start).count() * 10;
    EXPECT_LT(avgMs, 16.0f);  // Must be < 16ms for 60fps
}
```

---

## 📋 8. Checklist for Phase 1 Complete

- [ ] Entity ID system (create/destroy/reuse)
- [ ] Component storage (sparse sets)
- [ ] Transform component (local + world)
- [ ] Parent-child hierarchy (propagation)
- [ ] World container (scene management)
- [ ] Scene serialization (JSON format)
- [ ] Material system (shader + parameters)
- [ ] MeshRenderer component
- [ ] Camera component
- [ ] Entity queries (by component type)
- [ ] Editor integration (basic)
- [ ] 1000+ entity performance test
- [ ] Memory leak test (valgrind/ASan)
- [ ] 3 example scenes converted

**When all checked → Phase 1 is DONE and Phase 2 can begin.**

---

## 🔥 Next Steps

Once you approve this blueprint, I can generate:

1. **File-by-file implementation** - Exact code for each header/cpp
2. **CMake integration guide** - Step-by-step build system changes
3. **Migration tool** - Convert existing Kern projects to new format
4. **Editor UI mockups** - ImGui layout for hierarchy/inspector panels

**Which would you like first?**

