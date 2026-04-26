# Kern Lean Implementation Roadmap
## Reordered by Pain → Solution → Implementation

**Principle**: Every feature must directly fix code from `star_defender.kn`
**Timeline**: 8-10 weeks to production-ready game code

---

## Phase 1: Immediate Pain Relief (Weeks 1-2)

### 1.1 Named Arguments ⭐ CRITICAL
**Pain**: `Entity("sphere", (...), (...), 1)` is unreadable
**Solution**: `Entity(model: "sphere", position: (...), color: (...), scale: 1)`

**Implementation** (Parser only, 2 days):

```cpp
// parser.cpp - In argument list parsing
std::vector<CallArg> parseArguments() {
    std::vector<CallArg> args;
    
    while (!check(TokenType::RPAREN)) {
        if (match(TokenType::IDENTIFIER) && peek().type == TokenType::COLON) {
            // Named arg: name: value
            std::string name = previous().lexeme;
            consume(TokenType::COLON);
            ExprPtr value = expression();
            args.push_back({name, value});
        } else {
            // Positional - backtrack and parse as expression
            // (need 1-token lookahead to disambiguate)
            ExprPtr value = expression();
            args.push_back({std::nullopt, value});
        }
        
        if (!match(TokenType::COMMA)) break;
    }
    
    return args;
}
```

**AST Change**:
```cpp
struct CallArg {
    std::optional<std::string> name;  // null = positional
    ExprPtr value;
};
```

**Codegen** (reorder to match function signature):
```cpp
void emitCall(CallExpr* call) {
    auto func = resolveFunction(call->callee);
    auto params = func->params;
    
    // Map named args to positions
    std::vector<ExprPtr> ordered(params.size(), nullptr);
    
    for (size_t i = 0; i < call->args.size(); i++) {
        auto& arg = call->args[i];
        size_t pos;
        
        if (arg.name) {
            pos = findParamIndex(params, *arg.name);
            if (pos == SIZE_MAX) error("Unknown parameter: " + *arg.name);
        } else {
            pos = i;
        }
        
        ordered[pos] = arg.value;
    }
    
    // Check defaults filled
    for (size_t i = 0; i < ordered.size(); i++) {
        if (!ordered[i]) {
            if (params[i].defaultValue) {
                ordered[i] = params[i].defaultValue;
            } else {
                error("Missing argument for parameter: " + params[i].name);
            }
        }
    }
    
    // Emit in order
    for (auto& expr : ordered) {
        generate(expr);
    }
    emit(Opcode::CALL, func->id);
}
```

**Test**:
```kern
def foo(a: int, b: int, c: int) { print(a + b + c) }

foo(1, 2, 3)              // positional
foo(a: 1, b: 2, c: 3)     // named
foo(c: 3, a: 1, b: 2)     // reordered
foo(1, c: 3, b: 2)        // mixed
```

---

### 1.2 Fix Random Function (10 minutes)
**Pain**: `randomRange()` doesn't exist

**Solution** - Add to `random` module:
```cpp
// In random module registration
add("random_range", [](VM* vm, std::vector<ValuePtr> args) -> Value {
    int min = args[0]->asInt();
    int max = args[1]->asInt();
    return vm->makeInt(vm->rng.randomInt(min, max));
});
```

---

### 1.3 Enhanced For-Each (3 days)
**Pain**: `while (i < len(bullets))` everywhere

**Current working**: `for star in stars` (line 247)
**Enhancement**: Add index, mutation, filter

**Parser**:
```cpp
// Current: for IDENTIFIER in EXPRESSION BLOCK
// Add:
//   for i, item in list { }     // with index
//   for mut item in list { }    // mutable reference
//   for item in list where cond  // filter

Stmt* forStatement() {
    consume(TokenType::FOR);
    
    std::optional<std::string> indexName;
    std::string itemName;
    bool isMutable = false;
    
    // Try: for i, x in ...
    Token first = consume(TokenType::IDENTIFIER);
    if (match(TokenType::COMMA)) {
        indexName = first.lexeme;
        itemName = consume(TokenType::IDENTIFIER).lexeme;
    } else {
        itemName = first.lexeme;
    }
    
    // Check for mut
    if (itemName == "mut") {
        isMutable = true;
        itemName = consume(TokenType::IDENTIFIER).lexeme;
    }
    
    consume(TokenType::IN);
    ExprPtr iterable = expression();
    
    // Optional where clause
    std::optional<ExprPtr> filter;
    if (match(TokenType::WHERE)) {
        filter = expression();
    }
    
    StmtPtr body = statement();
    
    return new ForInStmt(indexName, itemName, isMutable, iterable, filter, body);
}
```

**Bytecode**:
```cpp
enum class Opcode : uint8_t {
    // ... existing ...
    FOR_IN_INIT,       // setup iterator (pushes iterator state)
    FOR_IN_NEXT,       // pushes (has_next, index, value) or jumps
    FOR_IN_DONE,       // cleanup iterator
};
```

**VM Iterator State** (stack-based):
```cpp
struct IteratorState {
    Value collection;  // array, map, or custom iterator
    size_t index;
    // For custom: function ptr to next()
};

// In VM execution:
case FOR_IN_INIT: {
    Value collection = pop();
    // Push iterator state as hidden local
    locals[localCount++] = createIterator(collection);
    break;
}

case FOR_IN_NEXT: {
    IteratorState* it = locals[iteratorLocal].asIterator();
    
    if (hasNext(it)) {
        auto [index, value] = next(it);
        push(value);          // loop variable
        if (hasIndexVar) push(index);
    } else {
        ip = jumpTarget;      // exit loop
    }
    break;
}
```

**Desugaring** (simpler approach - transform to while):
```cpp
// for item in list where filter { body }
// →
// let __iter = list.iterator()
// while __iter.hasNext() {
//     let item = __iter.next()
//     if !filter { continue }
//     body
// }
```

**Test**:
```kern
let arr = [1, 2, 3, 4, 5]

for x in arr {
    print(x)  // 1 2 3 4 5
}

for i, x in arr {
    print(i + ": " + x)  // 0:1 1:2 ...
}

for mut x in arr {
    x = x * 2  // can modify
}

for x in arr where x > 2 {
    print(x)  // 3 4 5
}
```

---

## Phase 2: Core Types (Weeks 3-4)

### 2.1 Vec3 Type ⭐ CRITICAL
**Pain**: `playerX, playerY, playerZ` scattered globals

**Design**: Built-in VM type, unboxed when possible

**Value Changes**:
```cpp
// value.hpp
enum class Type { 
    // ... existing ...
    VEC2, VEC3 
};

struct Vec3Data {
    float x, y, z;
};

// Variant storage
std::variant<
    // ... existing ...
    Vec3Data
> data;
```

**Bytecode** (10 new opcodes):
```cpp
enum class Opcode : uint8_t {
    BUILD_VEC3,        // (f32, f32, f32) -> Vec3
    VEC3_GET_X,        // Vec3 -> f32
    VEC3_GET_Y,
    VEC3_GET_Z,
    VEC3_SET_X,        // (Vec3, f32) -> Vec3
    VEC3_SET_Y,
    VEC3_SET_Z,
    VEC3_ADD,          // (Vec3, Vec3) -> Vec3
    VEC3_SUB,
    VEC3_MUL,          // (Vec3, f32) -> Vec3
    VEC3_DIV,
    VEC3_DOT,          // (Vec3, Vec3) -> f32
    VEC3_LENGTH,       // Vec3 -> f32
    VEC3_NORMALIZE,    // Vec3 -> Vec3
    VEC3_DISTANCE,     // (Vec3, Vec3) -> f32
};
```

**VM Implementation**:
```cpp
case Opcode::BUILD_VEC3: {
    float z = popFloat();
    float y = popFloat();
    float x = popFloat();
    push(Value::makeVec3(x, y, z));
    break;
}

case Opcode::VEC3_ADD: {
    Vec3Data b = popVec3();
    Vec3Data a = popVec3();
    push(Value::makeVec3(a.x + b.x, a.y + b.y, a.z + b.z));
    break;
}

case Opcode::VEC3_LENGTH: {
    Vec3Data v = popVec3();
    float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    push(Value::makeFloat(len));
    break;
}
```

**Parser** (constructor syntax):
```cpp
// Vec3(x, y, z)  - treated as function call (can be builtin)
// v.x, v.y, v.z  - field access
// v + v2         - operator overloading (optional, can start with functions)

// Initially: just builtin function
let v = Vec3(1.0, 2.0, 3.0)
let len = Vec3_length(v)
let sum = Vec3_add(v1, v2)

// Later: operators
let v3 = v1 + v2
let len = v.length()
```

**Test**:
```kern
let pos = Vec3(0, 0, -10)
let vel = Vec3(1, 0, 0)
let newPos = Vec3_add(pos, vel)

// Distance check (collision)
let dist = Vec3_distance(bullet.pos, enemy.pos)
if dist < 3.0 {
    // hit
}
```

---

### 2.2 Structs (NO GENERICS) ⭐ CRITICAL
**Pain**: `{entity: e, health: 10}` dictionaries are fragile

**Design**: Value types, inline storage, no inheritance

**Parser**:
```cpp
Stmt* structDeclaration() {
    consume(TokenType::STRUCT);
    Token name = consume(TokenType::IDENTIFIER);
    consume(TokenType::LBRACE);
    
    std::vector<FieldDecl> fields;
    while (!check(TokenType::RBRACE)) {
        Token fieldName = consume(TokenType::IDENTIFIER);
        consume(TokenType::COLON);
        TypeExpr type = parseType();  // int, float, str, Vec3, OtherStruct
        
        std::optional<ExprPtr> defaultVal;
        if (match(TokenType::EQUAL)) {
            defaultVal = expression();
        }
        
        fields.push_back({fieldName.lexeme, type, defaultVal});
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RBRACE);
    
    return new StructDeclStmt(name.lexeme, fields);
}

Expr* structLiteral() {
    // TypeName { field: value, ... }
    Token typeName = consume(TokenType::IDENTIFIER);
    consume(TokenType::LBRACE);
    
    std::vector<std::pair<std::string, ExprPtr>> fields;
    while (!check(TokenType::RBRACE)) {
        Token name = consume(TokenType::IDENTIFIER);
        consume(TokenType::COLON);
        ExprPtr value = expression();
        fields.push_back({name.lexeme, value});
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RBRACE);
    
    return new StructLiteralExpr(typeName.lexeme, fields);
}
```

**AST**:
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
```

**Bytecode**:
```cpp
enum class Opcode : uint8_t {
    BUILD_STRUCT,      // operand: struct_type_id
    GET_FIELD,         // operand: field_index  
    SET_FIELD,         // operand: field_index
};
```

**VM - Simple Implementation First**:
```cpp
// Start simple: heap-allocated struct
struct StructValue {
    uint32_t typeId;
    std::vector<Value> fields;  // heap vector
};

// Later optimize to inline on stack

class VM {
    std::vector<StructType> structTypes;  // registry by ID
    
    void registerStruct(const StructType& type) {
        structTypes.push_back(type);
    }
};

case Opcode::BUILD_STRUCT: {
    uint32_t typeId = operand;
    const StructType& type = structTypes[typeId];
    
    StructValue* sv = allocate<StructValue>();
    sv->typeId = typeId;
    sv->fields.resize(type.fields.size());
    
    // Pop field values in reverse order
    for (int i = type.fields.size() - 1; i >= 0; i--) {
        sv->fields[i] = pop();
    }
    
    push(Value::makeStruct(sv));
    break;
}

case Opcode::GET_FIELD: {
    uint32_t fieldIndex = operand;
    StructValue* sv = pop().asStruct();
    push(sv->fields[fieldIndex]);
    break;
}
```

**Usage in Game**:
```kern
// types.kn
struct Bullet {
    entity: Entity,
    pos: Vec3,
    vel: Vec3,
    active: bool,
    damage: int = 10  // default value
}

struct Enemy {
    entity: Entity,
    pos: Vec3,
    health: int,
    active: bool,
    speed: float
}

// main.kn
fn spawnEnemy() {
    let x = random_range(-WORLD_BOUNDS, WORLD_BOUNDS)
    let e = Entity(
        model: "cube",
        position: Vec3(x, 0, 30),
        color: Color(255, 80, 80),
        scale: 2
    )
    
    enemies.append(Enemy{
        entity: e,
        pos: Vec3(x, 0, 30),
        health: 10,
        active: true,
        speed: ENEMY_SPEED
    })
}

fn updateBullets() {
    for mut b in bullets {
        if !b.active { continue }
        
        // Type-safe field access
        b.pos = Vec3_add(b.pos, b.vel)
        b.entity.position = b.pos
        
        if b.pos.z > 40 {
            b.entity.hide()
            b.active = false
        }
    }
}
```

**Type Checking**:
```cpp
// Semantic analysis pass
void checkFieldAccess(FieldAccessExpr* expr) {
    Type objType = inferType(expr->object);
    
    if (!objType.isStruct()) {
        error("Cannot access field on non-struct type");
        return;
    }
    
    auto structType = getStruct(objType.name);
    if (!structType.hasField(expr->fieldName)) {
        error("Unknown field: " + expr->fieldName);
    }
    
    // Set expression type for downstream checks
    expr->type = structType.getFieldType(expr->fieldName);
}
```

---

## Phase 3: Modularity & Control (Weeks 5-7)

### 3.1 Module System (File-based)
**Pain**: Everything in one file

**Design**: Simple, no circular deps

**Resolution**:
```
import "math"           →  math.kn (or math/index.kn)
import "player"         →  player.kn
import "../common"      →  ../common.kn
from "math" import Vec3, sin, cos
```

**Parser**:
```cpp
Stmt* importStatement() {
    if (match(TokenType::IMPORT)) {
        std::string path = consume(TokenType::STRING).lexeme;
        std::optional<std::string> alias;
        if (match(TokenType::AS)) {
            alias = consume(TokenType::IDENTIFIER).lexeme;
        }
        return new ImportStmt(path, alias, {});  // full import
    }
    
    if (match(TokenType::FROM)) {
        std::string path = consume(TokenType::STRING).lexeme;
        consume(TokenType::IMPORT);
        std::vector<ImportItem> items;
        
        do {
            std::string name = consume(TokenType::IDENTIFIER).lexeme;
            std::optional<std::string> alias;
            if (match(TokenType::AS)) {
                alias = consume(TokenType::IDENTIFIER).lexeme;
            }
            items.push_back({name, alias});
        } while (match(TokenType::COMMA));
        
        return new ImportStmt(path, std::nullopt, items);
    }
}
```

**Resolution Algorithm**:
```cpp
class ModuleResolver {
    std::unordered_map<std::string, std::shared_ptr<Module>> cache;
    
    std::shared_ptr<Module> resolve(const std::string& path, 
                                       const std::string& fromFile) {
        // 1. Resolve relative to fromFile
        std::string resolved = resolvePath(path, fromFile);
        
        // 2. Check cache
        if (cache.count(resolved)) return cache[resolved];
        
        // 3. Try file extensions
        std::vector<std::string> candidates = {
            resolved + ".kn",
            resolved + "/index.kn"
        };
        
        for (auto& cand : candidates) {
            if (fileExists(cand)) {
                return load(cand);
            }
        }
        
        error("Module not found: " + path);
    }
    
    std::shared_ptr<Module> load(const std::string& file) {
        auto mod = std::make_shared<Module>();
        mod->path = file;
        mod->source = readFile(file);
        mod->ast = parse(mod->source);
        
        // Recursive load imports
        for (auto* import : mod->ast->imports) {
            import->module = resolve(import->path, file);
        }
        
        cache[file] = mod;
        return mod;
    }
};
```

**Codegen** (name mangling):
```cpp
std::string mangle(const std::string& module, const std::string& symbol) {
    return module + "::" + symbol;
}

// math::Vec3_add  →  "math_Vec3_add"
```

**Visibility**:
```kern
// math.kn
pub fn sin(x: float) -> float { ... }  // exported

fn internal() { ... }  // private to module

pub struct Vec3 { ... }  // exported
```

---

### 3.2 Result Type + `?` Operator
**Pain**: Manual error checking is verbose

**Design**: Enum-like with sugar

**Parser**:
```cpp
// Result<T, E> is a special type (like Option)
// ? postfix operator for propagation

Expr* postfix() {
    ExprPtr expr = primary();
    
    while (true) {
        if (match(TokenType::QUESTION)) {
            expr = std::make_unique<TryExpr>(std::move(expr));
        } else if (match(TokenType::DOT)) {
            // field access
        } else if (match(TokenType::LBRACKET)) {
            // index
        } else {
            break;
        }
    }
    
    return expr;
}
```

**Desugaring**:
```cpp
// let x = foo()?  →
// let __result = foo()
// if __result.isErr() { return __result }
// let x = __result.unwrap()

void CodeGenerator::visit(TryExpr* expr) {
    // Generate the expression
    generate(expr->value);
    
    // Check if Err
    size_t elseJump = emitJump(Opcode::JUMP_IF_ERR);
    
    // Unwrap Ok value (already on stack)
    // ... fall through ...
    
    // Else: return the error
    patchJump(elseJump);
    emit(Opcode::RETURN);  // return the Err
}
```

**Result Methods** (built-in):
```kern
result.isOk() -> bool
result.isErr() -> bool
result.unwrap() -> T  (panics if Err)
result.unwrapOr(default: T) -> T
result.unwrapOrElse(fn: () -> T) -> T
result.map(fn: (T) -> U) -> Result<U, E>
```

**Usage**:
```kern
fn readFile(path: str) -> Result<str, IOError> {
    if !fileExists(path) {
        return Err(IOError::NotFound)
    }
    // ... read ...
    return Ok(content)
}

fn processFile(path: str) -> Result<Data, Error> {
    let content = readFile(path)?  // propagates Err
    let data = parse(content)?      // propagates Err  
    return Ok(transform(data))
}

// Or manual
let result = readFile("data.txt")
if result.isOk() {
    print(result.unwrap())
} else {
    print("Failed: " + result.unwrapErr())
}
```

---

### 3.3 Defer
**Pain**: Cleanup scattered everywhere

**Parser**:
```cpp
Stmt* deferStatement() {
    consume(TokenType::DEFER);
    
    if (match(TokenType::LBRACE)) {
        // defer { block }
        StmtPtr block = blockStatement();
        return new DeferStmt(block);
    } else {
        // defer expression
        ExprPtr expr = expression();
        return new DeferStmt(expr);
    }
}
```

**Codegen** (defer stack):
```cpp
class CodeGenerator {
    std::vector<std::vector<size_t>> deferStack;  // per scope
    
    void beginScope() {
        deferStack.push_back({});
    }
    
    void endScope() {
        // Emit all pending defers (LIFO)
        auto& defers = deferStack.back();
        for (auto it = defers.rbegin(); it != defers.rend(); ++it) {
            // Emit jump to deferred code
            emit(Opcode::CALL, *it);
        }
        deferStack.pop_back();
    }
    
    void visit(DeferStmt* stmt) {
        // Generate deferred code in separate section
        size_t deferLabel = emitJumpPlaceholder();
        
        // Generate the deferred code at end of function
        size_t deferCode = emitDeferred([&]() {
            generate(stmt->body);
        });
        
        patchJump(deferLabel, deferCode);
        deferStack.back().push_back(deferCode);
    }
    
    void visit(ReturnStmt* stmt) {
        // Run defers before returning
        for (auto& scope : deferStack) {
            for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
                emit(Opcode::CALL, *it);
            }
        }
        
        if (stmt->value) generate(stmt->value);
        emit(Opcode::RETURN);
    }
};
```

**Usage**:
```kern
fn processFile(path: str) -> Result<str, Error> {
    let file = open(path)?
    defer file.close()
    
    let buffer = allocate(1024)
    defer free(buffer)
    
    let data = read(file, buffer)?
    return Ok(data)
}  // file.close() and free(buffer) run here
```

---

## Phase 4: Collections (Week 8)

### 4.1 Array Improvements
**Pain**: Basic arrays with manual operations

**Built-in Methods**:
```kern
arr.len() -> int
arr.push(item)
arr.pop() -> T
arr.insert(index: int, item: T)
arr.remove(index: int) -> T
arr.clear()
arr.contains(item: T) -> bool
arr.find(item: T) -> int  // -1 if not found
arr.sort()
arr.reverse()
arr.slice(start: int, end: int) -> Array<T>
arr.map(fn: (T) -> U) -> Array<U>
arr.filter(fn: (T) -> bool) -> Array<T>
arr.fold(initial: U, fn: (U, T) -> U) -> U
```

**Map Methods**:
```kern
m.len() -> int
m[key] = value
m.get(key) -> Option<V>
m.remove(key)
m.contains(key) -> bool
m.keys() -> Array<K>
m.values() -> Array<V>
m.clear()
```

---

## Phase 5: Power Features (Optional - Weeks 9+)

### 5.1 Simple Enums (no payloads first)
```kern
enum Direction { North, South, East, West }
enum GameState { Menu, Playing, Paused, GameOver }

let dir = Direction::North
if dir == Direction::South { ... }
```

### 5.2 Iterator Protocol
```kern
// Any type with next() and hasNext()
struct Counter {
    current: int,
    max: int
}

impl Counter {
    fn hasNext(self) -> bool {
        return self.current < self.max
    }
    
    fn next(mut self) -> int {
        let val = self.current
        self.current = self.current + 1
        return val
    }
}

for x in Counter{current: 0, max: 10} {
    print(x)
}
```

---

## Summary: What Gets Built When

| Week | Feature | Pain Fixed | Lines Saved |
|------|---------|------------|-------------|
| 1 | Named args | `Entity("...", (...), 1)` mess | 30% |
| 1 | Random fix | `randomRange` error | - |
| 1-2 | Enhanced for | `while (i < len())` | 40% |
| 2-3 | Vec3 | `playerX, playerY, playerZ` | 50% |
| 3-4 | Structs | `{entity: e}` fragility | 30% |
| 5-6 | Modules | Single-file chaos | - |
| 6-7 | Result + ? | Error handling | 20% |
| 7 | Defer | Cleanup | 10% |
| 8 | Collections | Array operations | 20% |

**Total Impact**: Game code goes from 307 lines to ~150 lines, type-safe.

---

## Implementation Checklist

### Week 1
- [ ] Named argument parsing
- [ ] Named argument codegen (reordering)
- [ ] Random module alias
- [ ] Enhanced for-loop syntax
- [ ] For-loop desugaring

### Week 2  
- [ ] Vec3 value type
- [ ] Vec3 bytecode opcodes
- [ ] Vec3 VM implementation
- [ ] Vec3 stdlib functions

### Week 3
- [ ] Struct declaration parsing
- [ ] Struct literal parsing
- [ ] Field access parsing
- [ ] Struct type registry
- [ ] BUILD_STRUCT opcode

### Week 4
- [ ] Struct field access bytecode
- [ ] Struct semantic checking
- [ ] Struct method calls
- [ ] Game code rewrite test

### Week 5
- [ ] Module resolution
- [ ] Import statement handling
- [ ] Name mangling
- [ ] Visibility (pub)

### Week 6
- [ ] Result type
- [ ] ? operator parsing
- [ ] Try desugaring
- [ ] Result methods

### Week 7
- [ ] Defer statement
- [ ] Defer stack in codegen
- [ ] Scope-based defer execution

### Week 8
- [ ] Array methods
- [ ] Map methods
- [ ] Method dispatch system

---

## Testing Strategy

After each feature: **rewrite star_defender.kn**

Measure:
1. Line count reduction
2. Type errors caught (intentionally introduce bugs)
3. Readability score (subjective but useful)
4. Performance (frame time)

Success: 
- 50% line reduction
- All intentional typos caught at compile time
- Same or better frame time

---

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Structs too complex | Start heap-allocated, optimize later |
| Vec3 NaN boxing conflicts | Use tagged pointers, not unboxed |
| Module circular deps | Explicit ban, good error message |
| Result type complexity | Desugar early, optimize later |
| Scope creep | Strict weekly deliverables |

---

## The Bottom Line

**Build in this order**:
1. **Named args** → Readable code immediately
2. **Vec3** → Clean math  
3. **Structs** → Type safety
4. **Modules** → Organization
5. **Result + defer** → Robustness
6. **Collections** → Convenience
7. Everything else → Later

**Don't build**:
- Generics (use copy-paste for now)
- Ownership/borrowing (keep GC)
- Async (generators work)
- Enums with payloads (if/else works)

This gets you to **production-ready** in 8-10 weeks, not 8-10 months.
