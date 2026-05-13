# Kern Engine Dependency Graph
## Visual System Interactions & Build Order

**Purpose:** Shows what breaks if you change one module, and the correct build sequence.

---

## 🎯 System Dependency Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         KERN ENGINE DEPENDENCY GRAPH                        │
└─────────────────────────────────────────────────────────────────────────────┘

LEGEND:
  [BLOCK]     = Core System
  (module)    = Sub-module
  "file"      = Implementation file
  ───►        = Depends on (arrow points to dependency)
  ═══►        = Strong dependency (cannot function without)
  ──►         = Weak dependency (optional enhancement)
  ★           = Critical path
  ⚠️           = Breaking change risk


═══════════════════════════════════════════════════════════════════════════════
PHASE 1: FOUNDATION (Build These FIRST)
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│ 1. ENTITY COMPONENT SYSTEM (ECS) ★                                          │
└─────────────────────────────────────────────────────────────────────────────┘

[Entity Registry] ══════════════════════════════════════════════════════════════╗
│  • entity.h/cpp                                                              ║
│  • ID generation + recycling                                                 ║
│  • Sparse set storage pattern                                                  ║
└──────────────────────────────────────────────────────────────────────────────╨──►
                                                                                      ╗
[Component System] ════════════════════════════════════════════════════════════════╬══►
│  • component.h                                                               ║   ║
│  • Component base class                                                      ║   ║
│  • ComponentStorage<T> template                                              ║   ║
│  • Type registration                                                           ║   ║
└──────────────────────────────────────────────────────────────────────────────╨───╨──►
                                                                                           ╗
[World/Scene] ═══════════════════════════════════════════════════════════════════════════╬═══╗
│  • world.h/cpp                                                               ║           ║ ║
│  • Entity ownership                                                          ║           ║ ║
│  • Component queries                                                         ║           ║ ║
│  • Update scheduling                                                         ║           ║ ║
└──────────────────────────────────────────────────────────────────────────────╨───────────╨─╨──►
                                                                                                       ╗
                                                                                                       ║
┌─────────────────────────────────────────────────────────────────────────────┐                        ║
│ 2. SCENE GRAPH ★                                                            │                        ║
└─────────────────────────────────────────────────────────────────────────────┘                        ║
                                                                                                         ║
[Transform Component] ═════════════════════════════════════════════════════════╗                       ║
│  • transform.h/cpp                                                           ║                       ║
│  • Local position/rotation/scale                                             ║                       ║
│  • World matrix cache                                                        ║                       ║
│  • Requires: Component System                                                ║                       ║
└──────────────────────────────────────────────────────────────────────────────╨──► [World]            ║
                                                                                      (updates)        ║
[Hierarchy System] ═════════════════════════════════════════════════════════════╗                      ║
│  • hierarchy.h/cpp                                                           ║                      ║
│  • Parent-child links                                                        ║                      ║
│  • Tree traversal                                                            ║                      ║
│  • Requires: Transform Component, World                                      ║                      ║
└──────────────────────────────────────────────────────────────────────────────╨──► [World]            ║
                                                                                      (manages)        ║
                                                                                                         ║
                                                                                                         ║
┌─────────────────────────────────────────────────────────────────────────────┐                        ║
│ 3. SERIALIZATION ★                                                          │                        ║
└─────────────────────────────────────────────────────────────────────────────┘                        ║
                                                                                                         ║
[Type Registry] ═══════════════════════════════════════════════════════════════╗                      ║
│  • type_registry.h/cpp                                                       ║                      ║
│  • Component type → name mapping                                             ║                      ║
│  • Property reflection                                                       ║                      ║
│  • Requires: Component System                                                ║                      ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Component System]

[Scene Format] ═══════════════════════════════════════════════════════════════╗
│  • scene_format.h/cpp                                                        ║
│  • JSON/Binary serialization                                               ║
│  • Entity GUIDs                                                              ║
│  • Requires: Entity Registry, Component System, Transform, Type Registry   ║
└──────────────────────────────────────────────────────────────────────────────╨──► [World] + [Type Registry]


═══════════════════════════════════════════════════════════════════════════════
PHASE 2: RENDER PIPELINE (Build AFTER Foundation)
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│ 4. GPU ABSTRACTION                                                          │
└─────────────────────────────────────────────────────────────────────────────┘

[Buffer Management] ──────────────────────────────────────────────────────────╗
│  • gpu/buffer.h/cpp                                                          ║
│  • VBO/IBO abstraction                                                       ║
│  • Optional dependency on graphics API                                     ║
└──────────────────────────────────────────────────────────────────────────────╨──► (Raylib/OpenGL)

[Texture System] ─────────────────────────────────────────────────────────────╗
│  • gpu/texture.h/cpp                                                         ║
│  • Texture loading                                                           ║
│  • Texture atlas support (optional)                                        ║
└──────────────────────────────────────────────────────────────────────────────╨──► (Raylib/OpenGL)

┌─────────────────────────────────────────────────────────────────────────────┐
│ 5. SHADER & MATERIAL SYSTEM ★                                             │
└─────────────────────────────────────────────────────────────────────────────┘

[Shader Pipeline] ════════════════════════════════════════════════════════════╗
│  • renderer/shader.h/cpp                                                     ║
│  • Shader loading (GLSL → compiled)                                        ║
│  • Uniform management                                                        ║
│  • Shader cache                                                              ║
│  • Requires: GPU Buffer, Texture                                             ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Buffer] + [Texture]
                                                                                      (strong)
[Material System] ══════════════════════════════════════════════════════════════╗
│  • renderer/material.h/cpp                                                     ║
│  • Shader + parameters                                                       ║
│  • Texture binding                                                           ║
│  • Material instances                                                        ║
│  • Requires: Shader Pipeline                                                 ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Shader Pipeline]

┌─────────────────────────────────────────────────────────────────────────────┐
│ 6. RENDER COMPONENTS                                                        │
└─────────────────────────────────────────────────────────────────────────────┘

[MeshRenderer Component] ═════════════════════════════════════════════════════╗
│  • scene/mesh_renderer.h/cpp                                                 ║
│  • Mesh + Material reference                                                 ║
│  • Render command generation                                                 ║
│  • Requires: Material System, Transform, Component System                  ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Material] + [Transform] + [World]

[Camera Component] ═════════════════════════════════════════════════════════════╗
│  • scene/camera.h/cpp                                                        ║
│  • View/projection matrices                                                  ║
│  • Culling planes                                                            ║
│  • Requires: Transform, Component System                                     ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Transform] + [World]

[Light Component] ───────────────────────────────────────────────────────────────╗
│  • scene/light.h/cpp                                                         ║
│  • Light parameters                                                          ║
│  • Shadow data (optional v2)                                                 ║
│  • Requires: Transform, Component System                                   ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Transform] + [World]


═══════════════════════════════════════════════════════════════════════════════
PHASE 3: WORLD SIMULATION (Build AFTER Render Pipeline)
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│ 7. PHYSICS INTEGRATION                                                      │
└─────────────────────────────────────────────────────────────────────────────┘

[Physics World] ══════════════════════════════════════════════════════════════╗
│  • physics/world.h/cpp                                                       ║
│  • External physics engine (Bullet/PhysX)                                  ║
│  • Simulation stepping                                                       ║
└──────────────────────────────────────────────────────────────────────────────╨──► (Bullet/PhysX library)

[Rigidbody Component] ══════════════════════════════════════════════════════════╗
│  • components/rigidbody.h/cpp                                                ║
│  • Physics body properties                                                   ║
│  • Mass, drag, constraints                                                 ║
│  • Requires: Physics World, Transform, Component System                  ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Physics World] + [Transform] + [World]

[Collider Component] ═════════════════════════════════════════════════════════╗
│  • components/collider.h/cpp                                                 ║
│  • Shape definitions                                                         ║
│  • Trigger vs. collider                                                      ║
│  • Requires: Physics World, Transform, Component System                  ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Physics World] + [Transform] + [World]

        ╔═══════════════════════════════════════════════════════════════════════════╗
        ║                                                                             ║
        ║   CRITICAL SYNC POINT:                                                    ║
        ║   Physics updates → writes to Transform.worldMatrix                     ║
        ║   ⚠️  Must NOT conflict with Scene Graph transform updates                ║
        ║                                                                             ║
        ╚═══════════════════════════════════════════════════════════════════════════╝

┌─────────────────────────────────────────────────────────────────────────────┐
│ 8. ANIMATION SYSTEM                                                         │
└─────────────────────────────────────────────────────────────────────────────┘

[Animation Clip] ═════════════════════════════════════════════════════════════╗
│  • animation/clip.h/cpp                                                      ║
│  • Keyframe data                                                             ║
│  • Sample interpolation                                                      ║
└──────────────────────────────────────────────────────────────────────────────╨──► (Asset loading)

[Animator Component] ════════════════════════════════════════════════════════════╗
│  • components/animator.h/cpp                                                 ║
│  • State machine                                                             ║
│  • Blend trees                                                               ║
│  • Requires: Animation Clip, Transform, Component System                 ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Animation Clip] + [Transform] + [World]

[Skeleton/Skinning] ══════════════════════════════════════════════════════════╗
│  • animation/skeleton.h/cpp                                                  ║
│  • Bone hierarchy                                                            ║
│  • Skinning matrices                                                         ║
│  • Requires: Transform hierarchy                                           ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Hierarchy System] + [MeshRenderer]


═══════════════════════════════════════════════════════════════════════════════
PHASE 4: TOOLING (Build AFTER World Systems)
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│ 9. EDITOR INTEGRATION                                                       │
└─────────────────────────────────────────────────────────────────────────────┘

[Editor World] ═══════════════════════════════════════════════════════════════╗
│  • editor_core/editor_world.h/cpp                                          ║
│  • Wraps engine World                                                       ║
│  • Undo/redo system                                                          ║
│  • Edit-time only components                                               ║
│  • Requires: World, Serialization                                            ║
└──────────────────────────────────────────────────────────────────────────────╨──► [World] + [Scene Format]

[Hierarchy Panel] ════════════════════════════════════════════════════════════╗
│  • gui/hierarchy_panel.h/cpp                                               ║
│  • Entity tree view                                                          ║
│  • Drag-drop parenting                                                       ║
│  • Requires: Editor World, Hierarchy System                                ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Editor World] + [Hierarchy System]

[Inspector Panel] ══════════════════════════════════════════════════════════════╗
│  • gui/inspector_panel.h/cpp                                               ║
│  • Property editing                                                          ║
│  • Requires: Editor World, Type Registry, Reflection                       ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Editor World] + [Type Registry]

[Viewport] ════════════════════════════════════════════════════════════════════╗
│  • gui/viewport.h/cpp                                                      ║
│  • 3D scene view                                                             ║
│  • Gizmo rendering                                                           ║
│  • Requires: g3d_v2 renderer, Editor World, Camera                       ║
└──────────────────────────────────────────────────────────────────────────────╨──► [g3d_v2] + [Editor World] + [Camera]

[Gizmo System] ───────────────────────────────────────────────────────────────╗
│  • tools/gizmo.h/cpp                                                       ║
│  • Translate/Rotate/Scale                                                    ║
│  • Requires: Viewport, Transform                                           ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Viewport] + [Transform]


═══════════════════════════════════════════════════════════════════════════════
PHASE 5: VM INTEGRATION (Build LAST)
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│ 10. SCRIPTING BRIDGE                                                        │
└─────────────────────────────────────────────────────────────────────────────┘

[Script Component] ════════════════════════════════════════════════════════════╗
│  • components/script.h/cpp                                                   ║
│  • VM script attachment                                                      ║
│  • Lifecycle hooks (onStart/onUpdate/onDestroy)                          ║
│  • Requires: VM, Component System                                          ║
└──────────────────────────────────────────────────────────────────────────────╨──► [VM] + [World]

[Engine API Binding] ═════════════════════════════════════════════════════════╗
│  • bindings/engine_api.h/cpp                                               ║
│  • Safe API for scripts                                                      ║
│  • Entity spawn/destroy                                                      ║
│  • Transform queries                                                         ║
│  • Requires: Script Component, World, all engine systems                   ║
└──────────────────────────────────────────────────────────────────────────────╨──► [Script Component] + [World] + [ALL SYSTEMS]


═══════════════════════════════════════════════════════════════════════════════
CRITICAL DEPENDENCY CHAINS (What Breaks What)
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│ BREAKING CHANGE RISK ANALYSIS                                               │
└─────────────────────────────────────────────────────────────────────────────┘

⚠️  HIGH RISK: Entity ID Format Change
┌────────────────────────────────────────────────────────────────────────────┐
│ If you change EntityId from uint64_t to something else:                    │
│                                                                              │
│  [Entity Registry] ──► ALL COMPONENTS ──► ALL SYSTEMS ──► EDITOR           │
│       ⚠️               ⚠️                  ⚠️              ⚠️            │
│                                                                              │
│  IMPACT: Complete engine rebuild required                                  │
│  FIX: Keep EntityId abstracted, never expose raw ID format                 │
└────────────────────────────────────────────────────────────────────────────┘

⚠️  HIGH RISK: Component Storage Pattern Change
┌────────────────────────────────────────────────────────────────────────────┐
│ If you change from sparse set to dense array:                              │
│                                                                              │
│  [ComponentStorage] ──► [World] ──► ALL COMPONENT QUERIES ──► SYSTEMS    │
│       ⚠️               ⚠️              ⚠️                      ⚠️        │
│                                                                              │
│  IMPACT: All iteration code breaks                                         │
│  FIX: Abstract storage behind iterator interface                           │
└────────────────────────────────────────────────────────────────────────────┘

⚠️  HIGH RISK: Transform Matrix Layout Change
┌────────────────────────────────────────────────────────────────────────────┐
│ If you change from glm::mat4 to custom matrix:                             │
│                                                                              │
│  [Transform] ──► [Hierarchy] ──► [MeshRenderer] ──► [Renderer] ──► GPU  │
│       ⚠️          ⚠️               ⚠️                ⚠️            ⚠️     │
│                                                                              │
│  IMPACT: All rendering breaks                                                │
│  FIX: Keep Transform as opaque, expose GetMatrix() only                      │
└────────────────────────────────────────────────────────────────────────────┘

⚠️  MEDIUM RISK: Serialization Format Change
┌────────────────────────────────────────────────────────────────────────────┐
│ If you change .kscene JSON format:                                         │
│                                                                              │
│  [Scene Format] ──► [Editor] ──► [User Projects]                           │
│       ⚠️           ⚠️              ⚠️ (BREAKS USER SCENES!)              │
│                                                                              │
│  IMPACT: All saved scenes become unloadable                                │
│  FIX: Version field in format, migration tool, backwards compatibility     │
└────────────────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
BUILD ORDER EXECUTION (Week-by-Week Dependencies)
═══════════════════════════════════════════════════════════════════════════════

WEEK 1: Foundation Core
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Day 1-2: Entity Registry
  └─► Can create/destroy entities
  └─► ID recycling works
  └─► TESTS PASS

Day 3-4: Component Base + Storage
  └─► Can add/remove components
  └─► Sparse set iteration works
  └─► TESTS PASS

Day 5: World Container
  └─► Integrates Entity + Components
  └─► Basic queries work
  └─► TESTS PASS

WEEK 2: Scene Graph
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Day 1-2: Transform Component
  └─► Local/world matrices
  └─► Cache dirty flag
  └─► TESTS PASS

Day 3-4: Hierarchy System
  └─► Parent-child links
  └─► Tree traversal
  └─► TESTS PASS

Day 5: World Integration
  └─► Update transforms in World::update()
  └─► 1000 entity stress test
  └─► TESTS PASS

WEEK 3: Serialization
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Day 1-2: Type Registry + Reflection
  └─► Component type → name
  └─► Property inspection
  └─► TESTS PASS

Day 3-4: Scene Format (JSON)
  └─► Can serialize World
  └─► Can deserialize World
  └─► GUIDs stable
  └─► TESTS PASS

Day 5: Round-trip Testing
  └─► Save → Load → Compare
  └─► Hierarchy preserved
  └─► TESTS PASS

WEEK 4: Material + Shader
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Day 1-2: Shader Pipeline
  └─► Load GLSL
  └─► Compile/Cache
  └─► TESTS PASS

Day 3-4: Material System
  └─► Material instances
  └─► Uniform binding
  └─► TESTS PASS

Day 5: MeshRenderer Component
  └─► Integrates with Transform
  └─► Generates render commands
  └─► Renders cube successfully
  └─► TESTS PASS

WEEK 5: Integration
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Day 1-2: System Integration
  └─► All components work together
  └─► Example scene renders

Day 3-4: Performance Testing
  └─► 1000 entities @ 60fps
  └─► Memory profiling

Day 5: Bug Fixes
  └─► Fix integration issues
  └─► Fix memory leaks

WEEK 6: Editor (Optional)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Day 1-2: Editor World + Hierarchy Panel
Day 3-4: Inspector Panel + Viewport
Day 5: Gizmos + Integration Testing


═══════════════════════════════════════════════════════════════════════════════
INTER-SYSTEM COMMUNICATION PATTERNS
═══════════════════════════════════════════════════════════════════════════════

PATTERN 1: Component → System
┌─────────────────────────────────────────────────────────────────────────────┐
│ How systems read component data:                                             │
│                                                                              │
│  System::update(World& world) {                                              │
│      world.query<Transform, MeshRenderer>().forEach([](EntityId e,          │
│                                                      Transform& t,         │
│                                                      MeshRenderer& m) {    │
│          // System reads components, does work                             │
│          renderer.submit(m.mesh, m.material, t.worldMatrix);                │
│      });                                                                   │
│  }                                                                         │
└─────────────────────────────────────────────────────────────────────────────┘

PATTERN 2: System → Component Modification
┌─────────────────────────────────────────────────────────────────────────────┐
│ How systems write component data:                                          │
│                                                                              │
│  PhysicsSystem::update(World& world, float dt) {                          │
│      world.query<Rigidbody, Transform>().forEach([](EntityId e,           │
│                                                   Rigidbody& rb,          │
│                                                   Transform& t) {         │
│          // Physics writes to world matrix                                  │
│          t.worldMatrix = physics.calculateTransform(rb.body);              │
│          t.worldDirty = false;  // Mark as already updated                 │
│      });                                                                   │
│  }                                                                         │
│                                                                              │
│  // Later in frame, TransformSystem skips these                            │
│  TransformSystem::update(World& world) {                                    │
│      // Only update entities NOT touched by physics                         │
│  }                                                                         │
└─────────────────────────────────────────────────────────────────────────────┘

PATTERN 3: Event System (Decoupled)
┌─────────────────────────────────────────────────────────────────────────────┐
│ For loose coupling between systems:                                        │
│                                                                              │
│  // Physics emits event                                                    │
│  eventSystem.emit<CollisionEvent>(entityA, entityB, contactPoint);       │
│                                                                              │
│  // Audio system subscribes                                                │
│  eventSystem.on<CollisionEvent>([](const CollisionEvent& e) {             │
│      audio.playSoundAtPosition("boom.wav", e.contactPoint);                │
│  });                                                                       │
│                                                                              │
│  // Script system subscribes                                               │
│  eventSystem.on<CollisionEvent>([](const CollisionEvent& e) {             │
│      scriptSystem.callOnCollision(e.entityA, e.entityB);                   │
│  });                                                                       │
└─────────────────────────────────────────────────────────────────────────────┘


═══════════════════════════════════════════════════════════════════════════════
API STABILITY PROMISES
═══════════════════════════════════════════════════════════════════════════════

STABLE (Won't change in v2.x):
  ✓ EntityId type (always uint64_t)
  ✓ Component base class interface
  ✓ World::createEntity() / destroyEntity()
  ✓ Component storage pattern (sparse set)
  ✓ Transform component layout
  ✓ .kscene format version 1.0

UNSTABLE (May change in v2.x):
  ⚠️  Internal renderer API ( evolving)
  ⚠️  Shader language details (GLSL/HLSL abstraction coming)
  ⚠️  Editor UI layout (iterating on UX)
  ⚠️  Physics API (depends on chosen engine)
  ⚠️  Animation state machine API (evolving)


═══════════════════════════════════════════════════════════════════════════════
MIGRATION COMPLEXITY BY SYSTEM
═══════════════════════════════════════════════════════════════════════════════

EASY MIGRATION (Low coupling):
  [Audio]          ──► Only depends on Transform, easy to replace
  [Particle]       ──► Self-contained, optional system
  [UI]            ──► Separate layer, doesn't affect core

MEDIUM MIGRATION (Medium coupling):
  [Animation]      ──► Tightly coupled to Transform + Skeleton
  [Physics]        ──► Coupled to Transform, requires sync logic
  [Material]       ──► Coupled to Shader, Renderer

HARD MIGRATION (High coupling):
  [Entity ID]      ═══► Affects EVERYTHING
  [Component]      ═══► Affects all component types
  [Transform]      ═══► Affects hierarchy, rendering, physics
  [Scene Format]   ═══► Affects all saved scenes


═══════════════════════════════════════════════════════════════════════════════
END OF DEPENDENCY GRAPH
═══════════════════════════════════════════════════════════════════════════════

Next Steps:
1. Review this graph for your specific constraints
2. Identify which systems you can skip for MVP
3. Use the Week-by-Week order for implementation
4. Monitor the Breaking Change Risks during development

