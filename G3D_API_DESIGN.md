# Kern G3D API Design - Ursina-Style ECS Abstraction

## Overview

The G3D module provides a **Ursina-style** 3D API that wraps an internal ECS system. Users write simple, intuitive code while the ECS handles the heavy lifting behind the scenes.

**Key Principle:** ECS is **INTERNAL ONLY**. Users never see entity IDs, components, or systems.

---

## User Experience

### What Users Write

```python
# Simple and intuitive - just like Ursina
cube = Entity(model="cube", color="red", position=(0,0,0))
camera = Camera(position=(0,2,-5))

def update(dt):
    cube.rotation_y += 1
    if input.pressed(input.Key.SPACE):
        cube.color = "blue"

App.run()
```

### What Actually Happens (Internal)

```
cube = Entity(...) → Creates ECS entity + TransformComponent + MeshComponent + MaterialComponent
cube.rotation_y += 1 → Updates TransformComponent.rotation.y internally
App.run() → Runs ECS systems for rendering, scripts, input
```

---

## Architecture

### Three Layers

```
┌─────────────────────────────────────────┐
│  Layer 1: USER API (Ursina-style)      │  ← What users see
│  - Entity, Camera, Light classes        │
│  - input.* functions                    │
│  - App singleton                        │
├─────────────────────────────────────────┤
│  Layer 2: WRAPPER CLASSES               │  ← Translates to ECS
│  - Entity wraps entity ID + components  │
│  - Cached component pointers            │
│  - Property accessors                   │
├─────────────────────────────────────────┤
│  Layer 3: ECS (Internal/Hidden)          │  ← Never exposed
│  - ECSWorld with component pools        │
│  - Systems for rendering, update      │
│  - Entity IDs, component structs        │
└─────────────────────────────────────────┘
```

---

## API Classes

### Entity Class

**Purpose:** Primary 3D object wrapper.

```cpp
// Multiple creation styles (all work the same internally)
Entity e1;                                    // Default cube
Entity e2("sphere");                          // Named model
Entity e3("cube", Vec3(0,1,0));               // Model + position
Entity e4(Entity::Params{                     // Full specification
    .model = "cube",
    .position = Vec3(0,0,0),
    .color = ColorRGB::red(),
    .scale = Vec3::one()
});
```

**Property Access (Ursina-style):**

```cpp
// Direct property access
cube.x = 5;                    // setX()
cube.y += 1;                   // getY() + setY()
cube.rotation_y += 10;         // Spin

// Vector operations
cube.position = Vec3(1, 2, 3);
cube.rotation = Vec3(0, 45, 0);
cube.scale = Vec3::one() * 2;
```

**Internal Implementation:**

```cpp
class Entity {
    internal::EntityId id_;  // Hidden ECS ID
    
    // Cached component pointers (no per-frame lookup)
    mutable TransformComponent* cachedTransform_;
    mutable MeshComponent* cachedMesh_;
    mutable MaterialComponent* cachedMaterial_;
    
public:
    float getX() const {
        ensureCached();              // Lazy init cache
        return cachedTransform_->position.x;
    }
    
    void setX(float x) {
        ensureCached();
        cachedTransform_->position.x = x;
    }
};
```

### Camera Class

**Purpose:** Viewpoint control with optional WASD + mouse look.

```cpp
// Simple camera
camera = Camera(position=(0, 2, -5));

// FPS-style camera
camera = Camera(Camera::Params{
    .position = (0, 2, -5),
    .enableWASD = true,
    .enableMouseLook = true,
    .fov = 60
});
```

**Features:**
- Automatic input processing (if enabled)
- FPS-style mouse look (lockMouse/unlockMouse)
- Configurable speed and sensitivity
- Main camera registration with App

### Light Class

**Purpose:** Light sources (point, directional, spot).

```cpp
// Sun (directional)
sun = Light(Light::DIRECTIONAL);
sun.position = (10, 20, 10);
sun.color = (255, 255, 230);
sun.intensity = 1.2;

// Point light
lamp = Light(Light::Params{
    .type = Light::POINT,
    .position = (0, 3, 0),
    .color = ColorRGB::orange(),
    .range = 10
});
```

### App Singleton

**Purpose:** Application lifecycle management.

```cpp
// Configuration
App.configure(Config{
    .windowWidth = 1280,
    .windowHeight = 720,
    .title = "My Game",
    .backgroundColor = ColorRGB::black()
});

// Callbacks
App.onUpdate([](float dt) {
    // Called every frame
});

App.onStart([]() {
    // Called once at startup
});

// Run
App.run();  // Starts the main loop
```

### Input System

**Purpose:** Clean input abstraction.

```cpp
// Keys
if (input.held(input.Key.W)) { moveForward(); }
if (input.pressed(input.Key.SPACE)) { jump(); }
if (input.released(input.Key.ESCAPE)) { pause(); }

// Mouse
Vec2 pos = input.getMousePosition();
Vec2 delta = input.getMouseDelta();

// Lock/unlock for FPS
input.lockMouse();     // Hide + center cursor
input.unlockMouse();   // Show cursor
```

---

## Internal ECS Design

### Why ECS?

**Benefits for 3D engine:**
- Efficient batch processing (all meshes rendered together)
- Cache-friendly data layout
- Easy to add/remove features (just add component)
- Parallel processing opportunities

### Component Layout

```cpp
// Components are simple POD structs
struct TransformComponent {
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
};

struct MeshComponent {
    Mesh mesh;           // Raylib mesh
    bool loaded;
    std::string modelPath;
};

struct MaterialComponent {
    Color color;
    bool visible;
    bool wireframe;
};

struct CameraComponent {
    float fov;
    bool isMain;
};
```

### Component Pools

```cpp
// Type-safe component storage
class ECSWorld {
    std::unordered_map<EntityId, TransformComponent> transforms;
    std::unordered_map<EntityId, MeshComponent> meshes;
    std::unordered_map<EntityId, MaterialComponent> materials;
    // ...
};
```

### Systems

```cpp
// Rendering system
void render() {
    for (each entity with Mesh + Transform + Material) {
        DrawMesh(mesh, material, transform_matrix);
    }
}

// Script update system
void update(float dt) {
    for (each entity with Script) {
        script.updateFunc(dt);
    }
}
```

---

## Wrapper Design Patterns

### 1. RAII for Entity Lifecycle

```cpp
class Entity {
public:
    Entity() {
        id_ = ECS::createEntity();  // Acquire
        // Add default components
    }
    
    ~Entity() {
        ECS::destroyEntity(id_);     // Release
    }
    
    // Disable copy (unique ownership)
    Entity(const Entity&) = delete;
    
    // Enable move
    Entity(Entity&& other) {
        id_ = other.id_;
        other.id_ = 0;  // Clear moved-from
    }
};
```

### 2. Lazy Component Caching

```cpp
void ensureCached() const {
    if (!cachedTransform_) {
        cachedTransform_ = ECS::getComponent<Transform>(id_);
    }
}
```

**Benefits:**
- No lookup cost after first access
- Components retrieved only when needed
- Cache automatically invalidated on component removal

### 3. Property Accessors

```cpp
// Property style: cube.x += 1
float getX() const { return getPosition().x; }
void setX(float x) { 
    auto pos = getPosition();
    pos.x = x;
    setPosition(pos);
}
```

**Benefits:**
- Familiar to Python/Ursina users
- Can add validation in setters
- Easy to debug (breakpoints on setters)

---

## Performance Considerations

### Zero-Cost Abstractions

**Design goal:** User API has zero overhead vs direct ECS access.

| Operation | Cost |
|-----------|------|
| `entity.x += 1` | 1 pointer dereference (cached component) |
| `ECS::getComponent(id)` | Hash map lookup (only on first access) |
| `entity.setPosition()` | Function call + 3 float copies |

### Memory Layout

```cpp
// Cache-friendly: components stored contiguously
std::vector<TransformComponent> allTransforms;  // Sequential in memory
// NOT: std::unordered_map<EntityId, TransformComponent>  // Scattered
```

### No Per-Frame Allocations

```cpp
void App::run() {
    while (running) {
        // Pre-allocated, no mallocs in hot path
        updateEntities();
        renderEntities();
    }
}
```

---

## Demo: Spinning Cube

```python
use g3d

# Create entities
cube = Entity(model="cube", color="red", scale=2)
sun = Light(type=DIRECTIONAL, position=(10, 20, 10))
camera = Camera(position=(0, 3, -8))

# Update function
def update(dt):
    cube.rotation_y += 50 * dt  # 50 degrees per second

App.onUpdate(update)
App.run()
```

**What happens internally:**

1. `Entity(...)` → Creates ECS entity + 3 components
2. `cube.rotation_y += 50 * dt` → Updates cached TransformComponent
3. `App.run()` →
   - Updates scripts (calls `update(dt)`)
   - Renders: transforms all entities, draws meshes
   - Repeats at 60 FPS

---

## Files

| File | Purpose |
|------|---------|
| `g3d_api.hpp` | User-facing API (Entity, Camera, Light, App, input) |
| `g3d_api.cpp` | Implementation of API classes |
| `ecs_bridge.cpp` | Internal ECS (component pools, systems) |
| `demo_ursina_style.kn` | Working example with spinning cube |

---

## Future Enhancements

### Phase 1 (Current)
- ✅ Basic Entity, Camera, Light
- ✅ WASD + mouse look
- ✅ Simple shapes (cube, sphere, plane)

### Phase 2
- [ ] Physics integration
- [ ] Particle systems
- [ ] Terrain
- [ ] Model loading (GLTF/FBX)

### Phase 3
- [ ] Scene serialization
- [ ] Editor integration
- [ ] Networking

---

## Conclusion

**Key Achievements:**
1. ✅ Ursina-style API (simple, intuitive)
2. ✅ ECS-powered (efficient, scalable)
3. ✅ Zero exposure of ECS internals
4. ✅ Performance optimized (caching, no allocations)
5. ✅ Working demo (spinning cube + camera)

**Result:** Users can write simple 3D code while benefiting from professional-grade ECS architecture behind the scenes.
