/* *
 * kern/runtime/vm/vm.hpp - Refactored Virtual Machine
 * 
 * Features:
 * - Register-window stack machine (not pure stack)
 * - Direct-threaded dispatch (computed goto)
 * - Arena allocation for frames
 * - Bounds-checked operations
 * - Clean module API
 */
#pragma once

#include "../core/value.hpp"
#include "../core/bytecode/bytecode.hpp"
#include "../core/bytecode/script_code.hpp"
#include "../core/errors/errors.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <atomic>
#include <string>
#include <future>
#include <optional>

namespace kern {

// ── Heap allocation helpers for TaggedValue objects ──────────────────
// These allocate ObjHeader-based objects on the C heap. Declared here
// (not static) so they are accessible from builtins.cpp, module files,
// and any other translation unit that includes this header.
// Defined in vm.cpp.
// When vm is non-null, the new object is linked into the VM's intrusive
// firstObject_ linked list for GC tracking.
ObjString* allocObjString(const char* data, size_t len, VM* vm = nullptr);
ObjString* allocObjString(const std::string& s, VM* vm = nullptr);
ObjArray* allocObjArray(VM* vm = nullptr);
ObjArray* allocObjArray(const std::vector<TaggedValue>& elems, VM* vm = nullptr);
ObjMap* allocObjMap(VM* vm = nullptr);
ObjMap* allocObjMap(std::unordered_map<std::string, TaggedValue>&& entries, VM* vm = nullptr);
ObjClosure* allocObjClosure(FunctionObject* fn, VM* vm = nullptr);

// ─── Coroutine support for cooperative multi-tasking ────────────────────
enum class CoroutineState : uint8_t {
    RUNNING,  // Active or ready to resume
    YIELDED,  // Suspended, waiting for resume via resumeAll()
    DEAD      // Completed or errored
};

// ─────────────────────────────────────────────────────────────────────────

// Forward declarations
class Allocator;
struct ScriptCode;

struct RuntimeGuardPolicy {
    bool debugMode = true;
    bool allowUnsafe = true;              // All unsafe ops allowed
    bool enforcePermissions = false;      // No permission enforcement
    bool enforcePointerBounds = false;    // No pointer bounds checking
    bool ffiEnabled = true;               // FFI always enabled
    bool sandboxEnabled = false;          // No sandbox restrictions
    std::vector<std::string> ffiLibraryAllowlist;  // Ignored when sandbox disabled
    std::unordered_set<std::string> grantedPermissions;  // Ignored
};

// VMError exception class (simplified for compilation)
class VMError : public std::runtime_error {
public:
    VMError(const std::string& msg, int line = 0, int column = 0, int category = 0, int code = 0, int lineEnd = 0, int columnEnd = 0)
        : std::runtime_error(msg), line_(line), column_(column), lineEnd_(lineEnd), columnEnd_(columnEnd), category_(category), code_(code) {}
    int line_, column_, lineEnd_, columnEnd_, category_, code_;
};

// Constants
inline constexpr size_t kMaxCallStackSnapshotFrames = 256;
inline constexpr size_t kMaxStackSize = 65536;  // 64k values

// Register window frame - 16 registers per frame
// Window slides: 8 input, 8 local (overlapping with caller's outputs)
constexpr size_t REG_WINDOW_SIZE = 16;
constexpr size_t REG_INPUT_COUNT = 8;

struct RegisterWindow {
    Value regs[REG_WINDOW_SIZE];
    uint16_t pc;           // Program counter
    uint16_t funcIdx;      // Function index
    uint16_t callerRegs;   // Caller register base
    
    // Register access with bounds checking
    Value& get(size_t idx) {
        if (idx >= REG_WINDOW_SIZE) throw std::out_of_range("register index");
        return regs[idx];
    }
    const Value& get(size_t idx) const {
        if (idx >= REG_WINDOW_SIZE) throw std::out_of_range("register index");
        return regs[idx];
    }
};

// Stack frame for call stack
struct CallFrame {
    const uint8_t* returnPc;
    const RegisterWindow* callerWindow;
    size_t stackBase;
    std::string functionName;
    std::string filePath;
    uint32_t line;
    uint32_t column;
};

// Exception frame for try-catch
struct ExceptionFrame {
    const uint8_t* handlerPc;
    size_t stackBase;
    size_t frameCount;
    ValuePtr thrown;
    size_t catchIp;
    size_t stackMark;
    
    ExceptionFrame() = default;
    ExceptionFrame(const uint8_t* handler, size_t stack, size_t frames) 
        : handlerPc(handler), stackBase(stack), frameCount(frames), catchIp(0), stackMark(stack) {}
};

// ─── Coroutine struct (must be after CallFrame and ExceptionFrame) ────────
struct Coroutine {
    CoroutineState state = CoroutineState::RUNNING;

    // Execution state snapshot (mirrors VM's execution members)
    Bytecode code;
    std::vector<std::string> stringConstants;
    std::vector<Value> valueConstants;
    size_t ip = 0;
    std::vector<ValuePtr> locals;
    std::vector<ValuePtr> stack;
    std::vector<CallFrame> callFrames;
    std::vector<StackFrame> callStack;
    std::vector<std::vector<ValuePtr>> frameLocals;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> deferStack;
    std::vector<std::pair<ValuePtr, size_t>> iterStack;
    std::vector<size_t> tryStack;
    std::vector<ExceptionFrame> exceptionStack;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> codeFrameStack;
    std::shared_ptr<ScriptCode> currentScript;
    std::string activeSourcePath;
    int unsafeDepth = 0;
    ValuePtr yieldedValue;  // The value produced by the last yield
    uint64_t wakeTimestampMs = 0;  // Time (ms) before which this coroutine should NOT be resumed; 0 = no constraint
    std::optional<std::future<std::string>> pendingStringTask;  // Non-blocking string I/O future (e.g. fs_read_async)
};

// VM Configuration
struct VMConfig {
    size_t initialStackSize = 1024;
    size_t maxStackSize = 100000;
    size_t maxCallDepth = 1000;
    bool enableTracing = false;
    bool enableJit = false;  // Future
};

// Clean module interface - no VM internals exposed
class ModuleContext {
public:
    virtual ~ModuleContext() = default;
    
    // Value creation
    virtual Value makeInt(int64_t val) = 0;
    virtual Value makeFloat(double val) = 0;
    virtual Value makeString(const std::string& val) = 0;
    virtual Value makeArray() = 0;
    virtual Value makeMap() = 0;
    
    // Value inspection (safe, const only)
    virtual bool isTruthy(const Value& val) const = 0;
    virtual std::string valueToString(const Value& val) const = 0;
    
    // Error reporting
    virtual void reportError(uint32_t code, const std::string& message) = 0;
    
    // Stack trace
    virtual std::vector<std::string> getStackTrace() const = 0;
};

// Module registration interface
using ModuleInitFn = std::function<void(ModuleContext* ctx, std::unordered_map<std::string, Value>* exports)>;

// Main VM Class
class alignas(64) VM {
public:
    // Builtin function type
    using BuiltinFn = std::function<Value(VM*, std::vector<ValuePtr>)>;
    
    explicit VM(const VMConfig& config = {});
    ~VM();
    
    // Disable copy/move (VM owns resources)
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;
    VM(VM&&) = delete;
    VM& operator=(VM&&) = delete;
    
    // Bytecode loading and execution
    Result<void> loadBytecode(const Bytecode& code, 
                              const std::vector<std::string>& stringPool,
                              const std::vector<Value>& valuePool);
    void run();
    Result<Value> runFunction(const std::string& name, std::vector<Value> args);
    
    // Cooperative coroutine scheduling: resume all YIELDED coroutines
    // in a round-robin pass until all are DEAD.
    void resumeAll(uint64_t currentTimeMs = 0);

    /// Returns true if any coroutines are still alive (YIELDED or RUNNING state).
    /// The host can use this after run() to decide whether to loop with resumeAll().
    bool hasActiveCoroutines() const;

    /// Start a new coroutine from a function object, bypassing the generator
    /// call path (the codegen marks any yield-containing function as a generator,
    /// so a normal CALL would just create a GeneratorObject).  This method
    /// creates a fresh Coroutine entry and saves it as YIELDED so the next
    /// resumeAll() pass will pick it up.
    /// Returns the coroutine index.
    size_t startCoroutine(FunctionPtr fn, std::vector<ValuePtr> args);

    /// Yield the currently active coroutine for at least `ms` milliseconds.
    /// The coroutine will not be resumed by resumeAll() until the host clock
    /// (currentTimeMs) advances past wakeTime = storedClock + ms.
    /// Intended to be called from a native builtin (e.g. kern_sleep).
    void sleepCurrentCoroutine(uint64_t ms);

    /// Start an asynchronous file read on a background thread.
    /// The current coroutine will yield until the future completes.
    /// Called from the kern_fs_read_async builtin.
    void startAsyncFileRead(const std::string& path);

    /// Start an asynchronous HTTP GET on a background thread.
    /// The current coroutine will yield until the future completes.
    /// Called from the kern_http_get_async builtin.
    void startAsyncHttpGet(const std::string& url);

    /// Start an asynchronous HTTP POST on a background thread.
    /// The current coroutine will yield until the future completes.
    /// Called from the kern_http_post_async builtin.
    void startAsyncHttpPost(const std::string& url, const std::string& payload);

    /// Hot-reload the VM with new bytecode.  Kills all active coroutines
    /// (except coroutine 0 / the main thread), replaces the bytecode and
    /// constant pools, resets execution state, and re-runs the top-level
    /// script so that global function definitions are rebound.
    /// After this call, the host should resume the coroutine loop normally;
    /// the new top-level script may start fresh coroutines via
    /// kern_start_coroutine, which resumeAll() will pick up.
    void hotReload(const Bytecode& code,
                   const std::vector<std::string>& stringConstants,
                   const std::vector<Value>& valueConstants);

    // Encrypted bytecode loading (.knb with ChaCha20)
    bool loadBytecodeFromFile(const std::string& path);
    
    // Module system
    void registerModule(const std::string& name, ModuleInitFn init);
    Result<Value> importModule(const std::string& name);
    
    // Globals
    void setGlobal(const std::string& name, ValuePtr value);
    ValuePtr getGlobal(const std::string& name) const;
    std::unordered_map<std::string, ValuePtr> getGlobalsSnapshot() const;
    
    // Debugging
    void setTracing(bool enabled);
    std::vector<std::string> getStackTrace() const;
    size_t getInstructionCount() const { return instructionCount; }
    
    // Emergency stop
    void requestStop();
    bool isStopped() const;
    
    // Decorator registry for builtins
    void setDecoratorRegistry(ValuePtr registry);
    ValuePtr getDecoratorRegistry() const;
    
    // Runtime guards
    RuntimeGuardPolicy getRuntimeGuards() const;
    RuntimeGuardPolicy& mutableRuntimeGuards();
    void setRuntimeGuards(RuntimeGuardPolicy policy);

    // Missing methods
    const std::vector<std::string>& getCliArgs() const;
    void setCliArgs(std::vector<std::string> args);
    void setActiveSourcePath(const std::string& path);
    bool hasResult() const;
    size_t getCallStackDepth() const;
    std::vector<StackFrame> getCallStackSlice(size_t start = 0, size_t count = -1) const;
    void resetCycleCount();
    uint64_t getCycleCount() const;
    void setStepLimit(uint64_t limit);
    uint64_t getStepLimit() const;
    void setMaxCallDepth(size_t depth);
    size_t getMaxCallDepth() const;
    void setCallbackStepGuard(uint64_t enabled);
    uint64_t getCallbackStepGuard() const;
    size_t unsafeDepth() const;
    bool isInUnsafeContext() const;
    
    // Script exit code management
    void setScriptExitCode(int64_t code);
    int64_t getScriptExitCode() const;
    
    // Missing methods from vm.cpp
    void shutdownGlobalState();
    void setBytecode(Bytecode code);
    void verifyBytecodeOrThrow(const Bytecode& bc, size_t strPool, size_t valPool);
    void setInstructionPointer(size_t ip);
    void addBreakpoint(size_t pc);
    void removeBreakpoint(size_t pc);
    void clearBreakpoints();
    bool runNextInstruction();
    void runUntilBreakpoint();
    void setStringConstants(std::vector<std::string> constants);
    void setValueConstants(std::vector<Value> constants);
    void registerBuiltin(size_t index, BuiltinFn fn);
    // Helper for external registration code (vec3_builtins, collection_builtins, ffi_module):
    // Creates a FunctionObject with the given builtinIndex, keeps it alive, wraps it
    // in an ObjClosure, and stores it as a global with the given name.
    void registerBuiltinGlobal(const std::string& name, size_t builtinIndex);
    bool builtinSlotFilled(size_t index) const;
    void runDeferredCalls();
    void runSubScript(Bytecode code, std::vector<std::string> stringConstants, std::vector<Value> valueConstants, const std::string& name);
    bool resumeGenerator(GeneratorObject* gen, ValuePtr& out);
    void attachTracebackToError(ValuePtr val);
    void initBuiltins();
    
    // Garbage collection
    void collectGarbage();
    
    // Execution state restoration
    void restoreExecutionState(
        Bytecode code,
        std::vector<std::string> stringConstants,
        std::vector<Value> valueConstants,
        size_t ip,
        std::vector<ValuePtr> locals,
        std::vector<CallFrame> callFrames,
        std::vector<std::vector<ValuePtr>> frameLocals,
        std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> deferStack,
        std::vector<StackFrame> callStack,
        std::vector<std::pair<ValuePtr, size_t>> iterStack,
        std::vector<size_t> tryStack,
        std::vector<ExceptionFrame> exceptionStack,
        std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> codeFrameStack,
        std::shared_ptr<ScriptCode> currentScript,
        std::string activeSourcePath
    );
    
    // Value calling (for builtins)
    ValuePtr callValue(ValuePtr callee, const std::vector<ValuePtr>& args);
    // Coroutine scheduling helpers
    void saveCurrentCoroutineState();
    void restoreCoroutineState(const Coroutine& cor);
    
    // Missing method declarations from vm.cpp
    void runInstruction(const Instruction& inst);

    kern::ValuePtr getResult();
    std::string getOperandStr(const Instruction& inst);
    size_t getOperandU64(const Instruction& inst);

private:
    // Configuration
    VMConfig config;
    
    // Memory management
    std::unique_ptr<Allocator> allocator;
    
    // GC-ready intrusive linked list of all heap-allocated ObjHeader objects
    // (ObjString, ObjArray, ObjMap, ObjClosure, etc.).  Each allocObj* helper
    // links new objects into this list via registerObject().
    ObjHeader* firstObject_ = nullptr;
    
    // Gray stack for tri-color mark-and-sweep GC (worklist of reachable objects
    // whose children have not yet been traced).  Populated by markObject(),
    // drained by traceReferences().
    std::vector<ObjHeader*> grayStack_;
    
    // Execution state variables
    Bytecode code_;
    std::vector<std::string> stringConstants_;
    std::vector<Value> valueConstants_;
    size_t ip_ = 0;
    std::vector<ValuePtr> locals_;
    std::unordered_set<size_t> breakpoints_;
    std::unordered_map<size_t, BuiltinFn> builtins_;
    std::vector<BuiltinFn> builtinsVec_;
    int scriptExitCode_ = -1;
    std::string activeSourcePath_;
    int unsafeDepth_ = 0;
    ValuePtr pendingYieldValue_;
    bool pendingYield_ = false;
    bool doneGenerator_ = false;
    bool inGeneratorExecution_ = false;
    size_t maxCallDepth_ = 0;
    size_t callbackStepGuard_ = 0;
    uint64_t cycleCount_ = 0;
    uint64_t stepLimit_ = 0;
    std::unordered_map<std::string, ValuePtr> globals;
    std::vector<ValuePtr> stack;
    std::vector<CallFrame> callFrames;
    std::vector<StackFrame> callStack;
    std::vector<size_t> tryStack;
    std::vector<ExceptionFrame> exceptionStack;
    std::vector<std::vector<ValuePtr>> frameLocals_;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> deferStack;
    std::vector<std::pair<ValuePtr, size_t>> iterStack;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> codeFrameStack;
    std::shared_ptr<ScriptCode> currentScript;
    std::shared_ptr<ScriptCode> entryScriptCache;
    GeneratorObject* activeGenerator = nullptr;
    // Keep-alive vectors: TaggedValue stores raw pointers to heap objects.
    // These shared_ptrs prevent premature destruction.
    std::vector<std::shared_ptr<FunctionObject>> functionKeepAlive_;
    std::vector<std::shared_ptr<GeneratorObject>> generatorKeepAlive_;
    
    // Register a newly-allocated ObjHeader into the intrusive linked list
    // (prepends to firstObject_).  Called by the allocObj* helpers.
    // Public because the namespace-scope allocObj* free functions (declared
    // above in this header) need to call it from vm.cpp.
public:
    void registerObject(ObjHeader* obj);

private:
    
    // Walk the firstObject_ linked list, call proper C++ destructors for
    // each ObjHeader-derived type, then std::free() the allocation.
    void freeAllObjects();
    
    // ── Mark-and-Sweep Garbage Collector ──────────────────────────
    
    // Mark a single heap object (push onto the gray worklist).
    void markObject(ObjHeader* obj);
    
    // If val is a heap object (OBJ tag), extract the ObjHeader* and mark it.
    void markValue(ValuePtr val);
    
    // Walk all GC roots (stack, globals, locals, frameLocals, coroutines)
    // and mark every reachable heap object.
    void markRoots();
    
    // Process the gray worklist: for each gray object, mark its children
    // (Array elements, Map values, Closure captures).
    void traceReferences();
    
    // Walk the firstObject_ linked list, free unmarked objects, clear the
    // isMarked flag on survivors for the next cycle.
    void sweep();
    
    bool vmTraceEnabled_ = false;
    
    RuntimeGuardPolicy runtimeGuards_;
    std::vector<std::string> cliArgs;
    
    // Modules
    std::unordered_map<std::string, ModuleInitFn> moduleRegistry;
    std::unordered_map<std::string, Value> moduleCache;
    
    // Decorator registry
    ValuePtr decoratorRegistry;
    // Execution control
    std::atomic<bool> stopRequested;
    uint64_t instructionCount;
    // Coroutine scheduler state
    std::vector<Coroutine> coroutines_;
    size_t activeCoroutineId_ = 0;
    bool coroutineYieldRequested_ = false;
    uint64_t currentTimeMs_ = 0;  // Set by resumeAll() each tick; used by sleepCurrentCoroutine()


    
    // Builtins
    std::vector<BuiltinFn> builtins;
    
    // Internal execution
    Result<Value> execute();
    void push(ValuePtr v);
    ValuePtr peek();
    ValuePtr popStack();
    ValuePtr getResult() const;
    
    // Register window management
    void slideWindow();
    void restoreWindow();
    
    // Exception handling
    void throwException(const Value& val);
    bool unwindToHandler();
    
    // Module context implementation
    class ModuleContextImpl;
    std::unique_ptr<ModuleContextImpl> moduleContext;
    
    // Instruction dispatch table (direct-threaded)
    static void* dispatchTable[];
    
    // Opcode handlers
#define DEFINE_OP(name) void op_##name()
    DEFINE_OP(nop);
    DEFINE_OP(push_const);
    DEFINE_OP(push_nil);
    DEFINE_OP(push_true);
    DEFINE_OP(push_false);
    DEFINE_OP(pop);
    DEFINE_OP(dup);
    DEFINE_OP(load_local);
    DEFINE_OP(store_local);
    DEFINE_OP(load_global);
    DEFINE_OP(store_global);
    DEFINE_OP(load_upvalue);
    DEFINE_OP(store_upvalue);
    DEFINE_OP(load_field);
    DEFINE_OP(store_field);
    DEFINE_OP(load_index);
    DEFINE_OP(store_index);
    DEFINE_OP(add);
    DEFINE_OP(sub);
    DEFINE_OP(mul);
    DEFINE_OP(div);
    DEFINE_OP(mod);
    DEFINE_OP(pow);
    DEFINE_OP(neg);
    DEFINE_OP(not);
    DEFINE_OP(eq);
    DEFINE_OP(lt);
    DEFINE_OP(le);
    DEFINE_OP(gt);
    DEFINE_OP(ge);
    DEFINE_OP(jump);
    DEFINE_OP(jump_if_true);
    DEFINE_OP(jump_if_false);
    DEFINE_OP(call);
    DEFINE_OP(call_builtin);
    DEFINE_OP(return);
    DEFINE_OP(make_array);
    DEFINE_OP(make_map);
    DEFINE_OP(make_closure);
    DEFINE_OP(make_class);
    DEFINE_OP(throw);
    DEFINE_OP(try_begin);
    DEFINE_OP(try_end);
    DEFINE_OP(iter_begin);
    DEFINE_OP(iter_next);
    DEFINE_OP(iter_end);
    DEFINE_OP(import);
    DEFINE_OP(halt);
#undef DEFINE_OP
    
    // Helper methods
    void checkStack(size_t needed);
    void checkCallDepth();
    void checkWindowBounds();
    
    // Tracing
    void traceInstruction();
};

// RAII guard for VM operations
class VMGuard {
    VM& vm;
    size_t savedStack;
    size_t savedCallStack;
    
public:
    explicit VMGuard(VM& v);
    ~VMGuard();
    void commit();  // Don't restore on destruction
};

} // namespace kern
