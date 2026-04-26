# Kern Evolution: From Star Defender Pain Points
## Targeted Implementation Plan (Minimal but Effective)

**Status**: Based on real code analysis of `star_defender.kn`
**Goal**: Fix the 6 pain points that make game code painful

---

## Pain Point Analysis from Real Code

### ❌ Issue 1: Named Arguments Not Supported
```kern
// CURRENT (BROKEN - causes PARSE-SYNTAX error)
let star = Entity(
    model: "sphere",
    position: (...),
    color: (...)
)

// WORKAROUND (Ugly - positional only)
let star = Entity("sphere", (...), (...), 1)
```

### ❌ Issue 2: Dictionaries Instead of Structs
```kern
// CURRENT (Fragile, no type safety)
enemies.append({entity: enemy, health: 10, active: true})
// Later: e.health  (no autocomplete, typo = silent bug)
```

### ❌ Issue 3: Too Many Global Position Variables
```kern
// CURRENT (Scattered state)
let playerX = 0
let playerY = 0
let playerZ = -10
// Later: player.position = (playerX, playerY, playerZ)  (verbose)
```

### ❌ Issue 4: Manual Index-Based Loops
```kern
// CURRENT (Verbose, error-prone)
let i = 0
while (i < len(bullets)) {
    let b = bullets[i]
    // ...
    i = i + 1
}
```

### ❌ Issue 5: Manual Collision Math
```kern
// CURRENT (Manhattan distance, awkward)
let bz = b.entity.position.z
let bx = b.entity.position.x
let dx = bx - ex
let dz = bz - ez
let dist = abs(dx) + abs(dz)
```

### ❌ Issue 6: Missing Random Function
```kern
// CURRENT (BROKEN - randomRange doesn't exist)
let x = randomRange(-WORLD_BOUNDS, WORLD_BOUNDS)

// SHOULD BE
let x = rnd.random_int(-WORLD_BOUNDS, WORLD_BOUNDS)
```

---

## Solution 1: Named Arguments (Quick Win)

**BEFORE:**
```kern
let star = Entity("sphere", (x, y, z), (r, g, b), 1)  // What is 1??
```

**AFTER:**
```kern
let star = Entity(
    model: "sphere",
    position: Vec3(x, y, z),
    color: Color(r, g, b),
    scale: 1
)
```

### Implementation: Parser Only

**File**: `kern/core/compiler/parser.cpp`

```cpp
// Add to CallExpr parsing (around line ~800)
// CURRENT: expects only expressions in argument list

// NEW: Support name: value syntax
struct NamedArg {
    std::string name;
    ExprPtr value;
};

// In finishCall() or similar:
if (match(TokenType::IDENTIFIER) && peekNext().type == TokenType::COLON) {
    // Named argument: name: value
    std::string name = consume(TokenType::IDENTIFIER).lexeme;
    consume(TokenType::COLON);
    ExprPtr value = expression();
    args.push_back(NamedArg{name, value});
} else {
    // Positional argument
    args.push_back(expression());
}
```

**AST Change** (`ast.hpp`):
```cpp
struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<CallArg> args;
};

struct CallArg {
    std::optional<std::string> name;  // NEW: null for positional
    ExprPtr value;
};
```

**Bytecode**: No change! Named args are just reordered at compile time.

**Codegen** (`codegen.cpp`):
```cpp
void CodeGenerator::emitCall(CallExpr* call) {
    // Get function signature to know parameter order
    auto* func = resolveFunction(call->callee);
    std::vector<std::string> paramNames = func->getParamNames();
    
    // Reorder named args to match parameter order
    std::vector<ExprPtr> orderedArgs(paramNames.size());
    
    for (size_t i = 0; i < call->args.size(); i++) {
        auto& arg = call->args[i];
        if (arg.name) {
            // Named arg - find position
            auto it = std::find(paramNames.begin(), paramNames.end(), *arg.name);
            if (it == paramNames.end()) {
                error("Unknown parameter: " + *arg.name);
            }
            size_t pos = it - paramNames.begin();
            orderedArgs[pos] = arg.value;
        } else {
            // Positional arg
            if (i < orderedArgs.size()) {
                orderedArgs[i] = arg.value;
            }
        }
    }
    
    // Emit args in correct order
    for (auto& arg : orderedArgs) {
        generate(arg);
    }
    
    emit(Opcode::CALL, func->id);
}
```

**Timeline**: 2 days
**Risk**: Low (parser-only, no VM changes)

---

## Solution 2: Vec3 Type + Vector Math

**BEFORE:**
```kern
let playerX = 0
let playerY = 0
let playerZ = -10
// ...
player.position = (playerX, playerY, playerZ)

// Collision
let dx = bx - ex
let dz = bz - ez
let dist = abs(dx) + abs(dz)
```

**AFTER:**
```kern
let playerPos = Vec3(0, 0, -10)
// ...
player.position = playerPos

// Collision
let dist = (bullet.pos - enemy.pos).length()
```

### Implementation: Built-in VM Type

**Value Changes** (`value.hpp`):
```cpp
// Add to Type enum
enum class Type { NIL, BOOL, INT, FLOAT, STRING, ARRAY, MAP, 
                  FUNCTION, CLASS, INSTANCE, GENERATOR, PTR,
                  VEC2, VEC3 };  // NEW

// Vec3 storage
struct Vec3Data {
    float x, y, z;
};

// Update variant
std::variant<
    // ... existing ...
    Vec2Data,  // NEW
    Vec3Data   // NEW
> data;
```

**Bytecode** (`bytecode.hpp`):
```cpp
enum class Opcode : uint8_t {
    // ... existing ...
    BUILD_VEC2,      // operand: constant pool index for (x, y)
    BUILD_VEC3,      // operand: constant pool index for (x, y, z)
    VEC_ADD,         // pop 2 vec3, push sum
    VEC_SUB,
    VEC_MUL,         // vec * scalar
    VEC_DIV,
    VEC_DOT,
    VEC_CROSS,
    VEC_LENGTH,
    VEC_NORMALIZE,
    VEC_GET_X,       // extract component
    VEC_GET_Y,
    VEC_GET_Z,
    VEC_SET_X,       // set component (for mut)
};
```

**Parser Changes**:
```cpp
// Allow Vec3 literal syntax
Vec3(x, y, z)     // Constructor call
Vec3{x, y, z}     // Literal (if we add this)
```

**VM Implementation** (`vm_unboxed.hpp`):
```cpp
case Opcode::VEC_ADD: {
    Value b = pop();
    Value a = pop();
    // Both are unboxed or tagged pointers
    Vec3Data* va = a.asVec3();
    Vec3Data* vb = b.asVec3();
    Value result = Value::makeVec3(
        va->x + vb->x,
        va->y + vb->y,
        va->z + vb->z
    );
    push(result);
    break;
}

case Opcode::VEC_LENGTH: {
    Value v = pop();
    Vec3Data* vv = v.asVec3();
    float len = sqrt(vv->x*vv->x + vv->y*vv->y + vv->z*vv->z);
    push(Value::makeFloat(len));
    break;
}
```

**Syntax Support**:
```kern
// Construction
let v = Vec3(1, 2, 3)
let v2 = Vec3{x: 1, y: 2, z: 3}  // With named args

// Operations
let sum = v1 + v2
let diff = v1 - v2
let scaled = v * 2.0
let len = v.length()
let normalized = v.normalize()
let dot = v1.dot(v2)

// Component access
let x = v.x
let y = v.y
v.x = 10  // mutable

// Swizzle (optional)
let xy = v.xy  // Vec2
```

**Timeline**: 1 week
**Risk**: Medium (VM changes, but localized)

---

## Solution 3: Structs (Big Win)

**BEFORE:**
```kern
enemies.append({entity: enemy, health: 10, active: true})
// ...
let b = bullets[i]
if (!b.active) { continue }  // Typo: b.actve would silently return nil!
```

**AFTER:**
```kern
struct Bullet {
    entity: Entity,
    active: bool,
    speed: float
}

enemies.append(Enemy{entity: enemy, health: 10, active: true})
// ...
let b = bullets[i]
if (!b.active) { continue }  // Typo caught at compile time!
```

### Implementation

**Parser** (`parser.cpp`):
```cpp
// New declaration type
if (match(TokenType::STRUCT)) {
    return structDeclaration();
}

StructDeclStmt* structDeclaration() {
    Token name = consume(TokenType::IDENTIFIER, "Expected struct name");
    consume(TokenType::LBRACE);
    
    std::vector<FieldDecl> fields;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token fieldName = consume(TokenType::IDENTIFIER);
        consume(TokenType::COLON);
        TypeExpr type = parseType();
        fields.push_back({fieldName.lexeme, type});
        
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RBRACE);
    
    return new StructDeclStmt(name.lexeme, fields);
}

// Struct literal expression
Expr* structLiteral() {
    // TypeName { field: value, ... }
    Token typeName = consume(TokenType::IDENTIFIER);
    consume(TokenType::LBRACE);
    
    std::vector<StructFieldInit> inits;
    while (!check(TokenType::RBRACE)) {
        Token field = consume(TokenType::IDENTIFIER);
        consume(TokenType::COLON);
        ExprPtr value = expression();
        inits.push_back({field.lexeme, value});
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RBRACE);
    
    return new StructLiteralExpr(typeName.lexeme, inits);
}
```

**AST** (`ast.hpp`):
```cpp
struct StructDeclStmt : Stmt {
    std::string name;
    std::vector<FieldDecl> fields;
};

struct FieldDecl {
    std::string name;
    TypeExpr type;
    std::optional<ExprPtr> defaultValue;
};

struct StructLiteralExpr : Expr {
    std::string structName;
    std::vector<std::pair<std::string, ExprPtr>> fieldValues;
};

struct FieldAccessExpr : Expr {
    ExprPtr object;
    std::string fieldName;
};

struct FieldAssignExpr : Expr {
    ExprPtr object;
    std::string fieldName;
    ExprPtr value;
};
```

**Bytecode**:
```cpp
enum class Opcode : uint8_t {
    // ... existing ...
    BUILD_STRUCT,      // operand: struct_type_id
    GET_FIELD,         // operand: field_index
    SET_FIELD,         // operand: field_index
};
```

**VM** - Struct Type Registry:
```cpp
struct StructType {
    std::string name;
    size_t id;
    std::vector<FieldInfo> fields;
    size_t size;  // total bytes for inline storage
};

struct StructValue {
    uint32_t typeId;
    // Fields stored inline after header
    // Memory layout: [typeId][field0][field1]...
};

class VM {
    std::unordered_map<std::string, StructType> structTypes;
    std::vector<StructType*> structTypeById;
    
    void registerStruct(const StructType& type);
    const StructType* getStructType(const std::string& name);
    const StructType* getStructType(uint32_t id);
};
```

**Memory Layout**:
```
StructValue for Enemy { entity, health, active }

[ 4 bytes: type_id = 5 ]
[ 8 bytes: entity (ptr to Entity) ]
[ 4 bytes: health (int) ]
[ 1 byte:  active (bool) + 3 padding ]
[ total: 20 bytes, inline on stack ]
```

**Codegen**:
```cpp
void CodeGenerator::visit(StructLiteralExpr* expr) {
    const StructType* type = vm.getStructType(expr->structName);
    
    // Emit field values in declaration order
    for (const auto& field : type->fields) {
        auto it = expr->fieldValues.find(field.name);
        if (it != expr->fieldValues.end()) {
            generate(it->second);
        } else if (field.defaultValue) {
            generate(field.defaultValue);
        } else {
            error("Missing field: " + field.name);
        }
    }
    
    emit(Opcode::BUILD_STRUCT, type->id);
}

void CodeGenerator::visit(FieldAccessExpr* expr) {
    generate(expr->object);
    const StructType* type = getType(expr->object);
    int fieldIndex = type->getFieldIndex(expr->fieldName);
    emit(Opcode::GET_FIELD, fieldIndex);
}
```

**Usage in Game**:
```kern
// types.kn
struct Transform {
    position: Vec3,
    rotation: Vec3,
    scale: Vec3
}

struct Bullet {
    entity: Entity,
    transform: Transform,
    active: bool,
    speed: float
}

struct Enemy {
    entity: Entity,
    health: int,
    active: bool,
    speed: float
}

// main.kn
import "types"

fn spawnEnemy() {
    let enemy = Entity(
        model: "cube",
        position: Vec3(x, 0, 30),
        color: Color(255, 80, 80),
        scale: 1
    )
    
    enemies.append(Enemy{
        entity: enemy,
        health: 10,
        active: true,
        speed: ENEMY_SPEED
    })
}

fn updateBullets() {
    for b in bullets {
        if !b.active { continue }
        
        // Clean field access
        b.transform.position.z = b.transform.position.z + b.speed
        
        // Check bounds
        if b.transform.position.z > 40 {
            b.entity.hide()
            b.active = false
        }
    }
}
```

**Timeline**: 2 weeks
**Risk**: Medium (new VM type, but similar to existing class system)

---

## Solution 4: For-Each Loops (Already Partially Working!)

Looking at line 247-255, `for star in stars` already works! But we need it for manual iteration too.

**BEFORE:**
```kern
let i = 0
while (i < len(bullets)) {
    let b = bullets[i]
    if (!b.active) {
        i = i + 1
        continue
    }
    // ...
    i = i + 1
}
```

**AFTER:**
```kern
for b in bullets {
    if !b.active { continue }
    // ...
}
```

**BUT**: The game already uses `for star in stars` successfully! So the basic for-in works. The issue is when you need index access or mutation.

### Enhanced For-Loop

```kern
// Current working
for star in stars { }

// Add index
for i, star in stars { }  // i = index, star = element

// Add mutation support
for mut b in bullets {
    b.active = false  // modifies array element
}

// With filter (optional)
for b in bullets where b.active {
    // only active bullets
}
```

### Implementation

**Parser**:
```cpp
// Current for-in
for (IDENTIFIER in EXPRESSION) { ... }

// Add index form
for (IDENTIFIER, IDENTIFIER in EXPRESSION) { ... }
```

**Bytecode**:
```cpp
enum class Opcode : uint8_t {
    // ... existing ...
    FOR_IN_INIT,     // setup iterator, push iterator state
    FOR_IN_NEXT,     // advance iterator, push (has_next, value)
    FOR_IN_DONE,     // cleanup
};
```

**VM Iterator Protocol**:
```cpp
// Any type with these methods works:
// - hasNext() -> bool
// - next() -> T

class ArrayIterator {
    ArrayObject* array;
    size_t index;
    
    bool hasNext() { return index < array->size(); }
    Value next() { return (*array)[index++]; }
};
```

**Timeline**: 3 days
**Risk**: Low (mostly already working)

---

## Solution 5: Collision / Math Utilities

**BEFORE:**
```kern
let dx = bx - ex
let dz = bz - ez
let dist = abs(dx) + abs(dz)  // Manhattan (wrong!)
if (dist < 3) { /* hit */ }
```

**AFTER:**
```kern
let dist = (bullet.pos - enemy.pos).length()
if dist < COLLISION_RADIUS { /* hit */ }

// Or with bounding boxes
if bullet.bounds.intersects(enemy.bounds) { /* hit */ }
```

### Implementation: Built-in Functions

Add to stdlib without language changes:

```rust
// math.kn (stdlib module)
struct Vec3 {
    x: float,
    y: float,
    z: float
}

// These become builtin VM functions
fn vec3(x: float, y: float, z: float) -> Vec3
fn vec3_add(a: Vec3, b: Vec3) -> Vec3
fn vec3_sub(a: Vec3, b: Vec3) -> Vec3
fn vec3_mul(v: Vec3, s: float) -> Vec3
fn vec3_length(v: Vec3) -> float
fn vec3_distance(a: Vec3, b: Vec3) -> float
fn vec3_normalize(v: Vec3) -> Vec3
fn vec3_dot(a: Vec3, b: Vec3) -> float

// Collision helpers
fn sphere_intersects(center1: Vec3, r1: float, center2: Vec3, r2: float) -> bool
fn box_intersects(min1: Vec3, max1: Vec3, min2: Vec3, max2: Vec3) -> bool
```

**In Game**:
```kern
import "math"

fn checkCollisions() {
    for b in bullets where b.active {
        for e in enemies where e.active {
            let dist = math.vec3_distance(b.pos, e.pos)
            if dist < 3.0 {
                // Hit!
                e.health -= 10
                b.active = false
            }
        }
    }
}
```

**Timeline**: 3 days (stdlib only)
**Risk**: Very low

---

## Solution 6: Fix Random Function

**BEFORE (BROKEN)**:
```kern
let x = randomRange(-WORLD_BOUNDS, WORLD_BOUNDS)  // ERROR: undefined
```

**AFTER**:
```kern
let x = rnd.random_int(-WORLD_BOUNDS, WORLD_BOUNDS)  // Use existing
// Or add alias:
let x = random_range(-WORLD_BOUNDS, WORLD_BOUNDS)
```

### Quick Fix: Add Alias

In `random` module:
```rust
// Alias for convenience
fn random_range(min: int, max: int) -> int {
    return random_int(min, max)
}
```

**Timeline**: 10 minutes
**Risk**: None

---

## Combined Impact: Before vs After

### BEFORE (Current Kern - 307 lines, fragile)
```kern
use g3d
import "random" as rnd

let playerX = 0
let playerY = 0
let playerZ = -10
let bullets = []
let enemies = []

def spawnEnemy() {
    let x = rnd.random_int(-WORLD_BOUNDS, WORLD_BOUNDS)
    let enemy = Entity("cube", (x, 0, 30), (255, 80, 80), 2)
    enemies.append({entity: enemy, health: 10, active: true})
}

def updateBullets() {
    let i = 0
    while (i < len(bullets)) {
        let b = bullets[i]
        if (!b.active) { i = i + 1; continue }
        
        let pos = b.entity.position
        pos.z = pos.z + BULLET_SPEED / 60
        b.entity.position = pos
        
        if (pos.z > 40) {
            b.entity.hide()
            b.active = false
        }
        i = i + 1
    }
}

def checkCollisions() {
    let bi = 0
    while (bi < len(bullets)) {
        let b = bullets[bi]
        if (!b.active) { bi = bi + 1; continue }
        
        let bz = b.entity.position.z
        let bx = b.entity.position.x
        let ei = 0
        
        while (ei < len(enemies)) {
            let e = enemies[ei]
            if (!e.active) { ei = ei + 1; continue }
            
            let ez = e.entity.position.z
            let ex = e.entity.position.x
            let dx = bx - ex
            let dz = bz - ez
            let dist = abs(dx) + abs(dz)
            
            if (dist < 3) {
                e.health = e.health - 10
                if (e.health <= 0) {
                    e.entity.hide()
                    e.active = false
                    score = score + 100
                }
                b.entity.hide()
                b.active = false
                break
            }
            ei = ei + 1
        }
        bi = bi + 1
    }
}
```

### AFTER (Improved Kern - ~150 lines, type-safe)
```kern
use g3d
import "random" as rnd
import "math"

struct Bullet {
    entity: Entity,
    pos: Vec3,
    active: bool
}

struct Enemy {
    entity: Entity,
    pos: Vec3,
    health: int,
    active: bool
}

let bullets: Array<Bullet> = []
let enemies: Array<Enemy> = []
let playerPos = Vec3(0, 0, -10)

fn spawnEnemy() {
    let x = rnd.random_range(-WORLD_BOUNDS, WORLD_BOUNDS)
    let enemy = Entity(
        model: "cube",
        position: Vec3(x, 0, 30),
        color: Color(255, 80, 80),
        scale: 2
    )
    enemies.append(Enemy{
        entity: enemy,
        pos: Vec3(x, 0, 30),
        health: 10,
        active: true
    })
}

fn updateBullets() {
    for mut b in bullets {
        if !b.active { continue }
        
        b.pos.z += BULLET_SPEED / 60
        b.entity.position = b.pos
        
        if b.pos.z > 40 {
            b.entity.hide()
            b.active = false
        }
    }
}

fn checkCollisions() {
    for b in bullets where b.active {
        for e in enemies where e.active {
            let dist = (b.pos - e.pos).length()
            
            if dist < COLLISION_RADIUS {
                e.health -= 10
                b.active = false
                
                if e.health <= 0 {
                    e.entity.hide()
                    e.active = false
                    score += 100
                }
                break
            }
        }
    }
}
```

**Improvements**:
- **Lines**: 307 → ~150 (50% reduction)
- **Type safety**: Structs prevent typos like `b.actve`
- **Readability**: Named args, vector math, clean loops
- **Maintainability**: Clear data model, reusable types

---

## Implementation Priority

### Week 1: Quick Wins
1. ✅ Fix `random_range` alias (10 min)
2. 🔄 Named arguments (2 days) - **UNBLOCKS readable Entity creation**
3. 🔄 Enhanced for-loops (3 days) - **CLEANS UP all the manual while loops**

### Week 2-3: Core Types
4. 🔄 Vec3 type (1 week) - **ELIMINATES scattered X/Y/Z globals**
5. 🔄 Math utilities (3 days) - **PROPER collision detection**

### Week 4-5: Big Feature
6. 🔄 Structs (2 weeks) - **TYPE SAFETY, CLEAN DATA MODEL**

### Total Timeline: 5 weeks

**Risk**: Low-to-medium, all incremental changes
**Testing**: Rewrite star_defender.kn after each feature
**Success Metric**: Game code is 50% shorter and type-safe

---

## Key Insight

This isn't about adding "cool features" - it's about removing specific friction that makes game code painful. Each feature directly fixes a pain point from real code:

| Pain Point | Solution | Code Impact |
|------------|----------|-------------|
| `Entity("sphere", (...), (...), 1)` | Named args | Readable constructors |
| `{entity: e, health: 10}` | Structs | Type safety |
| `playerX, playerY, playerZ` | Vec3 | Single variable |
| `while (i < len(...))` | For-each | Clean loops |
| `abs(dx) + abs(dz)` | Vec3.length() | Proper math |

That's how you evolve a language: **from working code → clean code**.
