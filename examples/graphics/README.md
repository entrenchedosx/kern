# Graphics examples

These use `import("g2d")`, `import("g3d")`, or game-related modules. Build Kern with **`KERN_BUILD_GAME=ON`** and Raylib (see repo root `README.md`). If graphics are missing, scripts usually print a short message instead of crashing.

Run from the **repository root**:

```powershell
.\build\Release\kern.exe examples\graphics\graphics_demo.kn
```

## By topic

| Kind | Files |
|------|--------|
| **Quick start** | `graphics_demo.kn` — window, 2D shapes, timed exit |
| **2D / sprites** | `g2d_shapes_plus.kn`, `g2d_render_target_gradient.kn`, `sprite_movement_integrity.kn`, `13_graphics.kn`, `15_bouncy_ball.kn` |
| **Games** | `pong.kn` |
| **3D scenes** | `3d_rotating_cube.kn`, `3d_textured_scene.kn`, `3d_interactive_scene.kn`, `3d_scene_load_save.kn`, `3d_multi_shapes.kn`, `3d_camera_*.kn`, `3d_object_registry.kn`, `3d_groups_visibility.kn` |
| **Smoke / import checks** | `g3d_import_smoke.kn` |

Data: `scene_demo.g3d` is a sample scene file for loaders that expect it.

See **[../README.md](../README.md)** for basic language examples and **[../golden/README.md](../golden/README.md)** for async and runtime demos.
