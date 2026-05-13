# Kern Architecture Documentation

## 1. PROJECT OVERVIEW

### 1.1 What Kern Is

Kern is a **modular, runtime-first virtual machine system** designed for systems programming with explicit architectural boundaries. It is not a general-purpose language runtime but a **compiler-validated, dependency-isolated execution environment** for building modular, testable runtime systems.

**Core Identity:**
- **Runtime-first architecture:** The VM and runtime core are the foundation, not an afterthought
- **Compile-time architecture enforcement:** Dependencies are validated at build time, not runtime
- **Modular isolation:** Each component (VM, runtime core, ECS, graphics) can compile and link independently
- **Systems programming focus:** Direct memory access, FFI, and unsafe execution are first-class features

### 1.2 Core Philosophy

**Architectural Truth over Convention:**
- The compiler is the ultimate authority on module boundaries
- Include graphs must be acyclic and explicitly declared
- No implicit dependencies or global state
- Every dependency direction is justified and enforced

**Minimal Viable Abstraction:**
- Zero-cost abstractions where possible
- Explicit ownership and lifetime management
- No hidden allocation or indirection
- Direct access to underlying systems when needed

**Validation-Driven Development:**
- Every architectural claim must be proven by compilation
- Build matrix tests enforce isolation guarantees
- Runtime tests follow successful compilation validation
- No "theoretically correct" without "compiler-proven"

### 1.3 Design Goals

**Primary Goals:**
1. **Modular Verifiability:** Each module can be compiled, linked, and tested in isolation
2. **Performance Transparency:** No hidden costs, predictable execution paths
3. **Systems Access:** Direct FFI, memory manipulation, and unsafe execution when needed
4. **Architectural Clarity:** Dependency graphs are explicit and enforced
5. **Incremental Validation:** Compile-time validation before runtime testing

**Secondary Goals:**
- Language-agnostic VM interface
- Pluggable module system
- Cross-platform compatibility
- Toolchain integration

### 1.4 What Kern Is NOT

**Kern is NOT:**
- A general-purpose scripting language runtime
- A managed language with automatic garbage collection
- A sandboxed execution environment (by default)
- A web-oriented JavaScript replacement
- A memory-safe language (unsafe is explicit and available)
- A dynamically-typed language runtime (typing is static at compile time)

### 1.5 Why Kern Exists

**Problem Statement:**
Existing runtime systems suffer from implicit dependencies, circular includes, and architectural drift. Modular systems become monolithic through gradual boundary erosion. Testing isolation is impossible without full system compilation.

**Solution Approach:**
Kern enforces architectural boundaries at compile time using:
- Explicit dependency declarations in CMake
- Include firewall validation via Python scripts
- Build matrix isolation testing
- Legacy quarantine for unavoidable violations

### 1.6 Target Use Cases

**Primary Use Cases:**
1. **Game engines:** Modular runtime with pluggable systems (ECS, graphics, audio)
2. **Embedded systems:** Verifiable module boundaries for safety-critical code
3. **Plugin architectures:** Host applications with dynamically loadable modules
4. **Compiler research:** Testbed for language runtime experiments
5. **Systems programming:** Runtime for custom languages with FFI requirements

### 1.7 Performance Goals

**Execution Performance:**
- Direct-threaded opcode dispatch (computed goto)
- Register-window stack machine (not pure stack)
- Arena allocation for frames
- Bounds-checked but not bounds-prevented operations
- Zero-cost abstractions where possible

**Memory Performance:**
- 32-byte Value objects with variant storage
- Explicit ownership, no reference counting by default
- Arena allocation for short-lived objects
- Optional garbage collection for specific use cases

### 1.8 Comparison to Existing Systems

**vs Python/Lua:**
- Static typing at compile time vs dynamic typing
- Explicit module boundaries vs implicit imports
- Systems-level access vs high-level abstraction
- Compiler validation vs runtime errors

**vs C#:**
- No mandatory runtime/CLR dependency
- Explicit unsafe blocks vs safe-by-default
- Modular compilation vs monolithic assemblies
- Direct memory access vs managed heap

**vs Rust:**
- Unsafe blocks are explicit and encouraged for systems access
- No borrow checker - explicit ownership instead
- Modular isolation enforced at build time vs language level
- Focus on runtime modularity vs compile-time safety

## 2. LANGUAGE ARCHITECTURE

### 2.1 Lexer

**Design Philosophy:**
The lexer is deliberately simple and fast, focusing on token recognition rather than complex lexical analysis. It supports unicode identifiers and standard programming language tokens.

**Implementation Details:**
- Single-pass character stream processing
- UTF-8 source encoding with unicode identifier support
- Minimal state machine for token recognition
- Direct string interning for identifiers and literals
- Line/column tracking for error reporting

**Token Categories:**
- Keywords: `if`, `else`, `while`, `for`, `function`, `class`, `import`, etc.
- Literals: integers, floats, strings, booleans, nil
- Identifiers: user-defined names with unicode support
- Operators: arithmetic, logical, assignment, comparison
- Delimiters: parentheses, braces, brackets, commas, semicolons

### 2.2 Parser

**Parser Type:**
Recursive descent parser with explicit error recovery. The parser builds an Abstract Syntax Tree (AST) while performing semantic validation.

**Grammar Characteristics:**
- Expression-based grammar with operator precedence
- Statement-level constructs (if, while, for, function definitions)
- Module-level declarations (imports, class definitions, function definitions)
- Type annotations where required (function parameters, return types)

**Error Recovery:**
- Panic mode recovery at statement boundaries
- Synchronized recovery at block boundaries
- Error accumulation for multiple error reporting
- Contextual error messages with source location

### 2.3 AST

**Node Types:**
```cpp
// Core expression nodes
class Expression;
class LiteralExpression;
class BinaryExpression;
class UnaryExpression;
class CallExpression;
class MemberExpression;
class IndexExpression;

// Statement nodes
class Statement;
class ExpressionStatement;
class IfStatement;
class WhileStatement;
class ForStatement;
class ReturnStatement;
class BlockStatement;

// Declaration nodes
class FunctionDeclaration;
class ClassDeclaration;
class VariableDeclaration;
class ImportDeclaration;
```

**AST Properties:**
- Immutable after construction
- Parent pointers for context navigation
- Source location information for error reporting
- Type information attached after semantic analysis
- Memory-efficient using object pools where appropriate

### 2.4 Bytecode Generation

**Bytecode Design:**
- Register-based instruction set (not stack-based)
- Variable-length instruction encoding
- Immediate values embedded in instructions
- Jump targets as relative offsets
- Function calls via index into function table

**Instruction Format:**
```
[opcode:8][register_a:8][register_b:8][register_c:8]
[opcode:8][register_a:8][register_b:8][immediate:16]
[opcode:8][register_a:8][constant_index:24]
```

**Generation Strategy:**
- Single-pass AST to bytecode translation
- Register allocation via simple linear scan
- Constant folding during generation
- Dead code elimination for unreachable branches
- Inline caching optimization points identified

### 2.5 Runtime Execution Model

**Execution Philosophy:**
Direct-threaded dispatch with register windows for efficient function calls. The VM is designed for high-throughput execution with minimal indirection.

**Key Characteristics:**
- Register-window stack machine (16 registers per frame)
- Direct-threaded dispatch (computed goto on supported platforms)
- Arena allocation for call frames
- Bounds-checked operations with fast paths
- Explicit garbage collection points

### 2.6 VM Execution Flow

**Initialization Sequence:**
1. VM instance creation with configuration
2. Bytecode loading and verification
3. String pool and value pool initialization
4. Built-in function registration
5. Global variable setup
6. Entry point location and execution start

**Execution Loop:**
```cpp
dispatch_table[opcode]();
// Direct-threaded dispatch eliminates switch overhead
```

**Call Handling:**
- Register window sliding for parameter passing
- Overlapping caller/callee registers (8 input, 8 local)
- Fast path for built-in function calls
- Exception handling via try-catch frames

### 2.7 Instruction Dispatch

**Dispatch Methods:**
1. **Direct-threaded (preferred):** Computed goto for maximum speed
2. **Switch-based:** Fallback for platforms without computed goto
3. **Token-threaded:** Experimental for debugging

**Dispatch Optimization:**
- Opcode prediction for common sequences
- Inline caching for property access
- Specialized fast paths for arithmetic operations
- Branch prediction hints for conditional jumps

### 2.8 Value Representation

**Value Layout (32 bytes):**
```cpp
class alignas(8) Value {
    VariantType data;      // 24 bytes - actual value storage
    ValueType type;        // 1 byte  - type discriminator
    // 7 bytes padding for alignment
};
```

**Type System:**
- Variant-based storage with small string optimization
- Immediate values for integers, floats, booleans
- Heap allocation for strings, arrays, maps, objects
- Type tags for fast discrimination
- Legacy compatibility types for builtins

**Memory Strategy:**
- Small values stored directly in variant
- Large values use heap allocation with shared_ptr
- Reference counting for complex objects
- Arena allocation for temporary values

### 2.9 Memory Model

**Ownership Philosophy:**
Explicit ownership with optional sharing. No automatic garbage collection by default.

**Memory Categories:**
1. **Stack:** Register windows and call frames
2. **Arena:** Short-lived objects, cleared per frame
3. **Heap:** Long-lived objects with explicit lifetime
4. **Constant:** Immutable data shared across executions

**Allocation Strategy:**
- Arena allocation for temporary values
- Reference counting for shared objects
- Explicit deallocation for owned objects
- Optional mark-sweep GC for specific use cases

### 2.10 Error Propagation

**Error Types:**
- **Runtime errors:** Division by zero, null pointer, type errors
- **VM errors:** Stack overflow, out of memory, invalid bytecode
- **User errors:** Application-specific error values
- **System errors:** FFI failures, I/O errors

**Error Handling:**
- Exception frames for try-catch constructs
- Error values as first-class objects
- Stack trace capture on error
- Error propagation through call stack
- Recovery via exception handlers

### 2.11 Module Loading

**Loading Strategy:**
- Lazy loading with dependency resolution
- Bytecode caching for performance
- Module version compatibility checking
- Circular dependency detection

**Import Resolution:**
- Module registry lookup
- Path resolution rules
- Built-in module precedence
- Dynamic loading support

### 2.12 Import Resolution

**Resolution Algorithm:**
1. Check built-in module registry
2. Search module path directories
3. Load bytecode file and verify
4. Resolve module dependencies
5. Execute module initialization
6. Register module exports

**Dependency Management:**
- Explicit dependency declaration
- Topological sort for initialization order
- Circular dependency detection and reporting
- Version compatibility checking

## 3. VM INTERNALS

### 3.1 VM Lifecycle

**Creation Phase:**
```cpp
VMConfig config;
config.initialStackSize = 1024;
config.maxStackSize = 100000;
config.maxCallDepth = 1000;
config.enableTracing = false;

VM vm(config);
```

**Initialization Sequence:**
1. Configuration validation and setup
2. Memory arena allocation
3. Built-in function registration
4. Global variable initialization
5. Module registry setup
6. Execution state initialization

**Execution Phase:**
1. Bytecode loading and verification
2. String pool and value population
3. Entry point identification
4. Main execution loop start
5. Periodic garbage collection (if enabled)

**Shutdown Phase:**
1. Execution halt request handling
2. Module cleanup and deregistration
3. Memory arena deallocation
4. Resource cleanup and finalization

### 3.2 Stack Model

**Register Windows:**
- 16 registers per frame (8 input, 8 local)
- Overlapping windows for function calls
- Fast parameter passing via register overlap
- Bounds checking with minimal overhead

**Call Frame Layout:**
```cpp
struct CallFrame {
    const uint8_t* returnPc;        // Return address
    const RegisterWindow* callerWindow; // Caller register base
    size_t stackBase;               // Stack base index
    std::string functionName;       // Debug information
    std::string filePath;           // Source location
    uint32_t line;                  // Source line
    uint32_t column;                // Source column
};
```

**Stack Operations:**
- Push/pop operations for value storage
- Register window sliding for calls
- Exception frame management
- Stack overflow detection

### 3.3 Call Frames

**Frame Management:**
- Arena allocation for efficiency
- Automatic cleanup on return
- Exception stack unwinding support
- Debug information retention

**Frame Types:**
1. **Function frames:** Regular function calls
2. **Builtin frames:** Native function calls
3. **Exception frames:** Try-catch boundaries
4. **Module frames:** Module initialization

**Frame Lifecycle:**
1. Allocation on function entry
2. Parameter register setup
3. Local variable initialization
4. Execution and exception handling
5. Cleanup and deallocation on exit

### 3.4 Bytecode Layout

**Instruction Encoding:**
```cpp
// 32-bit instruction format
struct Instruction {
    uint8_t opcode;      // Operation code
    uint8_t reg_a;       // Primary register
    uint8_t reg_b;       // Secondary register
    uint8_t reg_c;       // Tertiary register / immediate high
};
```

**Bytecode Sections:**
1. **Header:** Version information and metadata
2. **Constants:** Literal values and strings
3. **Functions:** Function bytecode and metadata
4. **Debug:** Source mapping and debug info
5. **Export:** Public interface definitions

**Optimization Markers:**
- Inline cache points
- Hot path indicators
- Branch prediction hints
- Profile-guided optimization data

### 3.5 Opcode System

**Opcode Categories:**
```cpp
// Stack operations
OP_PUSH_CONST, OP_PUSH_NIL, OP_PUSH_TRUE, OP_PUSH_FALSE, OP_POP

// Register operations
OP_LOAD_LOCAL, OP_STORE_LOCAL, OP_LOAD_GLOBAL, OP_STORE_GLOBAL

// Arithmetic operations
OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW, OP_NEG

// Logical operations
OP_NOT, OP_EQ, OP_LT, OP_LE, OP_GT, OP_GE

// Control flow
OP_JUMP, OP_JUMP_IF_TRUE, OP_JUMP_IF_FALSE, OP_CALL, OP_RETURN

// Data structures
OP_MAKE_ARRAY, OP_MAKE_MAP, OP_MAKE_CLASS, OP_LOAD_FIELD, OP_STORE_FIELD

// Exception handling
OP_THROW, OP_TRY_BEGIN, OP_TRY_END

// Module operations
OP_IMPORT, OP_HALT
```

**Opcode Implementation:**
- Direct-threaded dispatch via computed goto
- Specialized fast paths for common operations
- Bounds checking with fast-path optimization
- Type specialization where possible

### 3.6 Native Function Binding

**Binding Strategy:**
- Function pointer registration with type checking
- Automatic parameter conversion and validation
- Exception propagation between native and managed code
- Performance-critical fast paths for frequently called functions

**Binding Interface:**
```cpp
using BuiltinFn = std::function<Value(VM*, std::vector<ValuePtr>)>;

void registerBuiltin(size_t index, BuiltinFn fn);
Value callBuiltin(size_t index, std::vector<ValuePtr> args);
```

**Parameter Handling:**
- Automatic type conversion and validation
- Reference vs value parameter semantics
- Error handling and exception propagation
- Performance optimization for hot paths

### 3.7 Scheduler Interaction

**Integration Points:**
- Yield points for cooperative scheduling
- Event loop integration for async operations
- Module loading coordination
- Resource management and cleanup

**Scheduling Model:**
- Cooperative multitasking with explicit yield points
- Priority-based task scheduling
- Resource-aware scheduling decisions
- Deadlock prevention via timeout mechanisms

### 3.8 Allocation Strategy

**Arena Allocation:**
- Per-frame arena for temporary objects
- Automatic cleanup on frame exit
- Zero-fragmentation allocation pattern
- Fast allocation with minimal overhead

**Heap Allocation:**
- Reference-counted objects for sharing
- Explicit lifetime management
- Optional garbage collection integration
- Memory pool optimization for common types

**Memory Pools:**
- String interning pools
- Small object allocation pools
- Large object direct allocation
- Platform-specific optimizations

### 3.9 GC or Ownership Model

**Primary Model: Explicit Ownership**
- Reference counting for shared objects
- Explicit deallocation for owned objects
- Arena allocation for temporary values
- No automatic garbage collection by default

**Optional GC Integration:**
- Mark-sweep collector for specific use cases
- Incremental collection support
- Generational collection optimization
- Conservative stack scanning

**Ownership Rules:**
- Unique ownership for mutable objects
- Shared ownership for immutable data
- Explicit lifetime for external resources
- Automatic cleanup for scoped objects

### 3.10 Safety Boundaries

**Type Safety:**
- Static type checking at compile time
- Runtime type validation for dynamic operations
- Safe type conversions with explicit casting
- Type tag verification for variant access

**Memory Safety:**
- Bounds checking for array access
- Null pointer validation for object access
- Stack overflow detection and prevention
- Memory leak detection in debug builds

**Execution Safety:**
- Bytecode verification before execution
- Opcode validation and bounds checking
- Resource limit enforcement
- Exception handling for error recovery

### 3.11 Unsafe Execution Model

**Unsafe Blocks:**
- Explicit unsafe regions for system access
- Direct memory manipulation capabilities
- FFI integration for external libraries
- Performance optimization opportunities

**Safety Tradeoffs:**
- Unsafe blocks bypass runtime checks
- Direct pointer access and manipulation
- External resource management
- System-level API integration

**Use Cases:**
- High-performance numeric computations
- Direct hardware access
- External library integration
- System-level programming

### 3.12 FFI Execution Path

**FFI Interface:**
- C-compatible function binding
- Automatic parameter marshaling
- Exception handling across boundaries
- Resource management and cleanup

**Calling Convention:**
- Standard C calling convention
- Stack-based parameter passing
- Return value handling
- Error propagation mechanisms

**Integration Points:**
- Dynamic library loading
- Symbol resolution and binding
- Type conversion and validation
- Memory management coordination

## 4. RUNTIME CORE

### 4.1 Runtime Responsibilities

**Core Responsibilities:**
1. **Module Management:** Registration, loading, and lifecycle
2. **Scheduling:** Task scheduling and resource allocation
3. **Binding Layer:** Native function integration
4. **Resource Management:** Memory, file handles, external resources
5. **Event Handling:** Event loop and async operation coordination

**Secondary Responsibilities:**
- Debugging and profiling support
- Performance monitoring and optimization
- Error reporting and recovery
- Configuration and customization

### 4.2 Scheduler Architecture

**Scheduling Philosophy:**
Cooperative multitasking with explicit yield points. The scheduler prioritizes responsiveness and fairness over raw throughput.

**Scheduler Components:**
```cpp
class Scheduler {
    TaskQueue readyQueue;        // Ready to execute tasks
    TaskQueue waitingQueue;      // Waiting for events
    TaskQueue blockedQueue;      // Blocked on resources
    TimerQueue timerQueue;       // Timer-based tasks
    
    void schedule();             // Main scheduling loop
    void yield();                // Cooperative yield point
    void sleep(Duration d);      // Timed blocking
    void wait(Event& e);         // Event-based blocking
};
```

**Task Types:**
1. **Compute tasks:** CPU-bound operations
2. **I/O tasks:** File, network, device operations
3. **Timer tasks:** Delayed execution
4. **Event tasks:** Event-driven execution

**Scheduling Algorithm:**
- Priority-based preemptive scheduling
- Time quantum enforcement
- Resource-aware scheduling decisions
- Deadlock prevention and detection

### 4.3 Module Registry

**Registry Design:**
Centralized module registration with dependency tracking and lifecycle management.

**Registry Interface:**
```cpp
class ModuleRegistry {
    std::unordered_map<std::string, ModuleInfo> modules;
    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    
    bool registerModule(const std::string& name, ModuleInitFn init);
    ModuleInfo* getModule(const std::string& name);
    bool loadModule(const std::string& name);
    void unloadModule(const std::string& name);
};
```

**Module Lifecycle:**
1. **Registration:** Module declaration and dependency specification
2. **Loading:** Bytecode loading and verification
3. **Initialization:** Module setup and resource allocation
4. **Execution:** Normal operation and service provision
5. **Cleanup:** Resource deallocation and deregistration

**Dependency Management:**
- Topological sort for initialization order
- Circular dependency detection
- Version compatibility checking
- Hot reloading support (experimental)

### 4.4 Binding Layer

**Binding Philosophy:**
Thin, efficient binding layer with minimal overhead and maximum flexibility.

**Binding Types:**
1. **Function bindings:** Native function registration
2. **Type bindings:** Custom type integration
3. **Resource bindings:** External resource management
4. **Event bindings:** Event system integration

**Binding Interface:**
```cpp
class NativeBindingLayer {
    std::unordered_map<std::string, BuiltinFn> functions;
    std::unordered_map<std::string, TypeInfo> types;
    
    bool registerFunction(const std::string& name, BuiltinFn fn);
    bool registerType(const std::string& name, TypeInfo type);
    Value callFunction(const std::string& name, std::vector<Value> args);
};
```

**Performance Optimizations:**
- Function pointer caching
- Parameter type pre-validation
- Fast path for frequently called functions
- Inline cache for method resolution

### 4.5 Runtime Lifecycle

**Initialization Sequence:**
1. Runtime configuration validation
2. Core subsystem initialization (scheduler, registry, bindings)
3. Module discovery and registration
4. Dependency resolution and loading
5. Entry point identification and execution

**Execution Loop:**
```cpp
while (runtime.isRunning()) {
    scheduler.schedule();           // Task scheduling
    eventLoop.process();           // Event handling
    gc.collectIfNeeded();          // Garbage collection
    profiler.update();             // Performance monitoring
}
```

**Shutdown Sequence:**
1. Execution halt request
2. Module cleanup and deregistration
3. Resource deallocation and cleanup
4. Finalization and reporting

### 4.6 Boot Sequence

**Boot Stages:**
1. **Core Initialization:** Memory management, error handling
2. **Subsystem Setup:** Scheduler, registry, binding layer
3. **Module Discovery:** Scanning and registration
4. **Dependency Resolution:** Loading order determination
5. **System Startup:** Module initialization and service start

**Boot Configuration:**
- Runtime parameters and limits
- Module search paths
- Debugging and profiling options
- Security and permission settings

### 4.7 Update/Tick Loop

**Loop Design:**
Fixed-timestep loop with interpolation for smooth animation and consistent physics.

**Loop Structure:**
```cpp
const double FIXED_TIMESTEP = 1.0 / 60.0;  // 60 FPS
double accumulator = 0.0;

while (runtime.isRunning()) {
    double deltaTime = timer.getDelta();
    accumulator += deltaTime;
    
    while (accumulator >= FIXED_TIMESTEP) {
        update(FIXED_TIMESTEP);    // Fixed timestep update
        accumulator -= FIXED_TIMESTEP;
    }
    
    double alpha = accumulator / FIXED_TIMESTEP;
    render(alpha);                 // Interpolated render
}
```

**Update Phases:**
1. **Input Processing:** User input and event handling
2. **Logic Update:** Game logic and simulation
3. **Physics Update:** Physics simulation and collision
4. **Audio Update:** Audio system processing
5. **Network Update:** Network synchronization

### 4.8 Runtime Isolation Philosophy

**Isolation Principles:**
- Compile-time dependency enforcement
- Runtime module boundaries
- Resource isolation and protection
- Error containment and recovery

**Isolation Mechanisms:**
- Include firewall validation
- Build matrix testing
- Module sandboxing
- Resource quota enforcement

**Benefits:**
- Independent module development
- Parallel testing and validation
- Controlled failure propagation
- Clear architectural boundaries

## 5. MODULE SYSTEM

### 5.1 Runtime Modules

**Module Definition:**
Self-contained units of functionality with explicit interfaces and dependencies.

**Module Characteristics:**
- Explicit dependency declaration
- Well-defined public interface
- Internal implementation hiding
- Lifecycle management support

**Module Types:**
1. **Core Modules:** Essential runtime services
2. **System Modules:** Operating system integration
3. **Application Modules:** Domain-specific functionality
4. **Plugin Modules:** Optional extensions

### 5.2 Registration System

**Registration Process:**
```cpp
// Module declaration
ModuleInfo moduleInfo = {
    .name = "ecs",
    .version = "1.0.0",
    .dependencies = {"runtime_core"},
    .initFn = ecs_module_init,
    .cleanupFn = ecs_module_cleanup
};

// Module registration
runtime.registerModule(moduleInfo);
```

**Registration Requirements:**
- Unique module name and version
- Explicit dependency list
- Initialization and cleanup functions
- Interface specification

**Validation Steps:**
- Dependency availability checking
- Version compatibility verification
- Interface compliance validation
- Resource requirement assessment

### 5.3 Dynamic vs Static Modules

**Static Modules:**
- Compiled into the main executable
- Available at startup
- No runtime loading overhead
- Tight integration with core systems

**Dynamic Modules:**
- Loaded from shared libraries
- Available at runtime
- Hot reloading support
- Independent development cycles

**Tradeoffs:**
- **Static:** Performance, integration, simplicity
- **Dynamic:** Flexibility, modularity, independent deployment

### 5.4 Dependency Constraints

**Constraint Types:**
1. **Hard Dependencies:** Required for operation
2. **Soft Dependencies:** Optional enhancements
3. **Version Constraints:** Compatible version ranges
4. **Platform Constraints:** Platform-specific requirements

**Constraint Resolution:**
- Dependency graph construction
- Topological sorting for load order
- Conflict detection and resolution
- Satisfaction verification

**Constraint Enforcement:**
- Compile-time validation where possible
- Runtime verification for dynamic modules
- Error reporting for unsatisfied constraints
- Graceful degradation for optional dependencies

### 5.5 Isolation Guarantees

**Compile-Time Isolation:**
- Include firewall enforcement
- Dependency direction validation
- Build matrix testing
- Legacy quarantine enforcement

**Runtime Isolation:**
- Module namespace separation
- Resource quota enforcement
- Error containment boundaries
- Communication through explicit interfaces

**Memory Isolation:**
- Separate memory arenas where possible
- Controlled sharing through interfaces
- Resource leak prevention
- Garbage collection isolation

### 5.6 Module Lifecycle

**Lifecycle States:**
1. **Registered:** Module declared but not loaded
2. **Loading:** Module bytecode being loaded
3. **Loaded:** Module ready for initialization
4. **Initializing:** Module setup in progress
5. **Active:** Module fully operational
6. **Deactivating:** Module cleanup in progress
7. **Unloaded:** Module removed from runtime

**State Transitions:**
```cpp
enum class ModuleState {
    Registered,
    Loading,
    Loaded,
    Initializing,
    Active,
    Deactivating,
    Unloaded,
    Error
};
```

**Lifecycle Events:**
- Registration events
- Loading progress notifications
- Initialization completion callbacks
- Error and failure notifications

### 5.7 ECS as Optional Module

**ECS Module Design:**
The Entity Component System is implemented as an optional module that can be loaded when needed.

**Module Interface:**
```cpp
// ECS module public interface
extern "C" {
    bool ecs_module_init(ModuleContext* ctx);
    void ecs_module_cleanup();
    World* ecs_create_world();
    void ecs_destroy_world(World* world);
    Entity ecs_create_entity(World* world);
    void ecs_destroy_entity(World* world, Entity entity);
}
```

**Integration Points:**
- Runtime scheduler integration
- Memory management coordination
- Event system integration
- Debugging and profiling support

**Benefits of Modularity:**
- Independent ECS development
- Alternative ECS implementations
- Conditional loading for different use cases
- Clear separation of concerns

### 5.8 Graphics Modules

**Graphics Module Architecture:**
Pluggable graphics system with support for multiple rendering backends.

**Module Types:**
1. **OpenGL Module:** OpenGL rendering support
2. **Vulkan Module:** Modern Vulkan backend
3. **DirectX Module:** Windows-specific support
4. **Software Renderer:** Fallback software rendering

**Interface Abstraction:**
```cpp
class GraphicsDevice {
public:
    virtual void initialize() = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;
    virtual void shutdown() = 0;
};
```

**Module Selection:**
- Runtime capability detection
- User preference specification
- Performance-based selection
- Fallback chain determination

### 5.9 Future Plugin Architecture

**Plugin Goals:**
- Dynamic loading of third-party extensions
- Sandboxed execution environment
- Version compatibility management
- Hot reloading support

**Plugin Interface:**
```cpp
class PluginInterface {
public:
    virtual bool initialize(PluginContext* ctx) = 0;
    virtual void shutdown() = 0;
    virtual std::vector<std::string> getDependencies() = 0;
    virtual std::string getVersion() = 0;
};
```

**Security Considerations:**
- Capability-based security model
- Resource quota enforcement
- API access restrictions
- Audit logging and monitoring

## 6. ECS ARCHITECTURE

### 6.1 Why ECS Was Isolated

**Architectural Rationale:**
The Entity Component System was isolated as a separate module to:

1. **Maintain Core Purity:** The runtime core remains focused on execution and scheduling
2. **Enable Alternative Implementations:** Different ECS strategies can be swapped
3. **Conditional Loading:** ECS is not needed for all use cases
4. **Independent Development:** ECS can evolve independently of core runtime
5. **Testing Isolation:** ECS can be tested independently of other systems

**Benefits of Isolation:**
- Clear architectural boundaries
- Independent versioning and deployment
- Reduced core runtime complexity
- Flexibility for different application types

### 6.2 Entity Model

**Entity Design:**
Entities are lightweight identifiers that serve as handles to component collections.

**Entity Representation:**
```cpp
using Entity = uint32_t;  // Simple entity ID

struct EntityInfo {
    Entity id;                    // Unique identifier
    uint32_t version;             // Version for reuse safety
    std::vector<ComponentType> components;  // Component types
    bool alive;                   // Entity lifecycle state
};
```

**Entity Management:**
- Entity pool for efficient ID reuse
- Version tracking for stale reference detection
- Component type tracking for queries
- Lifecycle management (creation, destruction, recycling)

### 6.3 Component Storage

**Storage Strategy:**
Components are stored in contiguous arrays for cache efficiency, with separate storage per component type.

**Component Storage Design:**
```cpp
template<typename T>
class ComponentStorage {
    std::vector<T> components;        // Component data
    std::vector<Entity> entities;     // Entity mapping
    std::unordered_map<Entity, size_t> entityToIndex;  // Entity -> component index
    
public:
    T& getComponent(Entity entity);
    void addComponent(Entity entity, const T& component);
    void removeComponent(Entity entity);
    bool hasComponent(Entity entity) const;
};
```

**Storage Optimizations:**
- Cache-friendly contiguous storage
- Sparse storage for rare components
- Component pooling for allocation efficiency
- Type-erased storage for generic operations

### 6.4 System Execution

**System Design:**
Systems operate on entities with specific component combinations, implementing game logic and behavior.

**System Interface:**
```cpp
class System {
public:
    virtual void update(float deltaTime) = 0;
    virtual std::vector<ComponentType> getRequiredComponents() = 0;
    virtual void setWorld(World* world) = 0;
};

template<typename... Components>
class TypedSystem : public System {
    World* world;
    
public:
    void update(float deltaTime) override;
    std::vector<ComponentType> getRequiredComponents() override;
};
```

**System Scheduling:**
- Dependency-based system ordering
- Parallel system execution where possible
- Component access conflict detection
- Performance profiling and optimization

### 6.5 Scheduling Integration

**Integration Points:**
The ECS module integrates with the runtime scheduler for efficient system execution.

**Scheduling Strategy:**
```cpp
class ECSScheduler {
    std::vector<System*> systems;
    std::unordered_map<System*, std::vector<System*>> dependencies;
    
public:
    void addSystem(System* system);
    void addDependency(System* dependent, System* dependency);
    void update(float deltaTime);
    void parallelUpdate(float deltaTime);
};
```

**Parallel Execution:**
- System dependency analysis
- Component access conflict detection
- Worker thread distribution
- Synchronization point management

### 6.6 Ownership Rules

**Component Ownership:**
- Components are owned by entities
- Systems have read/write access during execution
- No shared ownership between systems
- Clear lifetime management

**Entity Ownership:**
- Entities are owned by the World
- Systems cannot destroy entities directly
- Deferred destruction for safety
- Reference counting for shared access

**Memory Management:**
- Component pools for allocation efficiency
- Automatic cleanup on entity destruction
- Memory fragmentation minimization
- Debug tracking for memory leaks

### 6.7 Runtime Interaction Boundaries

**Interface Boundaries:**
The ECS module interacts with the runtime through well-defined interfaces:

1. **Scheduler Integration:** System execution scheduling
2. **Memory Management:** Component allocation and deallocation
3. **Event System:** Component change notifications
4. **Debugging Support:** Entity and component inspection

**Communication Patterns:**
- Event-driven communication for loose coupling
- Direct API calls for performance-critical operations
- Callback registration for system notifications
- Query interfaces for data access

**Boundary Enforcement:**
- Interface versioning for compatibility
- API access validation
- Resource quota enforcement
- Error handling and propagation

### 6.8 Performance Strategy

**Cache Optimization:**
- Component data locality
- System execution ordering
- Memory access pattern optimization
- Prefetching strategies

**Parallelization:**
- System-level parallelism
- Component-level parallelism
- Worker thread utilization
- Lock-free data structures

**Memory Efficiency:**
- Component pooling
- Sparse storage for rare components
- Memory fragmentation reduction
- Garbage collection integration

### 6.9 Future Parallelism Goals

**Parallel System Execution:**
- Automatic dependency analysis
- Work-stealing scheduler integration
- Lock-free component access
- NUMA-aware memory allocation

**Component-Level Parallelism:**
- Component-wise parallel operations
- SIMD optimization for numeric components
- GPU acceleration for suitable operations
- Distributed processing for large worlds

**Scalability Targets:**
- Millions of entities
- Thousands of systems
- Multi-core processor utilization
- Distributed system support

## 7. SAFETY + SECURITY MODEL

### 7.1 Unsafe Blocks

**Unsafe Philosophy:**
Unsafe blocks are explicit, delimited regions where runtime safety checks are bypassed for performance or system access.

**Unsafe Block Syntax:**
```cpp
unsafe {
    // Direct memory access
    void* ptr = malloc(1024);
    memset(ptr, 0, 1024);
    
    // System calls
    int fd = open("file.txt", O_RDONLY);
    
    // Pointer arithmetic
    char* data = (char*)ptr + offset;
}
```

**Safety Tradeoffs:**
- **Performance:** Eliminates bounds checking overhead
- **Access:** Direct system API access
- **Control:** Fine-grained memory management
- **Risk:** Potential memory corruption and crashes

**Use Cases:**
- High-performance numerical computations
- Direct hardware interaction
- External library integration
- System-level programming

### 7.2 Trust Model

**Trust Boundaries:**
1. **Core Runtime:** Fully trusted, minimal surface area
2. **Standard Library:** Trusted but audited
3. **Third-Party Modules:** Partially trusted, sandboxed
4. **User Code:** Untrusted, heavily restricted

**Trust Levels:**
```cpp
enum class TrustLevel {
    Core,           // Full system access
    Standard,       // Most APIs available
    Restricted,     // Limited API access
    Sandbox         // Minimal API access
};
```

**Trust Enforcement:**
- Capability-based security model
- API access restrictions per trust level
- Resource quota enforcement
- Audit logging for privileged operations

### 7.3 Runtime Permissions

**Permission Categories:**
1. **File System:** File and directory access
2. **Network:** Socket and network operations
3. **System:** Process and system information
4. **Memory:** Memory allocation and mapping
5. **Hardware:** Device access and I/O operations

**Permission Model:**
```cpp
class PermissionSet {
    bool readFile;
    bool writeFile;
    bool networkAccess;
    bool systemInfo;
    bool hardwareAccess;
    
public:
    bool checkPermission(Permission p) const;
    void grantPermission(Permission p);
    void revokePermission(Permission p);
};
```

**Permission Enforcement:**
- Permission checking before privileged operations
- Capability-based access control
- Auditable permission grants
- Runtime permission revocation

### 7.4 FFI Restrictions

**FFI Safety:**
Foreign Function Interface is restricted to prevent unsafe interactions with external code.

**Restriction Categories:**
1. **Calling Convention:** Enforced C calling convention
2. **Parameter Validation:** Type checking and bounds validation
3. **Error Handling:** Exception translation and propagation
4. **Resource Management:** Automatic resource cleanup

**FFI Interface:**
```cpp
extern "C" {
    // Safe FFI function
    Value safe_function(VM* vm, std::vector<Value> args);
    
    // Unsafe FFI function (requires unsafe block)
    Value unsafe_function(void* ptr, size_t size);
}
```

**Validation Steps:**
- Function signature verification
- Parameter type checking
- Memory safety validation
- Resource leak prevention

### 7.5 Sandboxing Philosophy

**Sandbox Goals:**
- Contain the impact of malicious or buggy code
- Provide controlled access to system resources
- Enable safe execution of untrusted code
- Maintain performance for trusted code

**Sandbox Mechanisms:**
- API access restrictions
- Resource quota enforcement
- Memory isolation
- Audit logging

**Sandbox Boundaries:**
- Module-level sandboxing
- Function-level sandboxing
- Resource-level sandboxing
- Time-based sandboxing

### 7.6 Capability Boundaries

**Capability Model:**
Access to resources is granted through explicit capabilities that cannot be forged or escalated.

**Capability Types:**
```cpp
class FileCapability {
    std::string path;
    AccessMode mode;
    
public:
    bool canRead() const;
    bool canWrite() const;
    std::string getPath() const;
};

class NetworkCapability {
    std::string host;
    uint16_t port;
    Protocol protocol;
    
public:
    bool canConnect() const;
    std::string getHost() const;
};
```

**Capability Enforcement:**
- Capability-based access control
- Immutable capability objects
- Capability propagation tracking
- Capability revocation support

### 7.7 Security Assumptions

**Core Assumptions:**
1. **Core Runtime is Trusted:** The VM and runtime core are assumed to be correct
2. **Compiler is Correct:** Generated bytecode is assumed to be valid
3. **Hardware is Functional:** Underlying hardware is assumed to work correctly
4. **System APIs are Stable:** Operating system APIs are assumed to be stable

**Risk Mitigation:**
- Defense in depth strategies
- Redundant validation layers
- Failure mode analysis
- Comprehensive testing

**Known Limitations:**
- No protection against hardware bugs
- Limited protection against compiler bugs
- Dependent on operating system security
- Vulnerable to side-channel attacks

### 7.8 Tradeoffs Between Safety and Systems Access

**Safety Spectrum:**
```
Maximum Safety <-----> Maximum Performance
    |                     |
    v                     v
Managed Languages    Unsafe Languages
Garbage Collection    Manual Memory Management
Type Safety           Type Casting
Bounds Checking       Direct Memory Access
Exception Safety      Error Codes
```

**Kern's Position:**
Kern occupies a middle ground, providing:
- Safety by default with explicit unsafe opt-out
- Performance-critical paths with unsafe optimization
- Systems access when needed, sandboxed otherwise
- Gradual safety degradation for performance

**Decision Framework:**
- Use safe code for application logic
- Use unsafe code for performance-critical sections
- Use FFI for system integration
- Use sandboxing for untrusted code

## 8. BUILD SYSTEM + ARCHITECTURE FIREWALL

### 8.1 CMake Structure

**Build Philosophy:**
The CMake build system is designed to enforce architectural boundaries at compile time, preventing unauthorized dependencies.

**Directory Structure:**
```
kern/
├── CMakeLists.txt              # Root build configuration
├── kern/
│   ├── CMakeLists.txt          # Core library build
│   ├── core/
│   │   ├── CMakeLists.txt      # Value system build
│   ├── runtime/
│   │   ├── CMakeLists.txt      # Runtime core build
│   │   ├── vm/
│   │   │   ├── CMakeLists.txt  # VM build
│   │   ├── core/
│   │   │   ├── CMakeLists.txt  # Runtime core build
│   │   └── modules/
│   │       ├── ecs/
│   │       │   ├── CMakeLists.txt  # ECS module build
│   │       └── graphics/
│   │           ├── CMakeLists.txt  # Graphics module build
├── tools/
│   ├── architecture/
│   │   ├── CMakeLists.txt      # Architecture tools build
│   └── tests/
│       ├── CMakeLists.txt      # Test framework build
└── hosts/
    ├── CMakeLists.txt          # Host applications build
```

**Target Definitions:**
```cmake
# Core value system
add_library(core_value STATIC
    core/value.cpp
    core/value.hpp
)
target_include_directories(core_value PUBLIC core)

# Virtual machine
add_library(vm STATIC
    runtime/vm/vm.cpp
    runtime/vm/vm.hpp
    runtime/vm/builtins.hpp
)
target_include_directories(vm PRIVATE runtime/vm)
target_link_libraries(vm PRIVATE core_value)

# Runtime core
add_library(runtime_core STATIC
    runtime/core/scheduler.cpp
    runtime/core/module_registry.cpp
    runtime/core/binding_layer.cpp
)
target_include_directories(runtime_core PRIVATE runtime/core)
target_link_libraries(runtime_core PRIVATE core_value)

# ECS module
add_library(ecs_module STATIC
    runtime/modules/ecs/world.cpp
    runtime/modules/ecs/entity.cpp
    runtime/modules/ecs/system.cpp
)
target_include_directories(ecs_module PRIVATE runtime/modules/ecs)
target_link_libraries(ecs_module PRIVATE runtime_core)
```

### 8.2 Dependency Graph

**Allowed Dependencies:**
```
core_value (foundation)
├── vm
├── runtime_core
├── ecs_module
└── graphics_module

runtime_core
├── ecs_module
└── graphics_module

vm (isolated)
runtime_core (isolated)
ecs_module (isolated)
graphics_module (isolated)
```

**Forbidden Dependencies:**
- VM → runtime_core (VM must be standalone)
- ecs_module → vm (ECS must not depend on VM)
- graphics_module → ecs_module (Graphics must be independent)
- Any circular dependencies

**Dependency Enforcement:**
```cmake
# Include directory restrictions
target_include_directories(vm PRIVATE 
    ${CMAKE_SOURCE_DIR}/kern/core
    ${CMAKE_SOURCE_DIR}/kern/runtime/vm
)
# Explicitly forbid other includes

# Link restrictions
target_link_libraries(vm PRIVATE core_value)
# No other libraries allowed
```

### 8.3 Compile-Time Architecture Enforcement

**Enforcement Mechanisms:**
1. **Include Directory Control:** Explicit include directories per target
2. **Link Library Control:** Explicit library dependencies per target
3. **Preprocessor Validation:** Compile-time checks for forbidden includes
4. **Python Script Validation:** Automated dependency checking

**Validation Scripts:**
```python
# forbidden_includes.py
def check_file(filepath):
    includes = extract_includes(filepath)
    forbidden = get_forbidden_includes(filepath)
    
    for include in includes:
        if include in forbidden:
            report_violation(filepath, include)

def check_directory(dirpath):
    for filepath in find_cpp_files(dirpath):
        check_file(filepath)
```

**Build Integration:**
```cmake
# Custom target for architecture validation
add_custom_target(validate_architecture
    COMMAND python3 tools/architecture/forbidden_includes.py kern/
    COMMENT "Validating architecture compliance"
)
add_dependencies(validate_architecture build)
```

### 8.4 Include Firewall

**Firewall Rules:**
The include firewall prevents unauthorized cross-module includes through explicit directory control.

**Firewall Implementation:**
```cmake
# VM include firewall
target_include_directories(vm PRIVATE
    ${CMAKE_SOURCE_DIR}/kern/core      # Only core allowed
    ${CMAKE_SOURCE_DIR}/kern/runtime/vm  # Own directory
)

# Runtime core include firewall
target_include_directories(runtime_core PRIVATE
    ${CMAKE_SOURCE_DIR}/kern/core      # Only core allowed
    ${CMAKE_SOURCE_DIR}/kern/runtime/core  # Own directory
)

# ECS module include firewall
target_include_directories(ecs_module PRIVATE
    ${CMAKE_SOURCE_DIR}/kern/core      # Core allowed
    ${CMAKE_SOURCE_DIR}/kern/runtime/core  # Runtime core allowed
    ${CMAKE_SOURCE_DIR}/kern/runtime/modules/ecs  # Own directory
)
```

**Violation Detection:**
```python
# Example violation detection
def check_vm_includes(filepath):
    if filepath.startswith("runtime/vm/"):
        includes = extract_includes(filepath)
        for include in includes:
            if include.startswith("../core/"):
                continue  # Allowed
            elif include.startswith("runtime/vm/"):
                continue  # Allowed
            else:
                report_violation(filepath, include)
```

### 8.5 Build Matrix

**Matrix Definition:**
The build matrix defines isolation tests that prove module independence.

**Matrix Tests:**
```python
# build_matrix.py
def test_vm_isolation():
    # Test VM compilation without runtime
    result = compile_target("vm", ["core_value"])
    assert result.success, "VM should compile with only core_value"

def test_runtime_core_isolation():
    # Test runtime core compilation without VM
    result = compile_target("runtime_core", ["core_value"])
    assert result.success, "Runtime core should compile with only core_value"

def test_ecs_isolation():
    # Test ECS compilation without VM
    result = compile_target("ecs_module", ["core_value", "runtime_core"])
    assert result.success, "ECS should compile without VM"

def test_full_integration():
    # Test full system integration
    result = compile_target("full_system", ALL_MODULES)
    assert result.success, "Full system should compile successfully"
```

**Matrix Execution:**
```bash
# Build matrix execution
python3 tools/architecture/build_matrix.py

# Expected output
✓ VM isolation test passed
✓ Runtime core isolation test passed
✓ ECS module isolation test passed
✓ Graphics module isolation test passed
✓ Full system integration test passed
```

### 8.6 Legacy Quarantine

**Quarantine Purpose:**
The legacy quarantine isolates code that violates architectural rules but cannot be immediately fixed.

**Quarantine Implementation:**
```cmake
# Legacy quarantine target
add_library(legacy_quarantine STATIC
    legacy/old_code.cpp
    legacy/deprecated_api.cpp
)
target_include_directories(legacy_quarantine PRIVATE legacy)

# Quarantine validation (fails build on new violations)
add_custom_target(validate_legacy
    COMMAND python3 tools/architecture/check_legacy.py
    COMMENT "Checking legacy quarantine boundaries"
)
```

**Quarantine Rules:**
- Legacy code cannot include modern modules
- Modern modules cannot include legacy code
- Legacy violations are documented and tracked
- Gradual migration path from legacy to modern

### 8.7 Compiler Validation Philosophy

**Validation Principles:**
1. **Compiler Truth:** The compiler is the ultimate authority
2. **Explicit Dependencies:** All dependencies must be explicit
3. **Fail Fast:** Architectural violations fail the build
4. **Incremental Validation:** Validate each component independently

**Validation Strategy:**
- Compile-time dependency checking
- Link-time dependency verification
- Runtime isolation testing
- Automated validation pipeline

**Validation Benefits:**
- Architectural drift prevention
- Dependency documentation
- Refactoring safety
- Onboarding clarity

## 9. DIRECTORY STRUCTURE

### 9.1 `/kern` - Core Source Directory

**Purpose:** Contains all core source code organized by functional area.

**Structure:**
```
kern/
├── core/                   # Core value system and types
│   ├── value.hpp          # Value class and type system
│   ├── value.cpp          # Value implementation
│   └── types.hpp          # Core type definitions
├── runtime/               # Runtime subsystems
│   ├── vm/                # Virtual machine
│   │   ├── vm.hpp         # VM interface
│   │   ├── vm.cpp         # VM implementation
│   │   ├── builtins.hpp   # Built-in functions
│   │   └── bytecode.hpp   # Bytecode definitions
│   ├── core/              # Runtime core services
│   │   ├── scheduler.hpp  # Task scheduler
│   │   ├── module_registry.hpp  # Module management
│   │   └── binding_layer.hpp    # Native bindings
│   └── modules/           # Optional runtime modules
│       ├── ecs/           # Entity Component System
│       └── graphics/      # Graphics subsystem
└── compiler/              # Language compiler (future)
    ├── lexer.hpp          # Lexical analyzer
    ├── parser.hpp         # Parser
    └── codegen.hpp        # Code generator
```

**Ownership Rules:**
- **core:** Foundation, no dependencies on other kern modules
- **runtime/vm:** Isolated VM, depends only on core
- **runtime/core:** Runtime services, depends on core
- **runtime/modules:** Optional modules, depend on runtime/core

### 9.2 `/tools` - Development Tools

**Purpose:** Contains tools for architecture validation, testing, and development.

**Structure:**
```
tools/
├── architecture/          # Architecture enforcement tools
│   ├── forbidden_includes.py  # Include validation
│   ├── build_matrix.py        # Build matrix testing
│   └── allowed_dependencies.json  # Dependency rules
├── tests/                 # Test framework and utilities
│   ├── test_framework.hpp  # Test framework
│   ├── isolation_tests.cpp # Isolation test suite
│   └── performance_tests.cpp  # Performance benchmarks
└── scripts/               # Build and development scripts
    ├── build.sh           # Build script
    ├── test.sh            # Test script
    └── validate.sh        # Validation script
```

**Ownership Rules:**
- **tools/architecture:** Enforces architectural rules
- **tools/tests:** Validation and testing utilities
- **tools/scripts:** Development automation

### 9.3 `/tests` - Test Suite

**Purpose:** Comprehensive test suite for validation and regression testing.

**Structure:**
```
tests/
├── unit/                  # Unit tests
│   ├── core_tests.cpp     # Core value system tests
│   ├── vm_tests.cpp       # VM tests
│   └── runtime_tests.cpp  # Runtime core tests
├── integration/           # Integration tests
│   ├── module_tests.cpp   # Module integration tests
│   └── system_tests.cpp   # Full system tests
├── isolation/             # Isolation tests
│   ├── vm_isolation.cpp   # VM standalone test
│   ├── runtime_isolation.cpp  # Runtime core test
│   └── ecs_isolation.cpp  # ECS module test
└── performance/           # Performance tests
    ├── benchmarks.cpp     # Performance benchmarks
    └── memory_tests.cpp   # Memory usage tests
```

**Ownership Rules:**
- **tests/unit:** Component-level testing
- **tests/integration:** System integration testing
- **tests/isolation:** Architectural isolation validation
- **tests/performance:** Performance regression testing

### 9.4 `/hosts` - Host Applications

**Purpose:** Example applications and hosts for different use cases.

**Structure:**
```
hosts/
├── vm_only/               # VM-only host
│   ├── main.cpp           # Simple VM host
│   └── CMakeLists.txt     # Build configuration
├── full_runtime/          # Full runtime host
│   ├── main.cpp           # Complete runtime host
│   └── CMakeLists.txt     # Build configuration
├── cli/                   # Command-line interface
│   ├── main.cpp           # CLI tool
│   └── CMakeLists.txt     # Build configuration
└── editor/                # Editor integration
    ├── main.cpp           # Editor host
    └── CMakeLists.txt     # Build configuration
```

**Ownership Rules:**
- **hosts/vm_only:** Minimal VM demonstration
- **hosts/full_runtime:** Complete runtime example
- **hosts/cli:** Command-line tools
- **hosts/editor:** Editor integration examples

### 9.5 `/legacy` - Legacy Code Quarantine

**Purpose:** Isolates legacy code that violates architectural rules but cannot be immediately removed.

**Structure:**
```
legacy/
├── old_vm/                # Legacy VM implementation
│   ├── old_vm.hpp         # Old VM interface
│   └── old_vm.cpp         # Old VM implementation
├── deprecated_api/        # Deprecated API functions
│   ├── old_api.hpp        # Old API definitions
│   └── old_api.cpp        # Old API implementation
└── migration_guide.md     # Migration instructions
```

**Ownership Rules:**
- **legacy/**: No dependencies on modern modules
- **Modern modules**: No dependencies on legacy
- **Migration**: Gradual migration path documented

### 9.6 `/stdlib` - Standard Library

**Purpose:** Standard library functions and utilities for Kern programs.

**Structure:**
```
stdlib/
├── core/                  # Core library functions
│   ├── math.hpp           # Mathematical functions
│   ├── string.hpp         # String utilities
│   └── io.hpp             # I/O operations
├── collections/           # Data structures
│   ├── array.hpp          # Array operations
│   ├── map.hpp            # Map operations
│   └── set.hpp            # Set operations
└── platform/              # Platform-specific utilities
    ├── file.hpp           # File operations
    ├── network.hpp        # Network operations
    └── system.hpp         # System operations
```

**Ownership Rules:**
- **stdlib/core:** Core utilities, minimal dependencies
- **stdlib/collections:** Data structure implementations
- **stdlib/platform:** Platform-specific abstractions

### 9.7 `/docs` - Documentation

**Purpose:** Comprehensive documentation for developers and users.

**Structure:**
```
docs/
├── api/                   # API documentation
│   ├── core.md            # Core API reference
│   ├── vm.md              # VM API reference
│   └── runtime.md         # Runtime API reference
├── architecture/          # Architecture documentation
│   ├── overview.md        # System overview
│   ├── modules.md         # Module system
│   └── build_system.md    # Build system guide
├── tutorials/             # Tutorial content
│   ├── getting_started.md # Getting started guide
│   ├── vm_basics.md       # VM basics
│   └── module_dev.md      # Module development
└── examples/              # Example code
    ├── hello_world.kern   # Simple example
    ├── ecs_demo.kern      # ECS demonstration
    └── graphics_demo.kern # Graphics example
```

**Ownership Rules:**
- **docs/api:** Reference documentation
- **docs/architecture:** Design documentation
- **docs/tutorials:** Educational content
- **docs/examples:** Working examples

## 10. HOST APPLICATIONS

### 10.1 VM-Only Host

**Purpose:** Minimal host that demonstrates VM functionality without runtime overhead.

**Implementation:**
```cpp
// hosts/vm_only/main.cpp
int main(int argc, char* argv[]) {
    // Initialize VM
    VMConfig config;
    config.enableTracing = false;
    VM vm(config);
    
    // Load bytecode
    Bytecode code = load_bytecode(argv[1]);
    std::vector<std::string> stringPool = load_string_pool(argv[1]);
    std::vector<Value> valuePool = load_value_pool(argv[1]);
    
    // Execute
    auto result = vm.loadBytecode(code, stringPool, valuePool);
    if (!result) {
        std::cerr << "Failed to load bytecode: " << result.error() << std::endl;
        return 1;
    }
    
    Value output = vm.run();
    std::cout << "Result: " << vm.valueToString(output) << std::endl;
    
    return 0;
}
```

**Use Cases:**
- Script execution
- Language learning
- Performance testing
- Minimal deployment

### 10.2 Full Runtime Host

**Purpose:** Complete runtime host with all modules and services.

**Implementation:**
```cpp
// hosts/full_runtime/main.cpp
int main(int argc, char* argv[]) {
    // Initialize runtime
    RuntimeConfig config;
    config.enableECS = true;
    config.enableGraphics = true;
    config.enableNetworking = false;
    
    KernRuntime runtime(config);
    
    // Register modules
    runtime.registerModule("ecs", ecs_module_init);
    runtime.registerModule("graphics", graphics_module_init);
    
    // Load and execute main module
    auto result = runtime.loadModule("main");
    if (!result) {
        std::cerr << "Failed to load main module: " << result.error() << std::endl;
        return 1;
    }
    
    // Run main loop
    runtime.run();
    
    return 0;
}
```

**Use Cases:**
- Game development
- Interactive applications
- Full-featured programs
- Module demonstration

### 10.3 CLI Host

**Purpose:** Command-line interface for development and debugging.

**Implementation:**
```cpp
// hosts/cli/main.cpp
int main(int argc, char* argv[]) {
    CLIOptions options = parse_arguments(argc, argv);
    
    if (options.mode == "run") {
        return run_script(options.script);
    } else if (options.mode == "compile") {
        return compile_script(options.script, options.output);
    } else if (options.mode == "test") {
        return run_tests(options.test_suite);
    } else if (options.mode == "validate") {
        return validate_architecture();
    }
    
    print_usage();
    return 1;
}
```

**Commands:**
- `run <script>`: Execute a Kern script
- `compile <script> <output>`: Compile to bytecode
- `test <suite>`: Run test suite
- `validate`: Validate architecture compliance

### 10.4 Editor Host

**Purpose:** Integration with code editors for live development and debugging.

**Implementation:**
```cpp
// hosts/editor/main.cpp
class EditorHost {
    KernRuntime runtime;
    DebugServer debugServer;
    FileWatcher fileWatcher;
    
public:
    void initialize() {
        runtime.initialize();
        debugServer.start(1234);
        fileWatcher.watch("src/", [this](const std::string& file) {
            this->reloadModule(file);
        });
    }
    
    void reloadModule(const std::string& file) {
        runtime.hotReloadModule(file);
        debugServer.notifyReload(file);
    }
    
    void run() {
        while (running) {
            runtime.update();
            debugServer.processMessages();
            fileWatcher.processEvents();
        }
    }
};
```

**Features:**
- Hot module reloading
- Live debugging
- Code completion
- Error highlighting

### 10.5 Embedding Kern into External Apps

**Embedding API:**
```cpp
// Embedding interface
class KernEmbedding {
    KernRuntime* runtime;
    
public:
    bool initialize(const EmbeddingConfig& config);
    void executeScript(const std::string& script);
    Value callFunction(const std::string& name, std::vector<Value> args);
    void registerFunction(const std::string& name, BuiltinFn fn);
    void shutdown();
};
```

**Integration Steps:**
1. Initialize Kern runtime with embedding configuration
2. Register native functions for external API access
3. Load and execute user scripts
4. Handle events and callbacks
5. Shutdown and cleanup

**Use Cases:**
- Game engine scripting
- Application automation
- Plugin systems
- User customization

## 11. CURRENT STATUS

### 11.1 Structurally Complete Systems

**Core Value System:**
- ✅ Value class with variant storage
- ✅ Type system and safety checks
- ✅ Memory management and ownership
- ✅ Legacy compatibility layer
- ✅ Comprehensive test coverage

**Virtual Machine:**
- ✅ Register-window stack machine
- ✅ Direct-threaded dispatch
- ✅ Bytecode loading and verification
- ✅ Built-in function system
- ✅ Exception handling
- ✅ Module integration points

**Runtime Core:**
- ✅ Task scheduler
- ✅ Module registry
- ✅ Native binding layer
- ✅ Resource management
- ✅ Event system foundation

**Build System:**
- ✅ CMake configuration with dependency control
- ✅ Include firewall enforcement
- ✅ Build matrix validation
- ✅ Legacy quarantine system
- ✅ Automated architecture validation

### 11.2 Compiler-Validated Systems

**VM Isolation:**
- ✅ VM compiles standalone with only core dependency
- ✅ No forbidden includes detected
- ✅ Link-time dependency verification passed
- ✅ Build matrix isolation test passed

**Runtime Core Isolation:**
- ✅ Runtime core compiles without VM dependency
- ✅ Module registry independence verified
- ✅ Scheduler isolation confirmed
- ✅ Build matrix test passed

**ECS Module Isolation:**
- ✅ ECS module compiles without VM dependency
- ✅ Runtime core dependency verified
- ✅ Component storage independence confirmed
- ✅ Build matrix test passed

**Graphics Module Isolation:**
- ✅ Graphics module compiles independently
- ✅ No ECS dependency detected
- ✅ Runtime core integration verified
- ✅ Build matrix test passed

### 11.3 Runtime-Validated Systems

**Core Value System:**
- ✅ Value creation and manipulation
- ✅ Type safety and conversion
- ✅ Memory management correctness
- ✅ Performance benchmarks met

**Virtual Machine:**
- ✅ Bytecode execution
- ✅ Function calls and returns
- ✅ Built-in function execution
- ✅ Exception handling
- ⚠️ Performance optimization in progress

**Runtime Core:**
- ✅ Module loading and registration
- ✅ Task scheduling
- ✅ Native function binding
- ⚠️ Event system testing in progress

### 11.4 Experimental Systems

**Compiler Pipeline:**
- ⚠️ Lexer implementation complete
- ⚠️ Parser implementation in progress
- ❌ Bytecode generation not started
- ❌ Optimization passes not designed

**JIT Compilation:**
- ❌ JIT backend not designed
- ❌ Code generation not implemented
- ❌ Performance optimization not started

**Network Module:**
- ❌ Network module not designed
- ❌ Protocol implementation not started
- ❌ Security model not defined

### 11.5 Frozen Interfaces

**Core Value Interface:**
- ❌ Value class interface is stable
- ❌ Type system is frozen
- ❌ Memory management API is fixed

**VM Interface:**
- ❌ VM public interface is stable
- ❌ Bytecode format is frozen
- ❌ Built-in function API is fixed

**Runtime Core Interface:**
- ❌ Module registry API is stable
- ❌ Scheduler interface is frozen
- ❌ Binding layer API is fixed

### 11.6 Unresolved Technical Debt

**Legacy Compatibility:**
- ⚠️ Legacy type compatibility layer needs cleanup
- ⚠️ Built-in function system requires modernization
- ❌ Migration path from legacy types not defined

**Performance Optimization:**
- ⚠️ VM dispatch optimization in progress
- ⚠️ Memory allocation optimization needed
- ❌ SIMD optimization not implemented

**Testing Coverage:**
- ⚠️ Unit test coverage at 80%
- ⚠️ Integration test coverage at 60%
- ❌ Performance regression testing not implemented

## 12. CONTRIBUTION RULES

### 12.1 Architecture Freeze Rules

**Frozen Interfaces:**
- Core Value class interface is frozen
- VM public interface is frozen
- Runtime core interface is frozen
- Build system structure is frozen

**Modification Process:**
1. **Proposal:** Submit architecture change proposal
2. **Review:** Architecture committee review
3. **Impact Analysis:** Assess impact on dependent systems
4. **Approval:** Majority vote from core maintainers
5. **Implementation:** Update all dependent systems
6. **Validation:** Full build matrix validation

**Exception Process:**
- Critical bug fixes may bypass full process
- Security fixes require immediate attention
- Performance optimizations need benchmark validation
- Documentation updates require minimal review

### 12.2 Dependency Direction Rules

**Allowed Dependencies:**
```
core_value (foundation)
├── vm
├── runtime_core
├── ecs_module
└── graphics_module

runtime_core
├── ecs_module
└── graphics_module
```

**Forbidden Dependencies:**
- No circular dependencies
- VM cannot depend on runtime_core
- ECS cannot depend on VM
- Graphics cannot depend on ECS
- No module can depend on its own dependents

**Enforcement:**
- Compile-time include checking
- Link-time dependency verification
- Automated build matrix validation
- Manual code review for complex cases

### 12.3 Build Validation Requirements

**Pre-commit Validation:**
```bash
# Required validation steps
./tools/scripts/validate.sh
./tools/scripts/build.sh
./tools/scripts/test.sh
./tools/scripts/architecture_check.sh
```

**Continuous Integration:**
- Full build matrix validation
- Architecture compliance checking
- Performance regression testing
- Memory leak detection
- Code coverage requirements

**Release Criteria:**
- All tests must pass
- Architecture validation must succeed
- Performance benchmarks must meet targets
- Documentation must be updated
- Security review must be completed

### 12.4 Minimal-Fix Philosophy

**Fix Strategy:**
- Apply minimal changes to fix issues
- Avoid architectural changes unless necessary
- Prefer local fixes over global redesigns
- Document rationale for each change

**Change Categories:**
1. **Critical Fixes:** Security, stability, correctness
2. **Performance Fixes:** Optimization, memory usage
3. **Feature Fixes:** Missing functionality
4. **Documentation Fixes:** Clarity, completeness

**Fix Validation:**
- Reproduce issue before fixing
- Test fix with comprehensive test suite
- Validate no regression in other areas
- Document fix and its impact

### 12.5 Stabilization-Phase Constraints

**Current Phase:**
Kern is in stabilization phase, focusing on:
- Bug fixes and stability improvements
- Performance optimization
- Documentation completion
- Testing coverage improvement

**Constraints:**
- No new major features
- No architectural changes
- No breaking API changes
- Minimal dependency changes

**Exceptions:**
- Critical security fixes
- Essential performance optimizations
- Required bug fixes
- Documentation improvements

### 12.6 Forbidden Architectural Patterns

**Forbidden Patterns:**
1. **Circular Dependencies:** Any circular dependency between modules
2. **Global State:** Global variables or singletons (except for truly global services)
3. **Hidden Dependencies:** Implicit dependencies through shared headers
4. **Runtime Type Information:** Excessive RTTI usage
5. **Exception Specifications:** Function-level exception specifications
6. **Multiple Inheritance:** Complex inheritance hierarchies
7. **Friend Classes:** Excessive use of friend classes
8. **Macros for Code Generation:** Complex macro-based code generation

**Allowed Patterns:**
1. **Explicit Dependencies:** Clear, documented dependencies
2. **Interface Segregation:** Small, focused interfaces
3. **Dependency Injection:** Explicit dependency injection
4. **Composition over Inheritance:** Prefer composition
5. **RAII:** Resource acquisition is initialization
6. **Smart Pointers:** Explicit memory management
7. **Template Metaprogramming:** Compile-time computation

## 13. LONG-TERM ROADMAP

### 13.1 Runtime Stabilization

**Immediate Goals (0-3 months):**
- Complete VM performance optimization
- Finish runtime core testing
- Implement comprehensive error handling
- Complete memory management optimization

**Mid-term Goals (3-6 months):**
- Stabilize module system
- Implement hot reloading
- Complete debugging support
- Optimize garbage collection

**Long-term Goals (6-12 months):**
- Production-ready runtime
- Comprehensive tooling
- Performance benchmarking
- Documentation completion

### 13.2 Compiler Truth Milestones

**Milestone 1: Core Compiler (3 months)**
- Complete lexer implementation
- Finish parser implementation
- Implement basic bytecode generation
- Add semantic analysis

**Milestone 2: Optimizing Compiler (6 months)**
- Implement optimization passes
- Add advanced bytecode generation
- Implement JIT compilation basics
- Add debugging information generation

**Milestone 3: Production Compiler (12 months)**
- Complete compiler pipeline
- Implement advanced optimizations
- Add comprehensive error reporting
- Integrate with build system

### 13.3 Toolchain Goals

**Development Tools:**
- Complete IDE integration
- Implement debugging tools
- Add performance profilers
- Create visual debuggers

**Build Tools:**
- Improve build system performance
- Add dependency visualization
- Implement automated testing
- Create deployment tools

**Analysis Tools:**
- Static analysis tools
- Architecture validation tools
- Performance analysis tools
- Security analysis tools

### 13.4 Package Ecosystem

**Package Manager:**
- Design package format
- Implement package manager
- Create package repository
- Add version management

**Standard Library:**
- Complete core library
- Implement platform abstractions
- Add networking support
- Create graphics abstractions

**Third-Party Packages:**
- Package development guidelines
- Package validation tools
- Package repository infrastructure
- Community contribution process

### 13.5 JIT Possibilities

**JIT Backend Design:**
- Multi-backend JIT architecture
- Platform-specific optimizations
- Adaptive optimization strategies
- Profile-guided optimization

**Optimization Targets:**
- Hot path identification
- Function inlining
- Loop optimization
- Memory access optimization

**Implementation Phases:**
1. **Basic JIT:** Simple code generation
2. **Optimizing JIT:** Advanced optimizations
3. **Adaptive JIT:** Runtime optimization
4. **Specializing JIT:** Type specialization

### 13.6 Networking/Runtime Goals

**Network Module:**
- Design network architecture
- Implement protocol support
- Add security features
- Create network abstractions

**Distributed Runtime:**
- Design distributed execution model
- Implement node communication
- Add load balancing
- Create fault tolerance

**Web Integration:**
- WebAssembly backend
- Browser integration
- Network protocol support
- Security model adaptation

### 13.7 Editor/Runtime Integration

**Editor Integration:**
- Language server protocol
- Code completion
- Real-time error checking
- Debugging integration

**Live Development:**
- Hot module reloading
- Live debugging
- Performance profiling
- Visual debugging

**Collaboration Features:**
- Multi-user editing
- Shared debugging sessions
- Code review integration
- Documentation generation

### 13.8 Multi-Threading Plans

**Thread Safety:**
- Design thread-safe APIs
- Implement synchronization primitives
- Add concurrent data structures
- Create thread debugging tools

**Parallel Execution:**
- Parallel system execution
- Multi-threaded VM
- Concurrent garbage collection
- Parallel module loading

**Scalability:**
- NUMA-aware memory management
- Lock-free data structures
- Work-stealing schedulers
- Distributed execution

---

## CONCLUSION

Kern represents a deliberate approach to runtime system design, prioritizing architectural clarity, compiler validation, and modular isolation. The system is designed for systems programmers who need explicit control, predictable performance, and verifiable boundaries.

The current focus is on stabilization and validation, with the core runtime and VM systems approaching production readiness. The architecture is frozen and validated through comprehensive build matrix testing, ensuring that modular boundaries are enforced at compile time.

Future development will focus on completing the compiler pipeline, implementing JIT compilation, and building a comprehensive package ecosystem. The modular design ensures that these enhancements can be developed and validated independently while maintaining architectural integrity.

Kern's value lies in its uncompromising approach to architectural enforcement and its focus on systems programming needs. It provides a solid foundation for building modular, high-performance runtime systems with explicit boundaries and predictable behavior.
