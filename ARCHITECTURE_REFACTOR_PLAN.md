# Kern Architecture Refactor: Runtime-First Modular System

## 🎯 Goal
Transform Kern from "game engine with scripting" to "language runtime with pluggable engine modules"

## Current Problem (Architecture Drift)
```
BEFORE (Tightly Coupled):
Kern Language → g2d/g3d/ECS (tightly coupled) → VM buried inside
     ↓
[Monolithic - can't use language without loading graphics]

AFTER (Modular):
Kern Language → KernRuntime (VM core) → Optional Modules (ECS, g3d, etc.)
     ↓
[VM works standalone - modules loaded on demand]
```

## New Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     KERN LANGUAGE LAYER                          │
│  Compiler → Bytecode → VM execution                              │
└────────────────────┬────────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────────┐
│                     KERN RUNTIME CORE (Minimal)                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ VM Instance │  │ Module Reg  │  │ Scheduler   │             │
│  │             │  │             │  │ (tick loop) │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │           NATIVE BINDING LAYER (C++ ↔ VM)                  ││
│  │  • Type-safe function registration                         ││
│  │  • Safe bridging between native and VM                     ││
│  └─────────────────────────────────────────────────────────────┘│
└────────────────────┬───────────────────────────────────────────┘
                     │ Module Loading (optional)
        ┌────────────┼────────────┐
        ▼            ▼            ▼
┌──────────────┐ ┌──────────┐ ┌──────────┐
│  ECS Module  │ │ g3d Mod  │ │  io Mod  │
│  (optional)  │ │(optional)│ │(optional)│
└──────────────┘ └──────────┘ └──────────┘
```

## Folder Structure (New)

```
kern/
├── language/                    # Kern language (compiler, parser)
│   ├── compiler/
│   ├── lexer/
│   └── parser/
│
├── runtime/                     # KERN RUNTIME CORE (minimal)
│   ├── core/                    # VM + scheduler + module registry
│   │   ├── runtime.h/cpp         # Main runtime container
│   │   ├── vm_instance.h/cpp   # VM wrapper
│   │   ├── scheduler.h/cpp     # Tick/update loop
│   │   └── module_registry.h   # Module loading system
│   │
│   ├── bindings/                # NATIVE BINDING LAYER
│   │   ├── native_bindings.h   # C++ → VM registration API
│   │   ├── type_bridge.h       # Type conversion VM ↔ C++
│   │   └── function_binding.h  # Function wrapper templates
│   │
│   └── modules/                 # RUNTIME MODULES (optional plugins)
│       ├── module.h            # Module interface
│       │
│       ├── ecs/                 # ECS Module (moved from core)
│       │   ├── ecs_module.h/cpp
│       │   ├── entity_system.h/cpp
│       │   └── transform_system.h/cpp
│       │
│       ├── g3d/                 # 3D Graphics Module
│       │   ├── g3d_module.h/cpp
│       │   └── renderer.h/cpp
│       │
│       ├── g2d/                 # 2D Graphics Module
│       │   └── g2d_module.h/cpp
│       │
│       ├── io/                  # I/O Module
│       │   └── io_module.h/cpp
│       │
│       └── math/                # Math Module
│           └── math_module.h/cpp
│
└── host/                        # HOST APPLICATION (game/editor/CLI)
    ├── game_host/              # Example game host
    ├── editor_host/            # Editor application
    └── cli_host/               # Command-line REPL
```

## Key Principles

### 1. VM is the Center
- Runtime core owns ONE VM instance
- Everything executes through VM
- No direct C++ gameplay logic

### 2. Modules are Optional
- Kern runs without any modules loaded
- Modules register themselves at runtime
- No circular deps: modules can't depend on each other

### 3. Core is Minimal
- Runtime core: ~5 files, ~2000 lines
- Only VM, scheduler, module registry, bindings
- NO graphics, NO ECS, NO game logic in core

### 4. Clean Boundaries
```cpp
// VM Core (runtime/core/)
class KernRuntime {
    VM vm_;                                    // Central VM
    ModuleRegistry modules_;                   // Loaded modules
    Scheduler scheduler_;                      // Update loop
    NativeBindingLayer bindings_;              // C++ interop
};

// Module (runtime/modules/ecs/)
class ECSModule : public IModule {
    void registerWith(KernRuntime* runtime);   // Self-registration
    void update(float dt);                     // Called by scheduler
    // Owns: entities, components, transforms
    // Does NOT own: VM, runtime, global state
};

// Host (host/game_host/)
class GameHost {
    KernRuntime runtime_;                      // Language runtime
    // Loads modules as needed:
    // runtime_.loadModule<ECSModule>();
    // runtime_.loadModule<G3DModule>();
};
```

## Success Criteria

- [ ] Kern compiles after refactor
- [ ] VM can run WITHOUT ECS or graphics
- [ ] ECS loads as optional module
- [ ] No circular dependency runtime ↔ modules
- [ ] Module registration is self-contained
- [ ] Native binding layer works (C++ functions callable from VM)

## Migration Path

### Phase 1: Create New Structure (parallel to existing)
- Create `kern/runtime/` alongside existing code
- Don't touch existing g2d/g3d/ECS yet
- Build new modular system

### Phase 2: Port Existing Systems to Modules
- Move ECS → `kern/runtime/modules/ecs/`
- Move g3d → `kern/runtime/modules/g3d/`
- Wrap as IModule implementations

### Phase 3: Deprecate Old Structure
- Old `kern/modules/g3d/` marked deprecated
- Redirect to new module system
- Remove after transition

## Implementation Order

1. **Runtime Core** (`runtime/core/`)
   - VM instance wrapper
   - Module registry
   - Scheduler

2. **Binding Layer** (`runtime/bindings/`)
   - Type bridge VM ↔ C++
   - Function registration API

3. **Module Interface** (`runtime/modules/`)
   - IModule base class
   - Self-registration pattern

4. **ECS Module** (`runtime/modules/ecs/`)
   - Port existing ECS code
   - Wrap as IModule
   - Remove from core

5. **Host Examples** (`host/`)
   - Minimal host (VM only)
   - Game host (VM + ECS + g3d)
   - Editor host (VM + all modules)
