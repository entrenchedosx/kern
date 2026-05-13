# Legacy VM Headers - ARCHIVED

**Status:** ARCHIVED - No longer part of active Kern runtime  
**Date:** May 10, 2026  
**Reason:** VM layer consolidation - multiple variants causing build conflicts

---

## 🚨 CRITICAL - DO NOT USE

This directory contains the **old VM header variants** that have been replaced by a unified VM interface.

- ❌ **DO NOT** include these headers in new code
- ❌ **DO NOT** reference these VM variants
- ❌ **DO NOT** link against these implementations

---

## 📁 What Was Moved Here

```
kern/legacy/vm/
├── vm.hpp               # Original VM with old include patterns
└── README.md            # This file
```

---

## ✅ NEW VM INTERFACE

Use the unified VM interface:

```
kern/runtime/vm/vm.hpp  ← SINGLE SOURCE OF TRUTH
```

This is the consolidated VM that:
- Has clean include patterns
- Uses consistent dependency direction
- Provides unified VM interface
- Supports the modular runtime architecture

---

## 🔄 Migration Path

The old vm.hpp has been replaced by the refactored version:

| Old VM | New VM |
|--------|--------|
| `kern/legacy/vm/vm.hpp` | `kern/runtime/vm/vm.hpp` (consolidated) |

---

## 📋 Archive Reason

These VM variants were archived because they:

- Created fragmented VM identity
- Had inconsistent include patterns
- Caused build system conflicts
- Prevented proper dependency validation
- Made VM layer unstable for modular architecture

---

## 🎯 VM Consolidation Result

After consolidation:

- ✅ Single VM definition (vm.hpp)
- ✅ Consistent include paths
- ✅ Clean dependency direction
- ✅ Stable build foundation
- ✅ Ready for build validation

---

**These legacy VM headers are preserved for historical reference only. All new development must use the unified VM interface.**
