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

namespace kern {

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
    
    // Missing methods
    const std::vector<std::string>& getCliArgs() const;
    size_t getCallStackDepth() const;
    std::vector<StackFrame> getCallStackSlice(size_t start = 0, size_t count = -1) const;
    void resetCycleCount();
    uint64_t getCycleCount() const;
    void setStepLimit(uint64_t limit);
    uint64_t getStepLimit() const;
    void setMaxCallDepth(size_t depth);
    size_t getMaxCallDepth() const;
    void setCallbackStepGuard(bool enabled);
    bool getCallbackStepGuard() const;
    size_t unsafeDepth() const;
    bool isInUnsafeContext() const;
    RuntimeGuardPolicy getRuntimeGuards() const;
    RuntimeGuardPolicy& mutableRuntimeGuards();
    
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
    bool builtinSlotFilled(size_t index) const;
    void runDeferredCalls();
    void runSubScript(Bytecode code, std::vector<std::string> stringConstants, std::vector<Value> valueConstants, const std::string& name);
    bool resumeGenerator(std::shared_ptr<GeneratorObject> gen, ValuePtr& out);
    void attachTracebackToError(ValuePtr val);
    void initBuiltins();
    
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
    std::shared_ptr<GeneratorObject> activeGenerator;
    bool vmTraceEnabled_ = false;
    
    ValuePtr runtimeGuards;
    std::vector<std::string> cliArgs;
    
    // Modules
    std::unordered_map<std::string, ModuleInitFn> moduleRegistry;
    std::unordered_map<std::string, Value> moduleCache;
    
    // Decorator registry
    ValuePtr decoratorRegistry;
    
    // Execution control
    std::atomic<bool> stopRequested;
    uint64_t instructionCount;
    
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
