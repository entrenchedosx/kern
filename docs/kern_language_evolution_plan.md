# Kern Language Evolution Plan v3.0
## Comprehensive Architectural Roadmap

**Current State**: Kern v2.0.2 with VM, bytecode, AST, lexer/parser, basic types, functions, classes, modules
**Target**: Production-ready language for real programs (tools, games, systems)

---

## Executive Summary

This document outlines 6 phases of evolution to transform Kern from a scripting language into a systems-capable language while maintaining:
- **VM Stability**: No breaking changes to core execution model
- **Performance**: Maintain unboxed value optimization
- **Simplicity**: No package manager dependency (kargo-free)

---

## Phase 1: Language Power (CORE FEATURES)

### 1.1 STRUCTS + METHODS

**Design Decisions:**
- Structs are value types (unlike classes which are reference types)
- Methods are syntactic sugar for functions with receiver
- Support immutable (`struct`) and mutable (`mut struct`) variants
- Memory layout: inline in stack/arrays for cache efficiency

**Syntax:**
```rust
// Basic struct
struct Vec3 {
    x: f32,
    y: f32,
    z: f32
}

// With defaults
struct Player {
    name: str = "Unknown",
    health: int = 100,
    pos: Vec3
}

// Methods
impl Vec3 {
    fn length(self) -> f32 {
        return sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    }
    
    fn normalize(mut self) {
        let len = self.length()
        self.x = self.x / len
        self.y = self.y / len
        self.z = self.z / len
    }
}

// Associated functions (static)
impl Vec3 {
    fn zero() -> Vec3 {
        return Vec3 { x: 0, y: 0, z: 0 }
    }
    
    fn up() -> Vec3 {
        return Vec3 { x: 0, y: 1, z: 0 }
    }
}

// Usage
let v = Vec3 { x: 1, y: 2, z: 3 }
let len = v.length()
let zero = Vec3::zero()
```

**Implementation Plan:**

**Parser Changes** (`parser.cpp`):
```cpp
// Add to declaration()
if (match(TokenType::STRUCT)) return structDeclaration();
if (match(TokenType::IMPL)) return implDeclaration();

// New AST nodes
struct StructDeclStmt : Stmt {
    std::string name;
    std::vector<FieldDecl> fields;
    std::vector<MethodDecl> methods;
    bool isMutable;  // mut struct vs struct
};

struct ImplStmt : Stmt {
    std::string targetType;
    std::vector<FunctionDeclStmt> methods;
    std::vector<FunctionDeclStmt> associatedFunctions;
};
```

**AST Changes** (`ast.hpp`):
```cpp
struct StructLiteral : Expr {
    std::string structName;
    std::vector<std::pair<std::string, ExprPtr>> fields;
};

struct MethodCallExpr : Expr {
    ExprPtr receiver;
    std::string methodName;
    std::vector<CallArg> args;
};

struct PathExpr : Expr {  // Vec3::zero()
    std::string typeName;
    std::string memberName;
};
```

**Bytecode Changes** (`bytecode.hpp`):
```cpp
enum class Opcode : uint8_t {
    // ... existing
    BUILD_STRUCT,      // operand = struct_type_id
    GET_STRUCT_FIELD,  // operand = field_index
    SET_STRUCT_FIELD,  // operand = field_index
    CALL_METHOD,       // operand = method_id
    CALL_ASSOCIATED,   // operand = associated_fn_id
};
```

**VM Changes** (`vm_unboxed.hpp`):
```cpp
// Struct layout in memory (inline, no heap)
struct StructValue {
    uint32_t typeId;  // struct type identifier
    // Fields inline after this header
    // Memory: [typeId][field0][field1][field2]...
};

// Struct type registry
struct StructType {
    std::string name;
    size_t size;  // total bytes
    std::vector<std::pair<std::string, Value::Type>> fields;
    std::unordered_map<std::string, size_t> fieldOffsets;
};
```

**Incremental Implementation:**
1. **Week 1**: Parser support for struct declarations
2. **Week 2**: AST nodes for struct literals and method calls
3. **Week 3**: Codegen for BUILD_STRUCT, field access
4. **Week 4**: VM struct type registry and execution
5. **Week 5**: impl blocks and method dispatch

**Risks:**
- **Memory alignment**: Ensure fields are properly aligned for unboxed values
- **Recursion**: Handle self-referential structs (use indirection)
- **Method resolution**: Avoid conflicts with class methods

---

### 1.2 ENUMS (TAGGED UNIONS)

**Design Decisions:**
- Enums are tagged unions (sum types)
- Pattern matching is exhaustive (compile-time check)
- Variants can carry data
- Memory: tag byte + union of variant payloads

**Syntax:**
```rust
// Simple enum
enum Direction {
    North,
    South,
    East,
    West
}

// Data-carrying enum
enum Option<T> {
    Some(T),
    None
}

enum Result<T, E> {
    Ok(T),
    Err(E)
}

// Complex example
enum Message {
    Quit,
    Move { x: int, y: int },
    Write(str),
    ChangeColor(int, int, int)
}

// Pattern matching
match msg {
    Quit => print("Quitting"),
    Move { x, y } => print("Moving to " + x + "," + y),
    Write(text) => print("Writing: " + text),
    ChangeColor(r, g, b) => print("Color change")
}

// With guards
match value {
    Some(x) if x > 0 => print("Positive"),
    Some(x) if x < 0 => print("Negative"),
    Some(0) => print("Zero"),
    None => print("Nothing")
}

// if-let binding
if let Some(x) = maybeValue {
    print("Got: " + x)
}

// while-let
while let Some(item) = iter.next() {
    process(item)
}
```

**Implementation Plan:**

**Parser Changes:**
```cpp
// Add to declaration()
if (match(TokenType::ENUM)) return enumDeclaration();

// New AST nodes
struct EnumDeclStmt : Stmt {
    std::string name;
    std::vector<GenericParam> generics;
    std::vector<EnumVariant> variants;
};

struct EnumVariant {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields;  // named
    std::vector<std::string> tupleTypes;  // tuple variant
};

struct MatchStmt : Stmt {
    ExprPtr value;
    std::vector<MatchArm> arms;
};

struct MatchArm {
    Pattern pattern;
    ExprPtr guard;  // optional if-expression
    StmtPtr body;
};

// Pattern types
struct Pattern {
    enum Type { Wildcard, Literal, Identifier, Struct, Tuple, Enum };
    Type type;
    // variant data...
};
```

**Bytecode Changes:**
```cpp
enum class Opcode : uint8_t {
    BUILD_ENUM,        // operand = (type_id, variant_id)
    MATCH_BEGIN,       // operand = (type_id, jump_table_offset)
    MATCH_ARM,         // operand = (variant_id, target_pc)
    MATCH_DEFAULT,     // operand = target_pc
    UNWRAP_ENUM,       // operand = payload_index
    IS_VARIANT,        // operand = variant_id -> bool
};
```

**VM Changes:**
```cpp
struct EnumValue {
    uint32_t typeId;
    uint8_t variantTag;
    // Payload union follows inline
};

// Jump table for match dispatch
struct MatchJumpTable {
    std::vector<std::pair<uint8_t, size_t>> variantJumps;
    size_t defaultJump;
};
```

**Incremental Implementation:**
1. **Week 1-2**: Parser for enum declarations and pattern syntax
2. **Week 3**: Exhaustiveness checker (semantic analysis)
3. **Week 4**: Codegen for match expressions
4. **Week 5**: VM enum creation and pattern matching
5. **Week 6**: if-let and while-let syntactic sugar

**Risks:**
- **Exhaustiveness checking**: Requires type information at compile time
- **Generic enums**: Monomorphization complexity
- **Pattern compilation**: Efficient jump table generation

---

### 1.3 GENERICS (MINIMAL BUT USEFUL)

**Design Decisions:**
- **Monomorphization** at compile time (no runtime cost)
- Type parameters with optional bounds
- No higher-kinded types (keep it simple)
- Generic functions, structs, and enums

**Syntax:**
```rust
// Generic function
fn identity<T>(x: T) -> T {
    return x
}

// Multiple type parameters
fn pair<T, U>(a: T, b: U) -> (T, U) {
    return (a, b)
}

// Generic struct
struct Vec3<T> {
    x: T,
    y: T,
    z: T
}

impl<T> Vec3<T> {
    fn map<U>(self, f: fn(T) -> U) -> Vec3<U> {
        return Vec3 {
            x: f(self.x),
            y: f(self.y),
            z: f(self.z)
        }
    }
}

// Generic enum
enum Option<T> {
    Some(T),
    None
}

impl<T> Option<T> {
    fn is_some(self) -> bool {
        match self {
            Some(_) => true,
            None => false
        }
    }
    
    fn unwrap(self) -> T {
        match self {
            Some(x) => x,
            None => panic("unwrap on None")
        }
    }
}

// Type constraints (simple)
fn max<T: Comparable>(a: T, b: T) -> T {
    if a > b { return a } else { return b }
}

// Usage (type inference)
let x = identity(5)        // T = int
let y = identity("hello")  // T = str
let v = Vec3 { x: 1, y: 2, z: 3 }  // T = int
```

**Implementation Plan:**

**Compiler Architecture Changes:**
```cpp
// Type representation
struct Type {
    enum Kind { Concrete, Generic, Parameter };
    Kind kind;
    std::string name;
    std::vector<Type> typeArgs;  // for Concrete<T, U>
};

// Generic instantiation cache
class GenericCache {
    std::map<std::pair<std::string, std::vector<Type>>, FunctionPtr> functionCache;
    std::map<std::pair<std::string, std::vector<Type>>, StructType> structCache;
    
public:
    FunctionPtr instantiateFunction(const std::string& name, const std::vector<Type>& args);
    StructType instantiateStruct(const std::string& name, const std::vector<Type>& args);
};

// AST modifications
struct GenericParam {
    std::string name;
    std::vector<std::string> bounds;  // e.g., ["Comparable", "Copy"]
};

struct FunctionDeclStmt : Stmt {
    std::string name;
    std::vector<GenericParam> generics;  // NEW
    std::vector<Param> params;
    Type returnType;
    StmtPtr body;
};
```

**Monomorphization Pipeline:**
```cpp
class Monomorphizer {
public:
    // Transform generic AST to concrete AST
    std::unique_ptr<Program> monomorphize(
        std::unique_ptr<Program> genericAst,
        const std::vector<InstantiationRequest>& requests
    );
    
private:
    // Clone and specialize function
    FunctionDeclStmt* instantiateFunction(
        const FunctionDeclStmt* generic,
        const std::map<std::string, Type>& substitutions
    );
    
    // Replace type parameters with concrete types
    Type substitute(const Type& type, const std::map<std::string, Type>& subs);
    ExprPtr substituteExpr(ExprPtr expr, const std::map<std::string, Type>& subs);
    StmtPtr substituteStmt(StmtPtr stmt, const std::map<std::string, Type>& subs);
};
```

**Type Inference:**
```cpp
class TypeInference {
public:
    // Hindley-Milner style constraint solving
    void inferTypes(Program* program);
    
private:
    // Generate constraints from AST
    std::vector<Constraint> generateConstraints(Expr* expr);
    
    // Unification
    bool unify(Type& a, Type& b);
    
    // Substitute solved variables
    void applySubstitutions(Program* program, const Substitution& subst);
};
```

**Incremental Implementation:**
1. **Week 1-2**: Generic syntax in parser
2. **Week 3-4**: Type representation and parameter tracking
3. **Week 5-6**: Monomorphizer for functions
4. **Week 7-8**: Generic structs and enums
5. **Week 9-10**: Type inference for generic calls

**Risks:**
- **Code bloat**: Monomorphization creates copies
- **Compile time**: Large generic hierarchies slow compilation
- **Recursive generics**: Need cycle detection

---

### 1.4 MODULE SYSTEM (NO PACKAGE MANAGER)

**Design Decisions:**
- File-based modules: one file = one module
- Hierarchical: `import "math/vector"` loads `math/vector.kn`
- Visibility: `pub` keyword for exports
- No circular dependencies (enforced)
- Module-level constants and initialization

**Syntax:**
```rust
// math.kn - module file
// =====================

// Private by default
fn internalHelper() -> int {
    return 42
}

// Public exports
pub fn add(a: int, b: int) -> int {
    return a + b
}

pub fn multiply(a: int, b: int) -> int {
    return a * b
}

// Public constant
pub const PI = 3.14159

// Public struct
pub struct Vec2 {
    x: f32,
    y: f32
}

impl Vec2 {
    pub fn length(self) -> f32 {
        return sqrt(self.x * self.x + self.y * self.y)
    }
}

// Module initialization (runs once on first import)
module init {
    print("Math module initialized")
}
```

```rust
// main.kn - using the module
// ==========================

// Import entire module
import "math"
let sum = math.add(1, 2)

// Import with alias
import "math" as m
let product = m.multiply(3, 4)

// Selective import
from "math" import add, multiply, Vec2
let v = Vec2 { x: 1, y: 2 }

// Import with destructuring
from "math" import { add, multiply as mul, PI }

// Nested module import
import "math/vector/3d"
let v3 = 3d.Vec3 { x: 1, y: 2, z: 3 }

// Relative imports
import "./local_module"
import "../parent_module"

// Namespace import
import "std/collections" as col
let arr = col.ArrayList()
```

**Implementation Plan:**

**Module Resolution:**
```cpp
class ModuleResolver {
    std::unordered_map<std::string, std::shared_ptr<Module>> cache;
    std::vector<std::string> searchPaths;
    
public:
    std::shared_ptr<Module> resolve(const std::string& importPath);
    
private:
    std::string findModuleFile(const std::string& path);
    std::shared_ptr<Module> loadModule(const std::string& filePath);
    void detectCircularDependencies(const std::string& path, 
                                     std::vector<std::string>& stack);
};

struct Module {
    std::string path;
    std::string absolutePath;
    std::unique_ptr<Program> ast;
    std::unordered_map<std::string, Export> exports;
    std::vector<std::shared_ptr<Module>> dependencies;
    bool initialized = false;
};

struct Export {
    enum Kind { Function, Struct, Enum, Variable, Constant, TypeAlias };
    Kind kind;
    std::string name;
    std::string fullyQualifiedName;
    // AST node reference (weak)
};
```

**Parser Changes:**
```cpp
// New keywords: pub, module, from

// New AST nodes
struct ImportStmt : Stmt {
    std::string modulePath;
    std::string alias;
    std::vector<std::string> namedImports;  // for "from" imports
    std::vector<std::pair<std::string, std::string>> renamedImports;
    bool isWildcard;  // import "mod".*
};

struct ModuleInitStmt : Stmt {
    StmtPtr body;  // Runs once on first import
};

// Visibility modifier on declarations
struct FunctionDeclStmt : Stmt {
    std::string name;
    bool isPublic;  // pub keyword
    // ... rest
};
```

**Codegen Changes:**
```cpp
class ModularCodegen {
public:
    // Compile module to bytecode with exports table
    CompiledModule compileModule(const Module& module);
    
    // Link multiple modules
    Program link(const std::vector<CompiledModule>& modules);
    
private:
    // Mangle names for exports
    std::string mangle(const std::string& moduleName, const std::string& symbol);
    
    // Generate module initialization code
    void emitModuleInit(CompiledModule& module, const ModuleInitStmt* init);
};

struct CompiledModule {
    std::string name;
    Bytecode code;
    std::unordered_map<std::string, size_t> exportTable;  // name -> function index
    std::vector<std::string> imports;  // required modules
};
```

**Incremental Implementation:**
1. **Week 1**: Module file resolution and loading
2. **Week 2**: `pub` visibility modifier
3. **Week 3**: Basic import/export mechanism
4. **Week 4**: `from ... import` selective imports
5. **Week 5**: Module initialization blocks
6. **Week 6**: Circular dependency detection

**Risks:**
- **Namespace collisions**: Mangled names must be unique
- **Initialization order**: Module init dependencies
- **Hot reload**: Dynamic module reloading (future)

---

## Phase 2: Control + Safety

### 2.1 ERROR HANDLING (Result Type + Propagation)

**Design Decisions:**
- `Result<T, E>` is a built-in enum (special-cased for ergonomics)
- `?` operator for error propagation
- `try/catch` remains for exceptional cases
- Error types are enums for pattern matching

**Syntax:**
```rust
// Result type (built-in)
enum Result<T, E> {
    Ok(T),
    Err(E)
}

// Function returning Result
fn readFile(path: str) -> Result<str, IOError> {
    if !exists(path) {
        return Err(IOError::NotFound)
    }
    // ... read file
    return Ok(content)
}

// Error propagation with ?
fn processFile(path: str) -> Result<Data, IOError> {
    let content = readFile(path)?  // Returns early on Err
    let data = parse(content)?     // Also propagates
    return Ok(data)
}

// Equivalent to:
fn processFile(path: str) -> Result<Data, IOError> {
    let content = match readFile(path) {
        Ok(c) => c,
        Err(e) => return Err(e)
    }
    // ...
}

// Custom error types
enum MyError {
    InvalidInput { field: str, reason: str },
    NetworkError { code: int },
    DatabaseError(str)
}

// try/catch with typed errors
try {
    let data = riskyOperation()
    process(data)
} catch MyError::InvalidInput as e {
    print("Bad input: " + e.field)
} catch MyError::NetworkError as e if e.code == 404 {
    print("Not found")
} catch e {
    print("Other error: " + e)
} finally {
    cleanup()
}

// Result methods
let result = someOperation()
if result.is_ok() {
    let value = result.unwrap()
}
let value = result.unwrap_or(defaultValue)
let value = result.unwrap_or_else(|| computeDefault())
```

**Implementation Plan:**

**Parser Changes:**
```cpp
// ? postfix operator
struct TryExpr : Expr {
    ExprPtr value;  // expression being "tried"
};

// catch with pattern matching
try {
    // ...
} catch Pattern as identifier {
    // ...
}
```

**Bytecode Changes:**
```cpp
enum class Opcode : uint8_t {
    // ... existing
    RESULT_OK,         // pop value, wrap in Ok variant
    RESULT_ERR,        // pop value, wrap in Err variant
    UNWRAP_RESULT,     // pop result, push value or jump to error handler
    IS_RESULT_OK,      // pop result, push bool
    PROPAGATE_ERROR,   // check result, if Err return early
};
```

**VM Changes:**
```cpp
// Special handling for Result to avoid allocation
struct ResultValue {
    bool isOk;
    Value payload;  // Ok value or Err value
    // Stored inline when possible
};

// Error propagation logic
class VM {
    void handlePropagation() {
        // Check if current result is Err
        // If so, unwind to nearest try/catch or return
    }
};
```

**Incremental Implementation:**
1. **Week 1-2**: Result type as special enum
2. **Week 3**: `?` operator parsing and desugaring
3. **Week 4**: Error propagation bytecode
4. **Week 5**: try/catch with pattern matching
5. **Week 6**: Result helper methods

---

### 2.2 DEFER / CLEANUP

**Design Decisions:**
- `defer` executes at scope exit (LIFO order)
- Works with return, break, continue, panic
- Deferred expressions captured at point of defer
- Multiple defers stack

**Syntax:**
```rust
fn processFile(path: str) -> Result<str, Error> {
    let file = open(path)?
    defer file.close()  // Runs at scope exit
    
    let buffer = allocate(1024)
    defer free(buffer)  // Runs second (LIFO)
    
    if !valid(file) {
        return Err(Error::Invalid)  // defers run before return
    }
    
    let data = read(file, buffer)
    return Ok(data)  // defers run before return
}

// Defer with block
defer {
    cleanup()
    log("Exiting scope")
}

// Conditional defer (optional)
let needsCleanup = true
defer if needsCleanup {
    cleanup()
}

// Error-aware defer
defer {
    if errorOccurred() {
        rollback()
    }
}
```

**Implementation Plan:**

**AST Changes:**
```cpp
struct DeferStmt : Stmt {
    ExprPtr expr;      // single expression
    StmtPtr block;     // or block statement
    ExprPtr condition; // optional: defer if condition
};
```

**Bytecode Changes:**
```cpp
enum class Opcode : uint8_t {
    // ... existing
    DEFER,             // operand = jump offset to deferred code
    DEFER_CAPTURE,     // capture local values at defer point
    RUN_DEFERS,        // execute all pending defers (on scope exit)
    CLEAR_DEFERS,      // clear defer stack (normal exit)
};
```

**VM Changes:**
```cpp
struct DeferredCall {
    size_t codeOffset;      // where to jump to run defer
    std::vector<Value> captures;  // captured local values at defer point
    bool condition;         // if conditional defer
};

class VM {
    std::vector<std::vector<DeferredCall>> deferStack;  // per scope
    
    void executeDefer(DeferredCall& defer);
    void runAllDefers();
    void clearDefers();
};
```

**Incremental Implementation:**
1. **Week 1**: Parser support for defer
2. **Week 2**: AST node and semantic analysis
3. **Week 3**: Codegen for defer capture
4. **Week 4**: VM defer stack management
5. **Week 5**: Integration with control flow (return, break)

---

### 2.3 OPTIONAL TYPE SYSTEM IMPROVEMENTS

**Local Type Inference:**
```rust
// Explicit type
let x: int = 5

// Inferred type
let y = 5        // int
let z = 3.14     // f64
let s = "hello"  // str

// Function return type inference
fn add(a: int, b: int) {  // inferred: -> int
    return a + b
}

// Generic type inference
fn identity<T>(x: T) -> T { return x }
let n = identity(5)      // T = int, inferred
```

**Stronger Type Checking:**
```rust
// No implicit conversions
let x: int = 5
let y: f64 = x    // ERROR: need explicit cast
let y: f64 = x as f64  // OK

// Exhaustive match checking
match option {
    Some(x) => print(x)
    // ERROR: missing None case
}

// Unused variable warnings
let unused = 5  // WARNING: unused
```

---

## Phase 3: Data + Memory Model

### 3.1 IMPROVED COLLECTIONS

**Built-in Types:**
```rust
// Array (dynamic, contiguous)
let arr = [1, 2, 3]
arr.push(4)
arr.pop()
let len = arr.len()
let first = arr[0]
arr[0] = 10

// Map (hash map)
let m = {"a": 1, "b": 2}
m["c"] = 3
let val = m["a"]
let has = m.contains("a")
m.remove("b")

// Set (hash set)
let s = set {1, 2, 3}
s.insert(4)
s.remove(2)
let isMember = s.contains(3)
let union = s1 | s2
let intersect = s1 & s2

// String builder
let sb = StringBuilder()
sb.append("Hello")
sb.append(" World")
let str = sb.toString()
```

**Syntax Sugar:**
```rust
// Array comprehension
let squares = [for x in range(10): x * x]
let evens = [for x in range(100) if x % 2 == 0: x]

// Map comprehension
let squaresMap = {x: x*x for x in range(10)}
```

**Implementation:**
```cpp
// VM built-in types
struct ArrayObject {
    std::vector<Value> data;
    size_t capacity;
};

struct MapObject {
    std::unordered_map<std::string, Value> data;
};

struct SetObject {
    std::unordered_set<Value, ValueHash> data;
};
```

---

### 3.2 ITERATORS

**Iterator Protocol:**
```rust
// Any type with next() method is iterable
struct Counter {
    current: int,
    max: int
}

impl Counter {
    fn next(mut self) -> Option<int> {
        if self.current >= self.max {
            return None
        }
        let val = self.current
        self.current = self.current + 1
        return Some(val)
    }
}

// For-in loop
let counter = Counter { current: 0, max: 10 }
for x in counter {
    print(x)
}

// Range iterator (built-in)
for i in 0..10 {
    print(i)
}

// Iterator methods
let sum = [1, 2, 3].iter().fold(0, |a, b| a + b)
let doubled = [1, 2, 3].iter().map(|x| x * 2).collect()
let first = [1, 2, 3].iter().find(|x| x > 1)
```

**Implementation:**
```cpp
// Iterator trait (concept)
struct IteratorTrait {
    virtual Value next() = 0;
    virtual bool hasNext() = 0;
};

// Bytecode
FOR_IN_INIT    // setup iterator
FOR_IN_NEXT    // get next value or jump to end
FOR_IN_DONE    // cleanup
```

---

### 3.3 MEMORY MODEL IMPROVEMENTS

**Ownership and Borrowing (Simplified):**
```rust
// Move semantics
let v = Vec3 { x: 1, y: 2, z: 3 }
let v2 = v      // v is moved, no longer valid
// print(v.x)    // ERROR: use after move

// Borrowing
fn length(v: &Vec3) -> f32 {  // immutable borrow
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
}

fn normalize(v: &mut Vec3) {  // mutable borrow
    let len = length(v)
    v.x = v.x / len
}

// Clone for explicit copy
let v2 = v.clone()
```

**Explicit Memory Control:**
```rust
// Stack allocation
let arr = stack [int; 100]  // Fixed size, no heap

// Heap allocation
let big = heap Array[int]()   // Explicit heap

// Arena allocation
let arena = Arena()
let obj1 = arena.alloc(MyStruct {})
let obj2 = arena.alloc(MyStruct {})
defer arena.free()  // Free all at once
```

---

## Phase 4: Standard Library Expansion

### 4.1 FILE SYSTEM (fs module)

```rust
import "std/fs"

// File operations
let content = fs.read("file.txt")?           // Read entire file
fs.write("file.txt", "content")?             // Write file
fs.append("file.txt", "more")?               // Append

// Path operations
let exists = fs.exists("path")
let isDir = fs.isDir("path")
let size = fs.size("file.txt")

// Directory operations
let entries = fs.readDir(".")?                // List directory
fs.createDir("new_dir")?
fs.removeDir("old_dir")?
fs.removeFile("old.txt")?

// Path manipulation
let joined = fs.join("dir", "file.txt")
let parent = fs.parent("/a/b/c")
let filename = fs.filename("/a/b/c.txt")
let ext = fs.extension("file.txt")

// Walking
for entry in fs.walk(".") {
    print(entry.path + " (" + entry.size + " bytes)")
}
```

### 4.2 PROCESS MANAGEMENT (process module)

```rust
import "std/process"

// Spawn process
let result = process.run("ls", ["-la"])?     // Run and wait
print(result.stdout)
print(result.code)

// Streaming
let cmd = process.spawn("grep", ["pattern"])?
cmd.stdin.write("input data")
for line in cmd.stdout {
    print(line)
}
let code = cmd.wait()

// Environment
let path = process.env("PATH")
process.setEnv("MY_VAR", "value")
let allEnv = process.env()  // get all as map

// Exit
process.exit(0)
```

### 4.3 TIME AND TIMERS (time module)

```rust
import "std/time"

// Duration literals
let d1 = 5s
let d2 = 100ms
let d3 = 1m
let d4 = 1h

// Sleep
time.sleep(1s)

// Timestamps
let now = time.now()
let later = now + 5s

// Formatting
let str = time.format(now, "%Y-%m-%d %H:%M:%S")
let parsed = time.parse("2024-01-01", "%Y-%m-%d")?

// Timer for games/loops
let timer = time.Timer()
loop {
    let dt = timer.elapsed()
    timer.reset()
    update(dt)
}

// FPS counter
let fps = time.FpsCounter()
loop {
    fps.tick()
    if fps.count() % 60 == 0 {
        print(fps.rate())  // FPS
    }
}
```

### 4.4 STRING MANIPULATION (string module)

```rust
import "std/string"

// Builder
let sb = string.Builder()
sb.append("Hello")
sb.append(" ")
sb.append("World")
let s = sb.toString()

// Manipulation
let upper = string.toUpper("hello")      // "HELLO"
let lower = string.toLower("WORLD")      // "world"
let trimmed = string.trim("  hello  ")
let replaced = string.replace("hello", "l", "L")

// Split/Join
let parts = string.split("a,b,c", ",")    // ["a", "b", "c"]
let joined = string.join(["a", "b"], ",") // "a,b"

// Search
let idx = string.find("hello", "ll")       // 2
let has = string.contains("hello", "ll")   // true
let starts = string.startsWith("hello", "he")
let ends = string.endsWith("hello", "lo")

// Pattern matching
if string.matches("hello.txt", "*.txt") {
    // ...
}
```

### 4.5 MATH MODULE

```rust
import "std/math"

// Constants
let pi = math.PI
let e = math.E

// Basic
let abs = math.abs(-5)        // 5
let min = math.min(1, 2)      // 1
let max = math.max(1, 2)      // 2
let clamp = math.clamp(x, 0, 100)

// Powers
let pow = math.pow(2, 10)     // 1024
let sqrt = math.sqrt(16)      // 4
let cbrt = math.cbrt(27)      // 3

// Trig
let sin = math.sin(math.PI / 2)
let cos = math.cos(0)
let tan = math.tan(0.5)

// Rounding
let floor = math.floor(3.7)   // 3
let ceil = math.ceil(3.1)     // 4
let round = math.round(3.5)   // 4

// Random
let rng = math.Rng()
let n = rng.int(0, 100)       // 0-99
let f = rng.float(0, 1)       // 0.0-1.0
let choice = rng.choice(["a", "b", "c"])

// Vectors (for games)
let v1 = math.Vec2(1, 2)
let v2 = math.Vec3(1, 2, 3)
let len = v2.length()
let normalized = v2.normalize()
let dot = v1.dot(v2.xy())
```

### 4.6 JSON/TOML PARSING

```rust
import "std/json"

// Parsing
let data = json.parse('{"name": "John", "age": 30}')?
print(data["name"])

// Serialization
let obj = {"name": "John", "age": 30}
let str = json.stringify(obj)
let pretty = json.stringify(obj, { pretty: true })

// Typed parsing
struct User {
    name: str,
    age: int
}
let user: User = json.parseAs('{"name": "John", "age": 30}')?

// TOML
import "std/toml"
let config = toml.parseFile("config.toml")?
let port = config["server"]["port"]
```

---

## Phase 5: Advanced Capabilities

### 5.1 ASYNC / COROUTINES

**Design:**
- Built on existing generator/yield system
- `async fn` returns `Future<T>`
- `await` suspends until completion
- Single-threaded event loop (cooperative)

**Syntax:**
```rust
// Async function
async fn fetchData(url: str) -> Result<Data, Error> {
    let response = await httpGet(url)?
    let data = await parseAsync(response)?
    return Ok(data)
}

// Main async entry point
async fn main() {
    let task1 = fetchData("url1")
    let task2 = fetchData("url2")
    
    // Concurrent execution
    let (data1, data2) = await (task1, task2)
    
    // Or sequential
    let d1 = await fetchData("url1")?
    let d2 = await fetchData("url2")?
}

// Spawn tasks
let handle = spawn asyncTask()
let result = await handle

// Select (race)
let result = await select {
    timeout(5s) => Err(Error::Timeout),
    fetchData(url) => data
}
```

**Implementation:**
```cpp
struct Future {
    GeneratorObject generator;
    bool completed;
    Value result;
};

// Event loop
class AsyncRuntime {
    std::queue<Future> ready;
    std::vector<Future> waiting;
    
    void run() {
        while (!ready.empty()) {
            auto future = ready.pop();
            resume(future);
        }
    }
};
```

### 5.2 REFLECTION / TYPE INFO

```rust
// Type inspection
let t = type_of(x)
print(t.name)           // "Vec3"
print(t.fields)         // ["x", "y", "z"]
print(t.methods)        // ["length", "normalize"]

// Dynamic access
if t.hasField("x") {
    let val = t.getField(x, "x")
    t.setField(x, "x", 10)
}

// Cast checking
if is_type(x, Vec3) {
    let v = cast<Vec3>(x)
}

// Enum variant checking
if let Some(v) = option {
    let variant = variant_of(option)
    print(variant.name)  // "Some"
    print(variant.data)  // [v]
}
```

### 5.3 PLUGIN SYSTEM (No Kargo)

```rust
// Load native library dynamically
let lib = loadLibrary("./myplugin.dll")?

// Get function pointer
let process = lib.getFn<(int) -> int>("process")?
let result = process(42)

// Or with FFI-like interface
extern "./myplugin.dll" {
    fn process(x: int) -> int
}

// Safe wrapper
fn safeProcess(x: int) -> Result<int, Error> {
    try {
        return Ok(process(x))
    } catch e {
        return Err(Error::Plugin(e))
    }
}
```

---

## Phase 6: Developer Experience

### 6.1 BETTER ERRORS

**Error Message Format:**
```
error[E0425]: Undefined variable `foo`
  --> src/main.kn:10:15
   |
10 |     let x = foo + 1
   |             ^^^ not found in this scope
   |
   = help: Did you mean `for`? Perhaps you forgot to declare it.
   = note: Available names: bar, baz, qux

error[E0308]: Type mismatch
  --> src/main.kn:25:9
   |
25 |     let x: int = "hello"
   |                  ^^^^^^^ expected `int`, found `str`
   |
   = help: You can convert using: `parseInt("hello")` or `"hello".len()` for length
```

**Implementation:**
```cpp
struct CompileError {
    ErrorCode code;
    std::string message;
    SourceLocation location;
    std::vector<std::string> helpNotes;
    std::vector<FixSuggestion> suggestions;
};

class ErrorFormatter {
    std::string format(const CompileError& error);
    void printContext(const SourceLocation& loc);
    std::vector<std::string> suggestFixes(const CompileError& error);
};
```

### 6.2 DEBUGGING SUPPORT

**Stack Traces:**
```rust
// Automatic on panic
panic("Something went wrong")
// Output:
// thread 'main' panicked at 'Something went wrong', src/main.kn:15
// stack backtrace:
//    0: main::process
//            at src/main.kn:15
//    1: main::main
//            at src/main.kn:8
//    2: _start

// Manual backtrace
print(backtrace())
```

**Debug Breakpoints:**
```rust
// Conditional breakpoint
debug.break()           // Stop debugger
debug.break_if(x > 100) // Conditional

// Inspect values
debug.inspect(x)        // Print type and value
debug.watch(y)          // Break on change
```

---

## Implementation Timeline

### Month 1-2: Foundation
- **Week 1-2**: Structs with methods
- **Week 3-4**: Module system basics
- **Week 5-6**: Error handling with Result
- **Week 7-8**: Defer implementation

### Month 3-4: Type System
- **Week 9-10**: Enums and pattern matching
- **Week 11-12**: Generics (monomorphization)
- **Week 13-14**: Type inference improvements
- **Week 15-16**: Memory model clarifications

### Month 5-6: Collections + Iterators
- **Week 17-18**: Improved collections (Array, Map, Set)
- **Week 19-20**: Iterator protocol
- **Week 21-22**: Comprehensions
- **Week 23-24**: Collection methods

### Month 7-8: Standard Library
- **Week 25-26**: File system module
- **Week 27-28**: Process management
- **Week 29-30**: Time and timers
- **Week 31-32**: String manipulation
- **Week 33-34**: Math and vectors
- **Week 35-36**: JSON/TOML parsing

### Month 9-10: Advanced Features
- **Week 37-38**: Async/await
- **Week 39-40**: Reflection system
- **Week 41-42**: Plugin loading
- **Week 43-44**: Better error messages

### Month 11-12: Polish
- **Week 45-46**: Debugging support
- **Week 47-48**: Documentation and examples
- **Week 49-50**: Testing and bug fixes
- **Week 51-52**: Performance optimization

---

## Architecture Summary

### Modified Components

| Component | Changes |
|-----------|---------|
| **Lexer** | New tokens: `pub`, `struct`, `enum`, `match`, `impl`, `defer`, `async`, `await` |
| **Parser** | 15+ new AST node types, pattern parsing |
| **AST** | Structs, enums, impl blocks, match arms |
| **Semantic** | Type checking, exhaustiveness, ownership analysis |
| **Monomorphizer** | Generic instantiation (new phase) |
| **Codegen** | 20+ new opcodes |
| **VM** | Struct registry, enum support, defer stack |
| **Stdlib** | 6+ new modules |

### Backwards Compatibility

All existing Kern v2.0.2 code continues to work:
- Classes remain unchanged
- Functions work as before
- Module system is additive
- New features are opt-in

### Performance Targets

- **Structs**: Same speed as classes (inline in stack when possible)
- **Enums**: One byte tag overhead, no heap for small payloads
- **Generics**: Zero runtime cost (monomorphization)
- **Iterators**: Comparable to C++ iterators
- **Async**: Minimal overhead over generators

---

## Success Metrics

1. **Can build**: CLI tools, games, system utilities
2. **Performance**: Within 2x of C for numeric code
3. **Compile time**: < 1s for 10K LOC
4. **Binary size**: < 1MB for simple programs
5. **Error quality**: Helpful messages with suggestions
6. **Documentation**: Complete stdlib docs with examples

This evolution transforms Kern from a scripting language into a systems-capable language suitable for real-world applications while maintaining its simplicity and performance characteristics.
