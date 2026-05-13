# Legacy Engine Tests - ARCHIVED

**Status:** ARCHIVED - No longer part of active test suite  
**Date:** May 10, 2026  
**Reason:** Tests reference old monolithic engine core that has been replaced

---

## 🚨 CRITICAL - DO NOT USE

This directory contains the **old engine tests** that validate the monolithic architecture.

- ❌ **DO NOT** run these tests with new modular system
- ❌ **DO NOT** reference these tests in new code
- ❌ **DO NOT** add new tests to this directory

---

## 📁 What Was Moved Here

```
tests/legacy/engine/
├── test_entity_registry.cpp
├── test_world.cpp
└── README.md             # This file
```

---

## ✅ NEW TEST STRUCTURE

Use the new modular test structure instead:

```
tests/runtime/
├── test_runtime_core.cpp
├── test_module_registry.cpp
└── test_scheduler.cpp

tests/modules/
├── ecs/
│   ├── test_ecs_module.cpp
│   ├── test_entity_system.cpp
│   └── test_component_system.cpp
└── g3d/
    └── test_g3d_module.cpp

tests/vm/
├── test_vm_standalone.cpp
└── test_vm_execution.cpp
```

---

## 🔄 Migration Path

The old tests validated:

| Old Test | New Test |
|----------|----------|
| `test_entity_registry.cpp` | `tests/modules/ecs/test_entity_system.cpp` |
| `test_world.cpp` | `tests/modules/ecs/test_ecs_module.cpp` |

---

## 📋 Archive Reason

These tests were archived because they:

- Reference the old monolithic `kern/engine/core/*` system
- Validate architecture that no longer exists
- Would create confusion about which system is authoritative
- Cannot run with the new modular runtime

---

**These legacy tests are preserved for historical reference only. All new development must use the modular test structure.**
