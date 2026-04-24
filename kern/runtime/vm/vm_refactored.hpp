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

#include "core/value.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace kern {

// Forward declarations
class Allocator;
struct Bytecode;

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
    Result<Value> run();
    Result<Value> runFunction(const std::string& name, std::vector<Value> args);
    
    // Module system
    void registerModule(const std::string& name, ModuleInitFn init);
    Result<Value> importModule(const std::string& name);
    
    // Globals
    void setGlobal(const std::string& name, Value val);
    Value getGlobal(const std::string& name) const;
    bool hasGlobal(const std::string& name) const;
    
    // Debugging
    void setTracing(bool enabled);
    std::vector<std::string> getStackTrace() const;
    size_t getInstructionCount() const { return instructionCount; }
    
    // Emergency stop
    void requestStop();
    bool isStopped() const;

private:
    // Configuration
    VMConfig config;
    
    // Memory management
    std::unique_ptr<Allocator> allocator;
    
    // Execution state
    std::vector<Value> stack;
    std::vector<CallFrame> callStack;
    std::vector<ExceptionFrame> exceptionStack;
    std::vector<RegisterWindow> registerWindows;  // Register window stack
    size_t currentWindow;
    
    // Program state
    const Bytecode* currentCode;
    const std::vector<std::string>* stringPool;
    const std::vector<Value>* valuePool;
    const uint8_t* pc;  // Current instruction pointer
    
    // Globals
    std::unordered_map<std::string, Value> globals;
    
    // Modules
    std::unordered_map<std::string, ModuleInitFn> moduleRegistry;
    std::unordered_map<std::string, Value> moduleCache;
    
    // Execution control
    std::atomic<bool> stopRequested;
    uint64_t instructionCount;
    
    // Builtins
    std::vector<std::function<Value(VM*, std::vector<Value>&)>> builtins;
    
    // Internal execution
    Result<Value> execute();
    void push(const Value& val);
    Value pop();
    Value& top();
    
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
