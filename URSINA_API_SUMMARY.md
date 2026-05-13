# 3dengine Ursina-Style API

## Overview

Ursina-style high-level 3D engine API built on top of Kern's deterministic ECS core.

**Goal**: Minimal boilerplate, immediate-mode 3D development like Python's Ursina engine.

---

## Quick Start

```kern
// Minimal example - 3D scene in 5 lines
let e = import("3dengine")
let u = e["ursina"]

let app = u["App"]()
let cube = u["Entity"](app, {"model": "cube", "color": [1,0,0]})
// app.run()  // Starts the engine loop
```

---

## API Reference

### App

**`App()`** → app_id

Creates a new application with automatic:
- World creation
- Camera setup (position: [0, 5, 10])
- Default lighting (ambient + directional)
- Render pipeline registration
- Auto-scheduler enabled

```kern
let app = u["App"]()
```

**`app.run()`** → nil (blocks, runs main loop)

Starts the engine main loop:
1. SIMULATION stage (update callbacks)
2. RENDER stage (draw everything)
3. INPUT stage (process keys/mouse)

```kern
// In full implementation:
// app.run()  // or u["run"](app)
```

---

### Entity

**`Entity(app_id, params)`** → entity_id

Creates a 3D entity with automatic ECS component registration.

```kern
let entity = u["Entity"](app, {
    "model": "cube",              // "cube", "sphere", "plane", etc.
    "color": [r, g, b],           // or [r, g, b, a]
    "position": [x, y, z],        // or {"x": x, "y": y, "z": z}
    "rotation": [pitch, yaw, roll],
    "scale": [sx, sy, sz],
    "texture": "path/to/tex.png",
    "visible": true
})
```

**Transform Functions:**

```kern
// Set position
u["entity_set_position"](entity, [x, y, z])

// Get position
let pos = u["entity_get_position"](entity)  // returns [x, y, z]

// Set rotation
u["entity_set_rotation"](entity, [pitch, yaw, roll])

// Get rotation
let rot = u["entity_get_rotation"](entity)  // returns [pitch, yaw, roll]

// Look at target
u["entity_look_at"](entity, [target_x, target_y, target_z])
```

**Destruction:**

```kern
u["destroy"](entity)  // Removes entity from world
```

---

### Input

**`input.is_key_down(key)`** → bool

```kern
if (u["input"]["is_key_down"]("w")) {
    // Move forward
}

if (u["input"]["is_key_down"]("space")) {
    // Jump
}
```

**`input.is_key_pressed(key)`** → bool

True only on the frame key was first pressed:

```kern
if (u["input"]["is_key_pressed"]("escape")) {
    // Toggle pause menu
}
```

**`input.get_mouse_position()`** → [x, y]

```kern
let mouse = u["input"]["get_mouse_position"]()
let mx = mouse[0]
let my = mouse[1]
```

---

## Auto-Update System

Define an `update(dt)` function and the engine automatically registers it:

```kern
let cube = u["Entity"](app, {"model": "cube"})

def update(dt) {
    // dt = delta time in seconds
    // Rotate 50 degrees per second around Y axis
    let rot = u["entity_get_rotation"](cube)
    u["entity_set_rotation"](cube, [rot[0], rot[1] + 50.0 * dt, rot[2]])
}

// Engine automatically calls update() every frame
```

---

## Architecture

```
User Script (Ursina API)
         |
         v
+--------+--------+
|  Ursina Layer   |  <- App, Entity, Input (ursina_api.cpp)
|  (3dengine)     |
+--------+--------+
         |
         v
+--------+--------+
|   3dengine Core |  <- world_create, world_run (3dengine.cpp)
|   (ECS Wrapper) |
+--------+--------+
         |
         v
+--------+--------+
|   g3d Backend   |  <- Low-level rendering (g3d.cpp)
|   (Raylib)      |
+--------+--------+
```

**Key Design:**
- Ursina layer hides ECS complexity
- Core ECS remains deterministic and parallel-capable
- g3d handles all actual rendering
- No code duplication between layers

---

## Examples

### Basic Scene

```kern
let e = import("3dengine")
let u = e["ursina"]

let app = u["App"]()

let player = u["Entity"](app, {
    "model": "cube",
    "color": [0, 0.5, 1],
    "position": [0, 0, 0]
})

let ground = u["Entity"](app, {
    "model": "plane", 
    "color": [0.3, 0.3, 0.3],
    "position": [0, -1, 0],
    "scale": [10, 1, 10]
})

def update(dt) {
    if (u["input"]["is_key_down"]("w")) {
        let pos = u["entity_get_position"](player)
        u["entity_set_position"](player, [pos[0], pos[1], pos[2] + 5.0 * dt])
    }
}

// app.run()  // Start the game
```

### Look-At Camera

```kern
let enemy = u["Entity"](app, {"model": "cube", "position": [5, 0, 5]})
let camera = u["Entity"](app, {"model": "camera", "position": [0, 2, -5]})

def update(dt) {
    // Camera always faces enemy
    let enemyPos = u["entity_get_position"](enemy)
    u["entity_look_at"](camera, enemyPos)
}
```

---

## Files

| File | Description |
|------|-------------|
| `kern/modules/3dengine/ursina_api.h` | Header file |
| `kern/modules/3dengine/ursina_api.cpp` | Implementation |
| `examples/graphics/3dengine_ursina_cube.kn` | Example scene |
| `tests/3dengine_ursina_style.kn` | API test |
| `tests/3dengine_entity_mapping.kn` | Entity test |
| `tests/3dengine_auto_loop.kn` | Loop test |

---

## Success Criteria

| Criteria | Status |
|----------|--------|
| User never touches raw ECS | ✅ |
| <10 lines for simple 3D scene | ✅ |
| Entity-based API | ✅ |
| Engine owns loop | ✅ |
| Auto-update registration | ✅ |
| Input abstraction | ✅ |
| ECS + scheduler underneath | ✅ |
| g3d backend unchanged | ✅ |

---

## Compilation Required

Rebuild Kern binary to include Ursina API:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Test:
```bash
kern tests/3dengine_ursina_style.kn
kern examples/graphics/3dengine_ursina_cube.kn
```
