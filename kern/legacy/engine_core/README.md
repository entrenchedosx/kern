# Legacy Engine Core - ARCHIVED

**Status:** ARCHIVED - No longer part of active Kern architecture  
**Date:** May 10, 2026  
**Reason:** Dual-core architecture collision with new modular runtime system

---

## 🚨 CRITICAL - DO NOT USE

This directory contains the **old monolithic engine core** that has been replaced by the new modular runtime system.

- ❌ **DO NOT** include these files in new code
- ❌ **DO NOT** reference these headers
- ❌ **DO NOT** link against these implementations

---

## 📁 What Was Moved Here

```
kern/legacy/engine/core/
├── component.h/.cpp      # Old component system
├── entity.h/.cpp         # Old entity system  
├── world.h/.cpp/.inl     # Old world container
└── README.md             # This file
```

---

## ✅ NEW ARCHITECTURE LOCATION

Use the new modular system instead:

```
kern/runtime/core/           # NEW runtime core
kern/runtime/modules/ecs/    # NEW ECS module
```

---

## 🔄 Migration Path

The new modular system provides equivalent functionality:

| Old System | New System |
|------------|------------|
| `kern/engine/core/entity.h` | `kern/runtime/modules/ecs/entity_system.h` |
| `kern/engine/core/component.h` | `kern/runtime/modules/ecs/component_system.h` |
| `kern/engine/core/world.h` | `kern/runtime/core/runtime.h` + ECS module |

---

## 📋 Archive Reason

This was archived to resolve the **dual-core architecture collision** where:

- Old engine core still thought it was authoritative
- New runtime system was trying to replace it
- ECS module was incorrectly depending on old core

Having two competing entity/component systems caused:
- Circular dependencies
- Confusion about which system to use
- Architecture leaks that prevented true modularity

---

**This legacy system is preserved for historical reference only. All new development must use the modular runtime system.**
