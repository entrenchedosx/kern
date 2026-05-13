# Kern Dependency Leak Detection Checklist
## Post-Refactor Validation Gate

**Purpose:** Prevent architecture regression after every major change  
**Use:** Run this checklist after any significant code changes to Kern runtime system  
**Authority:** This checklist enforces the modular architecture defined in ARCHITECTURE_FINAL_STATE.md

---

## 🚨 CRITICAL RULE VIOLATION CHECK

### ❌ FAIL if ANY of these exist:

#### Runtime Core Violations
- [ ] Runtime includes ECS headers (`#include "ecs/..."`)
- [ ] Runtime includes graphics headers (`#include "g3d/..."` or `"g2d/..."`)
- [ ] Runtime includes any module implementation headers
- [ ] Runtime directly creates module instances (not through registry)

#### Module Boundary Violations  
- [ ] Any module includes `runtime/core/*` internals (except public interfaces)
- [ ] VM calls directly into module implementation code
- [ ] Module accesses runtime private members

### ✅ Allowed only (the ONLY valid dependency chain):
```
VM → Runtime → Module Interface → Module Implementation
```

**Validation:**
```cpp
// ✅ CORRECT (Runtime owns Module Interface)
runtime.loadModule("ecs");  // Through registry only

// ❌ FORBIDDEN (Runtime knows ECS internals)
#include "ecs/entity_system.h"  // Runtime should NOT know this
```

---

## 🔁 CIRCULAR DEPENDENCY CHECK

### Build Verification Commands:
```bash
# Check for circular includes
grep -r "runtime/core/" kern/runtime/modules/
grep -r "ecs/" kern/runtime/core/
grep -r "g3d/" kern/runtime/core/
```

### ❌ FAIL if:
- [ ] ECS imports runtime core internals
- [ ] Runtime imports ECS module directly (not through interface)
- [ ] Module registry depends on module internals
- [ ] Any header includes go "upwards" in architecture

### ✅ Valid dependency directions:
```
Downward only: Runtime → Modules → Module Internals
Never upward: Modules → Runtime
Never lateral: Module → Module
```

---

## 🧩 MODULE ISOLATION TEST

### Each module must pass these tests:

#### Compilation Independence
```bash
# Test: Can module compile independently?
cd kern/runtime/modules/ecs
# Should compile with only module.h and runtime public interfaces
```

#### Removal Test
```bash
# Test: Can module be removed without breaking runtime?
# Remove ECS module entirely and verify runtime still compiles
```

#### No Cross-Module References
```bash
# Test: Module has no knowledge of other modules
grep -r "g3d" kern/runtime/modules/ecs/  # Should be empty
grep -r "ecs" kern/runtime/modules/g3d/   # Should be empty
```

### ❌ FAIL if:
- [ ] ECS calls graphics directly
- [ ] Physics touches renderer
- [ ] Any module references another module directly
- [ ] Module cannot compile independently
- [ ] Module removal breaks runtime core

### ✅ Module Isolation Requirements:
- [ ] Self-contained headers
- [ ] No cross-module includes
- [ ] Runtime interface only
- [ ] Clean removal possible

---

## ⚙️ VM INTEGRITY TEST

### VM must work standalone:

#### Test VM-Only Execution
```cpp
// Test: VM runs with ZERO modules loaded
KernRuntime runtime;
// Should work without any runtime.loadModule() calls
runtime.executeScript("print('Hello VM')");
```

#### Test VM Bytecode Execution
```cpp
// Test: VM executes basic bytecode
CodeObject code = compile("print('test')");
VM::Result result = runtime.getVM().execute(code);
// Should succeed without any modules
```

### ❌ FAIL if VM depends on:
- [ ] Entity systems
- [ ] Transform systems  
- [ ] Rendering systems
- [ ] Any module-specific functionality

### ✅ VM Independence Requirements:
- [ ] VM compiles without modules
- [ ] VM executes basic scripts
- [ ] VM has no module dependencies
- [ ] VM works with empty module registry

---

## 🔌 BINDING LAYER CHECK

### Native bindings must be:

#### ✅ Allowed:
- [ ] Function registration (`registerFunction("ecs.create", fn)`)
- [ ] Data marshaling (VM Value ↔ C++ types)
- [ ] Safe type conversion
- [ ] Module interface exposure only

#### ❌ NOT allowed:
- [ ] Direct access to ECS internals
- [ ] Direct world manipulation
- [ ] Module-specific logic inside bindings
- [ ] Bypassing module interface

### Validation Examples:
```cpp
// ✅ CORRECT (Safe binding)
bindings.registerFunction("ecs.createEntity", 
    [](VM& vm) { 
        return ecsModule->createEntity(); 
    });

// ❌ FORBIDDEN (Direct access)
bindings.registerFunction("ecs.createEntity", 
    [](VM& vm) { 
        return entitySystem.create();  // Bypasses module
    });
```

---

## 🧱 RUNTIME OWNERSHIP RULE

### ONLY runtime owns:
- [ ] Module lifecycle (load/unload)
- [ ] Scheduler (frame/tick loop)
- [ ] VM instance
- [ ] Module registry

### ❌ NOTHING else allowed:
- [ ] No entity ownership (belongs to ECS module)
- [ ] No scene ownership (belongs to graphics module)
- [ ] No rendering state ownership (belongs to graphics module)
- [ ] No transform ownership (belongs to ECS module)

### Ownership Validation:
```cpp
// ✅ CORRECT (Runtime owns modules)
runtime.loadModule("ecs");  // Runtime controls lifecycle

// ❌ FORBIDDEN (Runtime owns entities)
runtime.createEntity();    // Should be ecsModule.createEntity()
```

---

## 🔥 ESCALATION TEST (CRITICAL)

### Simulate Complete Module Removal:

#### Test 1: Remove ECS Module
```bash
# Remove ECS module completely
rm -rf kern/runtime/modules/ecs/
# Remove ECS module registration
# Build and test
```

**System must:**
- [ ] Still compile
- [ ] Still run VM
- [ ] Still load other modules
- [ ] Not crash runtime core

#### Test 2: Remove All Engine Modules
```bash
# Remove ECS, g3d, g2d modules
# Keep only core runtime
```

**System must:**
- [ ] VM still works
- [ ] Runtime still starts
- [ ] Scheduler still runs
- [ ] No core functionality lost

### ❌ FAIL if:
- [ ] Core runtime breaks without modules
- [ ] VM fails without engine modules
- [ ] Runtime becomes unusable
- [ ] Hidden dependencies revealed

---

## 📊 VALIDATION RESULTS TRACKING

### Test Results Matrix:
| Test | Status | Date | Notes |
|------|--------|------|-------|
| Core Rule Violation | ❌ FAIL | | Runtime includes ECS headers |
| Circular Dependency | ✅ PASS | | Clean dependency direction |
| Module Isolation | ❌ FAIL | | ECS calls graphics directly |
| VM Integrity | ✅ PASS | | VM works standalone |
| Binding Layer | ❌ FAIL | | Direct ECS access in bindings |
| Runtime Ownership | ✅ PASS | | Correct ownership boundaries |
| Escalation Test | ❌ FAIL | | Runtime breaks without ECS |

### Pass/Fail Criteria:
- **✅ PASS**: All validation checks passed
- **❌ FAIL**: Any validation check failed
- **⚠️ WARN**: Minor issues that should be addressed

---

## 🚨 FAILURE RESPONSE PROCEDURE

### If ANY test fails:

1. **STOP** - Do not proceed with further development
2. **IDENTIFY** - Locate the violating code
3. **FIX** - Refactor to remove dependency leak
4. **RETEST** - Run full checklist again
5. **DOCUMENT** - Update ARCHITECTURE_FINAL_STATE.md if needed

### Common Failure Patterns:
- **Module Creep**: Engine code slowly moves back into runtime
- **Binding Bypass**: Direct module access through bindings
- **Interface Erosion**: Runtime starts knowing module internals
- **VM Dependencies**: VM becomes engine-dependent

---

## 🧪 AUTOMATED VALIDATION (Future Enhancement)

### Potential automated checks:
```bash
# Dependency graph analysis
./tools/check_dependencies.sh

# Include analysis
./tools/check_includes.sh

# Module isolation test
./tools/test_module_isolation.sh
```

### Continuous Integration Integration:
- Run checklist on every PR
- Fail build if any check fails
- Automated dependency graph validation

---

## 🎯 SUCCESS METRICS

### Architecture Health Indicators:
- **Zero circular dependencies**
- **Modules compile independently**  
- **VM works standalone**
- **Clean removal possible**
- **No cross-module references**

### Long-term Stability:
- **No architecture regression**
- **Module system remains optional**
- **Core stays minimal**
- **Dependencies stay one-way**

---

## 📋 CHECKLIST EXECUTION LOG

### Last Run: [Date]
- Core Rule Violation: ❌ FAILED
- Circular Dependency: ✅ PASSED  
- Module Isolation: ❌ FAILED
- VM Integrity: ✅ PASSED
- Binding Layer: ❌ FAILED
- Runtime Ownership: ✅ PASSED
- Escalation Test: ❌ FAILED

### Issues Found:
1. Runtime includes ECS headers in runtime.cpp
2. ECS module references graphics directly
3. Bindings bypass module interface

### Actions Required:
1. Remove ECS includes from runtime core
2. Decouple ECS from graphics
3. Fix binding layer to use module interface

---

**This checklist is the authoritative validation gate for Kern's modular architecture. Any failure indicates a regression from the ARCHITECTURE_FINAL_STATE.md specification.**
