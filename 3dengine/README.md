# 3dengine

High-performance 3D engine framework for Kern, designed for clean DX with strict engine boundaries.

This package currently contains **Phase 1 Core Foundation**:
- math primitives (`vec2`, `vec3`, `mat4`)
- app lifecycle and hybrid game loop
- backend abstraction over `g2d` / `game`
- first-render example
- explicit fixed-step guardrails (`max_frame_dt`, `max_fixed_steps`)
- time scaling (`time_scale`)

Public API is intentionally small:
- `app`
- `time`
- `vec2`, `vec3`, `mat4`
- `ecs`
- `conventions()`, `version()`

## Install

```powershell
kern install 3dengine
```

## Quick start

```kn
let u = import("3dengine")
let app = u["app"]
let draw_frame = lambda (s, dt, alpha) => { s["gfx"]["rect"](120, 100, 180, 120, 90, 170, 255, 255) s["gfx"]["text"]("Hello 3dengine", 130, 170, 24, 255, 255, 255) }
app.run({"title":"3dengine","width":960,"height":540,"fixed_hz":60.0,"time_scale":1.0}, null, draw_frame)
```

## Run local example

From this package root:

```powershell
kern run examples/first_render.kn --allow=filesystem.read,filesystem.write,process.control
```

If your build does not include the Raylib game modules, rendering APIs like `g2d`/`game` will be unavailable.

## Phase 2 ECS (current)

- Entity IDs use `index + generation` for stale-handle safety
- Component storage uses dense/sparse SoA-style layout
- Static deterministic fixed pipeline:
  - `input`
  - `simulation`
  - `transform_propagation`
  - `rendering_prep`
- Transform hierarchy supports `parent`, `first_child`, `next_sibling` and dirty propagation
- Stale-handle protection through `index + generation` validation in public ECS APIs
- Cycle prevention in `world_set_parent`
- Destroy behavior: children are detached to root (not recursively destroyed)
- Metrics: `world_entity_count`, `world_transform_count`, `world_velocity_count`
- Debug toggles: `set_debug(bool)`, `set_logging(bool)`, `config()`

Basic use:

```kn
let u = import("3dengine")
let ecs = u["ecs"]
let w = ecs["world_create"]()
let e = ecs["world_create_entity"](w)
ecs["world_add_transform"](w, e, ecs["transform_make"](u["vec3"]["zero"](), null, null))
```

Run ECS demo:

```powershell
kern run examples/ecs_transform.kn --allow=filesystem.read,filesystem.write,process.control
```

## ECS tests

From package root:

```powershell
kern run tests/ecs_basic.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_hierarchy.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_destroy.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_determinism.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_render_prep.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_render_determinism.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_render_batching.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_render_depth_order.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_render_camera_3d.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_input.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_registry_lock.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_scheduler.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_query_api.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_stage_guards.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_system_spec.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_system_graph.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_parallel_plan.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_parallel_runtime.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_parallel_serial_parity_hash.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_parallel_auto.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_scheduler_policy_presets.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_conflict_graph.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_conflict_graph_normalized.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_conflict_graph_hash.kn --allow=filesystem.read,filesystem.write,process.control
```

## Headless core split

- `src/headless.kn` is a graphics-free entrypoint for deterministic ECS/simulation testing.
- Use `import("src/headless.kn")` when you only need math + ECS + input + registries.
- Keep `import("3dengine")` (`src/index.kn`) for the full runtime (window/app/render bridge).
- The split keeps CI and simulation tests decoupled from `g2d`/`game` availability.

## Query mutation rule

Do not structurally mutate component stores while iterating a query result in the same loop body.
If structural changes are needed, defer them until after query iteration.

## Phase 3 Rendering Bridge (current)

- New render components:
  - `mesh` (`mesh_id` handle)
  - `material` (`material_id` handle)
- Registries are world-owned and deduplicated:
  - `mesh_registry`: `mesh_id -> { primitive_type, vertices, indices }`
  - `material_registry`: `material_id -> { material_id, color }`
- Renderable entities are composition-only:
  - `Transform + Mesh + Material`
- Render prep pipeline:
  - `world_query_renderables` -> command buffer in `world["render"]["commands"]`
- Renderer consumes prepared commands (does not query ECS directly):
  - `world_run_render_prep(world)`
  - `world_render(world, gfx)`
- Render command contract (stable schema):
  - `command.entity` (`EntityID`)
  - `command.depth_value` (`float`, sorted ascending in render prep)
  - `command.transform` (`mat4`)
  - `command.mesh_id` (`string`)
  - `command.material_id` (`string`)
  - `command.screen` (`{ x, y }`)
- Primitive helpers:
  - `mesh_make_quad()`
  - `mesh_make_cube()`
  - `mesh_make("quad"|"cube")`
- Render stats:
  - `world_render_stats(world)` returns `command_count`, `draw_calls`, `batched_draw_calls`, `unique_materials`, `rendered_entities`, `mesh_registry_size`, `material_registry_size`, `batch_sizes`
- Optional camera integration:
  - `camera_make(position, zoom, projection, mode)`
  - `world_set_camera(world, camera)`
  - `world_get_camera(world)`
  - supported `mode`: `"2d"` (default) and `"3d"`
  - camera data includes `view_matrix` and `projection_matrix`
- Structural ECS mutation is blocked during render prep in debug mode.
- **Registry immutability:** `locks.registry_write` is set during `render_prep` and `world_render`. New mesh/material registry entries are rejected while locked (existing dedup lookups still succeed). Handle IDs are append-only and never reused.

## Phase 4.0 Input (ECS)

- Per-world input snapshot in `world["input"]`: `held_keys`, `pressed_keys`, `released_keys` (string key maps, e.g. `"W"`, `"A"`).
- Feed from tests or your window layer before the fixed pipeline: `world_input_feed(world, held_array, pressed_array, released_array)`.
- `velocity_make(linear, input_move_speed)` — when `input_move_speed > 0`, the **input** system (first stage in the fixed pipeline) sets `velocity.linear` from WASD held keys only; it does not add/remove components or entities.
- Query helpers: `input_key_held`, `input_key_pressed`, `input_key_released`.

## System scheduler (deterministic)

- Fixed pipeline is now scheduler-driven: built-ins (`input`, `simulation`, `transform_propagation`, `rendering_prep`) plus custom systems.
- Register/unregister custom systems:
  - `world_register_system(world, stage, name, fn, after_names)`
  - `world_unregister_system(world, name)`
- Inspect resolved execution order:
  - `world_stage_order(world)`
  - `world_system_order(world)`
- Stage names are explicit and ordered: `PRE_INPUT`, `INPUT`, `SIMULATION`, `POST_SIMULATION`, `TRANSFORM`, `RENDER_PREP`.
- Registration requires a valid stage and stage boundaries are enforced by the resolver (dependencies cannot point to later stages).
- Dependency ordering is resolved deterministically whenever scheduler state is dirty.

## Stage-scoped guards

- Runtime tracks active scheduler context (`current_stage`, `current_system`) during fixed-pipeline execution.
- Structural operations (`world_create_entity`, `world_destroy_entity`, add/remove component APIs, hierarchy changes, registry registration) are blocked unless stage policy permits them.
- Write operations are stage-guarded by key (`input`, `transform`, `velocity`, `mesh`, `material`, `registry`, `render`).
- Custom systems can declare overrides at registration time:
  - `write_mask`: allowed write keys for that system
  - `allow_structural_mutation`: opt-in structural mutation for that system
- Stage policies are configurable:
  - `world_set_stage_policy(world, stage, {"allow_structural_mutation": bool, "allowed_writes": [...]})`

## Declarative system spec helper

- `world_register_system_spec(world, spec)` registers a system with one object:
  - `name`, `stage`, `after`, `query`, `write`, `allow_structural_mutation`, `run`
- `run(world, dt, query_result)` receives an auto-built query result when `query` is provided.
- `write` and `allow_structural_mutation` map to the same guard model used by manual `world_register_system`.

## Scheduler introspection

- `world_system_graph(world)` returns a structured graph:
  - `stages`
  - `execution_order`
  - `systems` (stage, deps, effective writes, structural permission, builtin flag)
  - `edges` (`from -> to`)
- `world_debug_print_graph(world)` prints the same graph as JSON and returns it.

## Parallel scheduler planning (dry run)

- `world_scheduler_parallel_plan(world)` returns a dry-run plan only (no threading/execution changes).
- Planning is stage-local and deterministic.
- Systems are separated when either condition is true:
  - dependency edge exists between them
  - effective write-mask overlap exists
- Output includes:
  - per-stage `systems`
  - per-stage `batches` (parallel-capable groups)
  - per-stage `conflicts` with reasons (`dependency` or `write_overlap`)
  - `flat_batches` for easy tooling/inspection
- `world_scheduler_conflict_graph(world)` returns an analysis graph:
  - `nodes`: per-system metadata (`stage`, effective `writes`, `structural`, `builtin`)
  - `edges`: typed edges with reasons:
    - `dependency` (`from -> to`)
    - `write_conflict` (stage-local planning conflicts with overlap/dependency reason)
  - `stage_batches`: planner batches per stage for visualization/tooling
- `world_debug_print_conflict_graph(world)` prints the conflict graph as JSON and returns it.
- `world_scheduler_conflict_graph_normalized(world)` returns a canonicalized conflict graph:
  - nodes inserted in deterministic scheduler order
  - each node `writes` array in canonical write-key order
  - edges sorted by `type_rank -> from_system_rank -> to_system_rank -> stage_rank`
  - `stage_batches` in stage-order, with systems sorted by system-rank within each batch
- `world_scheduler_conflict_graph_hash(world)` returns:
  - `hash`: SHA-256 of `json_stringify(world_scheduler_conflict_graph_normalized(world))`
  - `stage_hashes`: per-stage SHA-256 hashes of stage-local normalized payloads (`nodes`, `edges`, `batches`)
  - `batch_hashes`: per-stage batch hash arrays (`[{ batch_id, hash }]`) for fine-grained change localization
  - `normalized`: the same canonical graph payload used for hashing
  - use this for fast graph-change detection in tests/CI/tooling

## Parallel runtime (deterministic)

- `world_run_fixed_pipeline_parallel(world, dt)` executes the same fixed pipeline using planned stage batches.
- Stage order and batch order remain deterministic; this path is opt-in and does not replace serial execution.
- Current implementation is safety-first and conflict-aware (planner-driven), preserving the same world result as serial execution.
- `world_parallel_runtime_stats(world)` returns metrics:
  - `enabled`
  - `mode` (`serial` | `parallel` | `auto`)
  - `stages_executed`
  - `batches_executed`
  - `parallel_batches_executed`
  - `systems_executed`
  - `stage_parallel_ratio`
  - `parallel_speedup_estimate`
  - `auto_serial_stages`
  - `auto_parallel_stages`
  - `policy_decisions`

## Adaptive runtime policy

- `world_run_fixed_pipeline_auto(world, dt, policy)` chooses serial/parallel per stage deterministically.
- Optional world-level preset APIs:
  - `world_set_scheduler_policy(world, "safe"|"balanced"|"aggressive")`
  - `world_get_scheduler_policy(world)`
- Policy fields:
  - `min_systems_for_parallel` (int, default `2`)
  - `min_parallel_ratio` (float, default `0.34`)
  - `allow_render_prep_parallel` (bool, default `false`)
- Preset mapping:
  - `safe`: conservative thresholds, no render-prep parallel
  - `balanced`: default thresholds
  - `aggressive`: low thresholds, render-prep parallel allowed
- Decision rules (in order):
  - serial if stage system count is below threshold
  - serial if stage parallel batch ratio is below threshold
  - serial for `RENDER_PREP` unless explicitly allowed
  - otherwise parallel

Conflict graph contract (stable shape):

```json
{
  "nodes": {
    "<system_name>": {
      "stage": "SIMULATION",
      "writes": ["transform"],
      "structural": false,
      "builtin": false
    }
  },
  "edges": [
    {
      "from": "physics",
      "to": "move",
      "type": "dependency",
      "reason": "dependency: physics -> move"
    },
    {
      "from": "move",
      "to": "anim",
      "type": "write_conflict",
      "stage": "SIMULATION",
      "reason": "write_overlap: [\"transform\"]"
    }
  ],
  "stage_batches": [
    { "stage": "SIMULATION", "batches": [["physics"], ["move", "fx"]] }
  ],
  "notes": ["deterministic", "dependency_edges_global", "write_conflict_edges_stage_local"]
}
```

## Query API (component-filtered)

- `world_query(world, component_names)` provides component-filtered rows over dense ECS storage.
- Result shape:
  - `entities`: aligned entity ids
  - `components`: map of arrays keyed by component name
  - `count`: row count
  - `signature`: requested component list
- Built-in convenience queries remain:
  - `world_query_transform(world)`
  - `world_query_transform_velocity(world)`
  - `world_query_renderables(world)`

Minimal render setup:

```kn
let u = import("3dengine")
let ecs = u["ecs"]
let w = ecs["world_create"]()
let e = ecs["world_create_entity"](w)
ecs["world_add_transform"](w, e, ecs["transform_make"](u["vec3"]["zero"](), null, null))
ecs["world_add_mesh"](w, e, ecs["mesh_make_cube"]())
ecs["world_add_material"](w, e, ecs["material_make"](u["vec3"]["make"](1.0, 0.3, 0.3), "m.red"))
ecs["world_set_camera"](w, ecs["camera_make"](u["vec3"]["zero"](), 1.0, "orthographic"))
```

Render determinism test:

```powershell
kern run tests/ecs_render_determinism.kn --allow=filesystem.read,filesystem.write,process.control
kern run tests/ecs_render_batching.kn --allow=filesystem.read,filesystem.write,process.control
```

## Coordinate conventions

- Handedness: `right_handed`
- Up axis: `y_up`
- Matrix layout: `row_major`
- NDC depth range: `minus_one_to_one`
