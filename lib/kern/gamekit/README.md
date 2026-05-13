# Kern GameKit Modules

This folder adds importable modules to accelerate Kern game/app/system development.

## Import paths

Use file imports like:

- `import("lib/kern/gamekit/module.kn")`
- `import("lib/kern/gamekit/ecs/world.kn")`
- `import("lib/kern/gamekit/graphics/draw2d.kn")`

## Module map

- `core/runtime.kn` - runtime helpers, struct-like maps, safe call wrappers
- `core/game_loop.kn` - app loop orchestration for 2D apps
- `ecs/world.kn` - ECS world/entities/components/systems
- `input/input.kn` - unified keyboard/mouse over g2d/g3d/game
- `graphics/draw2d.kn` - 2D draw wrappers
- `graphics/draw3d.kn` - 3D draw wrappers
- `audio/audio.kn` - cached sound loading/playback wrappers
- `assets/cache.kn` - shared texture/model/sound caches
- `scene/scene.kn` - scene registration/load/update/render manager
- `math/vector.kn` - Vector2/Vector3 operations
- `physics/collision.kn` - AABB/circle collision + integration helpers
- `ui/widgets.kn` - immediate-mode panel/label/button/toggle
- `network/socket.kn` - HTTP helpers and networking compatibility API
- `threading/tasks.kn` - cooperative task scheduler + process spawn helpers
- `debug/log.kn` - logging, profiling markers, assertions

## Notes

- ECS uses dynamic component maps and is safe by default.
- Struct-like typing is provided via `make_struct` + `__type` checks in `core/runtime.kn`.
- UI and game loop are immediate-mode and designed for fast iteration.
- Networking currently wraps available `net` functions and exposes a stable API for future socket backends.

## Examples

- `examples/gamekit_simple_game.kn`
- `examples/gamekit_gui_app.kn`
