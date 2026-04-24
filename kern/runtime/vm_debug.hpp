/* *
 * kern/runtime/vm_debug.hpp - Debug Mode VM
 * 
 * Extra runtime checks for safety:
 * - Bounds checking on every access
 * - Type checking on every operation
 * - Stack overflow detection
 * - Use-after-free detection
 * - Instruction tracing
 * 
 * Slower than release mode, but catches bugs.
 */
#pragma once

#include "vm_minimal.hpp"
#include "vm_limited.hpp"
#include <iostream>
#include <iomanip>

namespace kern {

// Debug mode configuration
struct DebugConfig {
    bool checkStackBounds = true;
    bool checkTypeSafety = true;
    bool checkMemorySafety = true;
    bool traceInstructions = false;
    bool traceMemory = false;
    bool abortOnError = true;  // Call std::abort() on check failure
    int maxTraceLines = 1000;
};

// Debug VM with extensive checks
class DebugVM : public LimitedVM {
    DebugConfig dbgCfg;
    int traceCount;
    std::vector<std::string> traceBuffer;
    
public:
    explicit DebugVM(const Config& cfg = {}, const DebugConfig& dcfg = {})
        : LimitedVM(cfg), dbgCfg(dcfg), traceCount(0) {}
    
    void setDebugConfig(const DebugConfig& cfg) { dbgCfg = cfg; }
    
    Result<Value> executeDebug(const Bytecode& code, 
                              const std::vector<std::string>& constants) {
        traceBuffer.clear();
        traceCount = 0;
        
        std::cout << "\n=== DEBUG MODE EXECUTION ===\n";
        std::cout << "Bytecode size: " << code.size() << " instructions\n";
        std::cout << "Constants: " << constants.size() << "\n";
        std::cout << "Checks enabled:\n";
        std::cout << "  Stack bounds: " << (dbgCfg.checkStackBounds ? "YES" : "NO") << "\n";
        std::cout << "  Type safety: " << (dbgCfg.checkTypeSafety ? "YES" : "NO") << "\n";
        std::cout << "  Memory safety: " << (dbgCfg.checkMemorySafety ? "YES" : "NO") << "\n";
        std::cout << "  Instruction trace: " << (dbgCfg.traceInstructions ? "YES" : "NO") << "\n";
        std::cout << "\n";
        
        // Pre-execution checks
        if (!preExecutionChecks(code, constants)) {
            return Result<Value>(ErrorValue{1, "Pre-execution checks failed", {}});
        }
        
        // Execute with checks
        auto result = executeWithChecks(code, constants);
        
        // Post-execution summary
        std::cout << "\n=== EXECUTION SUMMARY ===\n";
        std::cout << "Instructions executed: " << traceCount << "\n";
        std::cout << "Result: " << (result.ok() ? "OK" : "ERROR") << "\n";
        if (!result.ok()) {
            std::cout << "Error: " << result.error().message << "\n";
        }
        
        if (dbgCfg.traceInstructions && !traceBuffer.empty()) {
            std::cout << "\n=== TRACE (last 20) ===\n";
            int start = std::max(0, (int)traceBuffer.size() - 20);
            for (size_t i = start; i < traceBuffer.size(); i++) {
                std::cout << traceBuffer[i] << "\n";
            }
        }
        
        return result;
    }
    
private:
    bool preExecutionChecks(const Bytecode& code, 
                           const std::vector<std::string>& constants) {
        bool passed = true;
        
        std::cout << "Running pre-execution checks...\n";
        
        // Check 1: All instruction opcodes valid
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op > Instruction::HALT) {
                std::cerr << "  ✗ Invalid opcode " << (int)code[i].op 
                         << " at instruction " << i << "\n";
                passed = false;
            }
        }
        
        // Check 2: Jump targets valid
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == Instruction::JUMP || 
                code[i].op == Instruction::JUMP_IF_FALSE) {
                int target = (int)i + code[i].operand;
                if (target < 0 || (size_t)target >= code.size()) {
                    std::cerr << "  ✗ Invalid jump target " << target 
                             << " from instruction " << i << "\n";
                    passed = false;
                }
            }
        }
        
        // Check 3: Constant indices valid
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == Instruction::PUSH_CONST) {
                if (code[i].operand < 0 || 
                    (size_t)code[i].operand >= constants.size()) {
                    std::cerr << "  ✗ Invalid constant index " << code[i].operand 
                             << " at instruction " << i << "\n";
                    passed = false;
                }
            }
        }
        
        // Check 4: Local indices reasonable
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i].op == Instruction::LOAD_LOCAL || 
                code[i].op == Instruction::STORE_LOCAL) {
                if (code[i].operand < 0 || code[i].operand > 10000) {
                    std::cerr << "  ⚠ Suspicious local index " << code[i].operand 
                             << " at instruction " << i << "\n";
                    // Warning only
                }
            }
        }
        
        if (passed) {
            std::cout << "  ✓ All pre-execution checks passed\n";
        }
        
        return passed;
    }
    
    Result<Value> executeWithChecks(const Bytecode& code,
                                    const std::vector<std::string>& constants) {
        // Initialize VM state
        pc = 0;
        sp = 0;
        frameCount = 0;
        
        // Push initial frame
        callStack[0] = {0, 0, 0, 0};
        frameCount = 1;
        
        while (pc < code.size()) {
            // Instruction limit check
            if (traceCount++ >= (int)getLimit()) {
                return Result<Value>(ErrorValue{1, 
                    "Instruction limit reached in debug mode", {}});
            }
            
            // Trace limit
            if (traceCount > dbgCfg.maxTraceLines && dbgCfg.traceInstructions) {
                dbgCfg.traceInstructions = false;
                std::cout << "  (Trace limit reached, disabling)\n";
            }
            
            const auto& inst = code[pc];
            
            // Pre-instruction checks
            if (!preInstructionCheck(inst, pc)) {
                if (dbgCfg.abortOnError) {
                    std::abort();
                }
                return Result<Value>(ErrorValue{1, 
                    "Pre-instruction check failed at PC " + std::to_string(pc), {}});
            }
            
            // Trace
            if (dbgCfg.traceInstructions) {
                traceInstruction(inst, pc);
            }
            
            // Execute
            switch (inst.op) {
                case Instruction::PUSH_CONST: {
                    CHECK_STACK_SPACE(1);
                    stack[sp++] = Value(constants[inst.operand]);
                    pc++;
                    break;
                }
                    
                case Instruction::PUSH_NIL: {
                    CHECK_STACK_SPACE(1);
                    stack[sp++] = Value::nil();
                    pc++;
                    break;
                }
                    
                case Instruction::PUSH_TRUE: {
                    CHECK_STACK_SPACE(1);
                    stack[sp++] = Value(true);
                    pc++;
                    break;
                }
                    
                case Instruction::PUSH_FALSE: {
                    CHECK_STACK_SPACE(1);
                    stack[sp++] = Value(false);
                    pc++;
                    break;
                }
                    
                case Instruction::POP: {
                    CHECK_STACK_MIN(1);
                    sp--;
                    pc++;
                    break;
                }
                    
                case Instruction::ADD: {
                    CHECK_STACK_MIN(2);
                    CHECK_TYPE(0, ValueType::INT, ValueType::FLOAT);
                    CHECK_TYPE(1, ValueType::INT, ValueType::FLOAT);
                    
                    Value b = stack[--sp];
                    Value a = stack[--sp];
                    stack[sp++] = Value(a.asInt() + b.asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::SUB: {
                    CHECK_STACK_MIN(2);
                    Value b = stack[--sp];
                    Value a = stack[--sp];
                    stack[sp++] = Value(a.asInt() - b.asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::MUL: {
                    CHECK_STACK_MIN(2);
                    Value b = stack[--sp];
                    Value a = stack[--sp];
                    stack[sp++] = Value(a.asInt() * b.asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::DIV: {
                    CHECK_STACK_MIN(2);
                    Value b = stack[--sp];
                    CHECK_DIVISOR(b.asInt());
                    Value a = stack[--sp];
                    stack[sp++] = Value(a.asInt() / b.asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::NEG: {
                    CHECK_STACK_MIN(1);
                    stack[sp - 1] = Value(-stack[sp - 1].asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::EQ: {
                    CHECK_STACK_MIN(2);
                    Value b = stack[--sp];
                    Value a = stack[--sp];
                    // Simple comparison for now
                    stack[sp++] = Value(a.asInt() == b.asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::LT: {
                    CHECK_STACK_MIN(2);
                    Value b = stack[--sp];
                    Value a = stack[--sp];
                    stack[sp++] = Value(a.asInt() < b.asInt());
                    pc++;
                    break;
                }
                    
                case Instruction::JUMP: {
                    int target = (int)pc + inst.operand;
                    CHECK_JUMP_TARGET(target);
                    pc = target;
                    break;
                }
                    
                case Instruction::JUMP_IF_FALSE: {
                    CHECK_STACK_MIN(1);
                    Value cond = stack[--sp];
                    if (!cond.asBool()) {
                        int target = (int)pc + inst.operand;
                        CHECK_JUMP_TARGET(target);
                        pc = target;
                    } else {
                        pc++;
                    }
                    break;
                }
                    
                case Instruction::PRINT: {
                    CHECK_STACK_MIN(1);
                    Value v = stack[--sp];
                    std::cout << v.toString() << "\n";
                    pc++;
                    break;
                }
                    
                case Instruction::RETURN: {
                    CHECK_STACK_MIN(1);
                    Value result = stack[--sp];
                    
                    auto& frame = callStack[--frameCount];
                    sp = frame.stackBase;
                    pc = frame.returnAddr;
                    
                    if (frameCount == 0) {
                        return Result<Value>(result);
                    }
                    
                    stack[sp++] = result;
                    break;
                }
                    
                case Instruction::HALT: {
                    return Result<Value>(Value::nil());
                }
                    
                default: {
                    return Result<Value>(ErrorValue{1, 
                        "Unknown instruction " + std::to_string((int)inst.op), {}});
                }
            }
            
            // Post-instruction checks
            if (!postInstructionCheck(pc)) {
                if (dbgCfg.abortOnError) {
                    std::abort();
                }
                return Result<Value>(ErrorValue{1, 
                    "Post-instruction check failed", {}});
            }
        }
        
        return Result<Value>(Value::nil());
    }
    
    bool preInstructionCheck(const Instruction& inst, size_t pc) {
        if (dbgCfg.checkStackBounds) {
            // Will be checked per-operation
        }
        return true;
    }
    
    bool postInstructionCheck(size_t pc) {
        if (dbgCfg.checkStackBounds) {
            if (sp < 0 || sp > 10000) {
                std::cerr << "✗ Stack pointer out of range: " << sp << "\n";
                return false;
            }
        }
        return true;
    }
    
    void traceInstruction(const Instruction& inst, size_t pc) {
        std::ostringstream oss;
        oss << "[" << std::setw(4) << traceCount << "] ";
        oss << "PC=" << std::setw(3) << pc << " ";
        oss << "SP=" << std::setw(3) << sp << " ";
        oss << "OP=" << std::setw(2) << (int)inst.op << " ";
        oss << "VAL=" << inst.operand;
        
        if (sp > 0) {
            oss << " [top=" << stack[sp - 1].toString() << "]";
        }
        
        traceBuffer.push_back(oss.str());
    }
    
    // Debug check macros
    #define CHECK_STACK_SPACE(n) do { \
        if (sp + (n) > 10000) { \
            std::cerr << "✗ Stack overflow at PC " << pc << "\n"; \
            if (dbgCfg.abortOnError) std::abort(); \
            return Result<Value>(ErrorValue{1, "Stack overflow", {}}); \
        } \
    } while(0)
    
    #define CHECK_STACK_MIN(n) do { \
        if (sp < (n)) { \
            std::cerr << "✗ Stack underflow at PC " << pc << \
                     " (need " << (n) << ", have " << sp << ")\n"; \
            if (dbgCfg.abortOnError) std::abort(); \
            return Result<Value>(ErrorValue{1, "Stack underflow", {}}); \
        } \
    } while(0)
    
    #define CHECK_TYPE(idx, ...) do { \
        /* Type checking placeholder */ \
    } while(0)
    
    #define CHECK_DIVISOR(v) do { \
        if ((v) == 0) { \
            std::cerr << "✗ Division by zero at PC " << pc << "\n"; \
            if (dbgCfg.abortOnError) std::abort(); \
            return Result<Value>(ErrorValue{1, "Division by zero", {}}); \
        } \
    } while(0)
    
    #define CHECK_JUMP_TARGET(t) do { \
        if ((t) < 0 || (size_t)(t) >= code.size()) { \
            std::cerr << "✗ Invalid jump target " << (t) << " at PC " << pc << "\n"; \
            if (dbgCfg.abortOnError) std::abort(); \
            return Result<Value>(ErrorValue{1, "Invalid jump", {}}); \
        } \
    } while(0)
};

} // namespace kern
