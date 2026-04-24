/* *
 * kern/runtime/bytecode_verifier.hpp - Bytecode Safety Verifier
 * 
 * Verifies bytecode before execution to prevent:
 * - Stack underflow/overflow
 * - Invalid instruction sequences
 * - Out-of-bounds access
 * - Invalid jumps
 * 
 * This is security-critical: invalid bytecode must NEVER crash the VM.
 */
#pragma once

#include "runtime/vm_minimal.hpp"
#include <vector>
#include <string>
#include <functional>

namespace kern {

struct VerificationError {
    size_t pc;
    std::string message;
    int severity;  // 0=warning, 1=error, 2=critical
};

class BytecodeVerifier {
public:
    enum class Severity {
        WARNING = 0,
        ERROR = 1,
        CRITICAL = 2
    };
    
    struct Config {
        bool strictMode = true;           // Reject any questionable code
        bool allowStackGrowth = true;      // Allow dynamic stack growth
        size_t maxStackSize = 10000;       // Hard limit
        size_t maxLocalCount = 1000;       // Per-function limit
        bool validateAllPaths = true;      // Check all code paths
        bool checkUnreachable = true;       // Warn on unreachable code
    };
    
private:
    Config config;
    std::vector<VerificationError> errors;
    std::vector<VerificationError> warnings;
    
public:
    explicit BytecodeVerifier(Config cfg = {}) : config(cfg) {}
    
    // Main verification entry point
    bool verify(const Bytecode& code, const std::vector<std::string>& constants) {
        errors.clear();
        warnings.clear();
        
        if (code.empty()) {
            addError(0, "Empty bytecode", Severity::ERROR);
            return false;
        }
        
        // Phase 1: Basic instruction validation
        if (!validateInstructions(code)) {
            return false;
        }
        
        // Phase 2: Control flow validation (jumps are valid)
        if (!validateControlFlow(code)) {
            return false;
        }
        
        // Phase 3: Stack balance validation (simulated execution)
        if (!validateStackBalance(code)) {
            return false;
        }
        
        // Phase 4: Local variable bounds
        if (!validateLocalAccess(code)) {
            return false;
        }
        
        // Phase 5: Constant pool access
        if (!validateConstantAccess(code, constants)) {
            return false;
        }
        
        return errors.empty() || (!config.strictMode && maxSeverity() < Severity::CRITICAL);
    }
    
    // Get verification results
    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
    
    const std::vector<VerificationError>& getErrors() const { return errors; }
    const std::vector<VerificationError>& getWarnings() const { return warnings; }
    
    void printReport() const {
        for (const auto& w : warnings) {
            std::cerr << "[VERIFY-WARN] PC " << w.pc << ": " << w.message << std::endl;
        }
        for (const auto& e : errors) {
            std::cerr << "[VERIFY-ERROR] PC " << e.pc << " (sev " << e.severity << "): " 
                      << e.message << std::endl;
        }
    }
    
private:
    void addError(size_t pc, const std::string& msg, Severity sev) {
        if (sev == Severity::WARNING) {
            warnings.push_back({pc, msg, (int)sev});
        } else {
            errors.push_back({pc, msg, (int)sev});
        }
    }
    
    Severity maxSeverity() const {
        Severity max = Severity::WARNING;
        for (const auto& e : errors) {
            if (e.severity > (int)max) {
                max = (Severity)e.severity;
            }
        }
        return max;
    }
    
    // Phase 1: Validate each instruction is valid
    bool validateInstructions(const Bytecode& code) {
        bool valid = true;
        
        for (size_t pc = 0; pc < code.size(); pc++) {
            const auto& inst = code[pc];
            
            // Check opcode is valid
            if (inst.op > Instruction::HALT) {
                addError(pc, "Invalid opcode: " + std::to_string(inst.op), Severity::CRITICAL);
                valid = false;
                continue;
            }
            
            // Validate operands based on opcode
            switch (inst.op) {
                case Instruction::PUSH_CONST:
                    if (inst.operand < 0) {
                        addError(pc, "Negative constant index", Severity::ERROR);
                        valid = false;
                    }
                    break;
                    
                case Instruction::LOAD_LOCAL:
                case Instruction::STORE_LOCAL:
                    if (inst.operand < 0 || (size_t)inst.operand >= config.maxLocalCount) {
                        addError(pc, "Local index out of bounds: " + std::to_string(inst.operand), 
                                Severity::ERROR);
                        valid = false;
                    }
                    break;
                    
                case Instruction::LOAD_GLOBAL:
                case Instruction::STORE_GLOBAL:
                    if (inst.operand < 0) {
                        addError(pc, "Negative global index", Severity::ERROR);
                        valid = false;
                    }
                    break;
                    
                case Instruction::JUMP:
                case Instruction::JUMP_IF_FALSE:
                    // Jumps validated in phase 2
                    break;
                    
                case Instruction::CALL:
                    if (inst.operand < 0 || inst.operand > 255) {
                        addError(pc, "Invalid argument count: " + std::to_string(inst.operand), 
                                Severity::ERROR);
                        valid = false;
                    }
                    break;
                    
                default:
                    // Most instructions don't need operand validation
                    break;
            }
        }
        
        return valid;
    }
    
    // Phase 2: Validate all jumps target valid instructions
    bool validateControlFlow(const Bytecode& code) {
        bool valid = true;
        
        for (size_t pc = 0; pc < code.size(); pc++) {
            const auto& inst = code[pc];
            
            if (inst.op == Instruction::JUMP || inst.op == Instruction::JUMP_IF_FALSE) {
                int target = (int)pc + inst.operand;
                
                // Check target is within bounds
                if (target < 0 || (size_t)target >= code.size()) {
                    addError(pc, "Jump out of bounds: PC " + std::to_string(pc) + 
                            " + " + std::to_string(inst.operand) + " = " + std::to_string(target),
                            Severity::CRITICAL);
                    valid = false;
                    continue;
                }
                
                // Check target is not middle of multi-byte instruction
                // (Not applicable for our single-byte opcodes, but good practice)
                
                // Check target is not HALT (would skip cleanup)
                if (code[target].op == Instruction::HALT && inst.op == Instruction::JUMP) {
                    // This is actually fine - it's a valid jump to end
                }
            }
        }
        
        // Check for unreachable code
        if (config.checkUnreachable) {
            std::vector<bool> reachable(code.size(), false);
            markReachable(code, 0, reachable);
            
            for (size_t pc = 0; pc < code.size(); pc++) {
                if (!reachable[pc] && code[pc].op != Instruction::HALT) {
                    addError(pc, "Unreachable code detected", Severity::WARNING);
                }
            }
        }
        
        return valid;
    }
    
    void markReachable(const Bytecode& code, size_t pc, std::vector<bool>& reachable) {
        while (pc < code.size() && !reachable[pc]) {
            reachable[pc] = true;
            const auto& inst = code[pc];
            
            if (inst.op == Instruction::HALT || inst.op == Instruction::RETURN) {
                break;
            }
            
            if (inst.op == Instruction::JUMP) {
                pc = pc + inst.operand;
                continue;
            }
            
            if (inst.op == Instruction::JUMP_IF_FALSE) {
                // Mark both paths as reachable
                int target = (int)pc + inst.operand;
                if (target >= 0 && (size_t)target < code.size()) {
                    markReachable(code, target, reachable);
                }
            }
            
            pc++;
        }
    }
    
    // Phase 3: Validate stack doesn't underflow and doesn't grow unbounded
    bool validateStackBalance(const Bytecode& code) {
        // Simulate execution to track stack depth
        int stackDepth = 0;
        int maxDepth = 0;
        bool valid = true;
        
        for (size_t pc = 0; pc < code.size(); pc++) {
            const auto& inst = code[pc];
            
            // Check stack requirements
            int required = getStackPops(inst.op);
            int produced = getStackPushes(inst.op);
            
            if (stackDepth < required) {
                addError(pc, "Stack underflow: need " + std::to_string(required) + 
                        ", have " + std::to_string(stackDepth), Severity::CRITICAL);
                valid = false;
            }
            
            stackDepth -= required;
            stackDepth += produced;
            
            if (stackDepth > maxDepth) {
                maxDepth = stackDepth;
            }
            
            if ((size_t)stackDepth > config.maxStackSize) {
                addError(pc, "Stack overflow: depth " + std::to_string(stackDepth), 
                        Severity::CRITICAL);
                valid = false;
                break;
            }
        }
        
        // Final stack should be clean (0 or 1 for return value)
        if (stackDepth > 1) {
            addError(code.size() - 1, "Stack not clean at end: " + std::to_string(stackDepth) + 
                    " items remaining", Severity::WARNING);
        }
        
        return valid;
    }
    
    int getStackPops(Instruction::Op op) {
        switch (op) {
            case Instruction::POP: return 1;
            case Instruction::ADD:
            case Instruction::SUB:
            case Instruction::MUL:
            case Instruction::DIV:
            case Instruction::MOD:
            case Instruction::EQ:
            case Instruction::LT:
            case Instruction::LE:
            case Instruction::GT:
            case Instruction::GE: return 2;
            case Instruction::NEG:
            case Instruction::NOT:
            case Instruction::PRINT: return 1;
            case Instruction::STORE_LOCAL:
            case Instruction::STORE_GLOBAL: return 1;
            case Instruction::JUMP_IF_FALSE: return 1;
            case Instruction::CALL: return 1;  // Function
            case Instruction::RETURN: return 1;
            default: return 0;
        }
    }
    
    int getStackPushes(Instruction::Op op) {
        switch (op) {
            case Instruction::PUSH_CONST:
            case Instruction::PUSH_NIL:
            case Instruction::PUSH_TRUE:
            case Instruction::PUSH_FALSE: return 1;
            case Instruction::DUP: return 1;
            case Instruction::LOAD_LOCAL:
            case Instruction::LOAD_GLOBAL: return 1;
            case Instruction::ADD:
            case Instruction::SUB:
            case Instruction::MUL:
            case Instruction::DIV:
            case Instruction::MOD:
            case Instruction::NEG:
            case Instruction::EQ:
            case Instruction::LT:
            case Instruction::LE:
            case Instruction::GT:
            case Instruction::GE:
            case Instruction::NOT: return 1;
            case Instruction::CALL: return 1;  // Return value
            default: return 0;
        }
    }
    
    // Phase 4: Validate local variable indices
    bool validateLocalAccess(const Bytecode& code) {
        // Track max local index used
        int maxLocalUsed = -1;
        
        for (size_t pc = 0; pc < code.size(); pc++) {
            const auto& inst = code[pc];
            
            if (inst.op == Instruction::LOAD_LOCAL || inst.op == Instruction::STORE_LOCAL) {
                if (inst.operand > maxLocalUsed) {
                    maxLocalUsed = inst.operand;
                }
            }
        }
        
        // Warn if no locals used but instructions reference them
        if (maxLocalUsed >= 0 && (size_t)maxLocalUsed >= config.maxLocalCount) {
            addError(0, "Too many locals used: " + std::to_string(maxLocalUsed + 1), 
                    Severity::ERROR);
            return false;
        }
        
        return true;
    }
    
    // Phase 5: Validate constant pool access
    bool validateConstantAccess(const Bytecode& code, const std::vector<std::string>& constants) {
        bool valid = true;
        
        for (size_t pc = 0; pc < code.size(); pc++) {
            const auto& inst = code[pc];
            
            if (inst.op == Instruction::PUSH_CONST ||
                inst.op == Instruction::LOAD_GLOBAL ||
                inst.op == Instruction::STORE_GLOBAL) {
                
                if ((size_t)inst.operand >= constants.size()) {
                    addError(pc, "Constant index out of bounds: " + std::to_string(inst.operand) + 
                            " >= " + std::to_string(constants.size()), Severity::ERROR);
                    valid = false;
                }
            }
        }
        
        return valid;
    }
};

// RAII verifier wrapper
class VerifiedExecution {
    BytecodeVerifier verifier;
    
public:
    explicit VerifiedExecution(BytecodeVerifier::Config cfg = {}) : verifier(cfg) {}
    
    bool verifyAndExecute(MinimalVM& vm, const Bytecode& code, 
                         const std::vector<std::string>& constants,
                         Result<Value>& outResult) {
        if (!verifier.verify(code, constants)) {
            std::cerr << "Bytecode verification failed:" << std::endl;
            verifier.printReport();
            outResult = Result<Value>(ErrorValue{1, "Bytecode verification failed", {}});
            return false;
        }
        
        if (verifier.hasWarnings()) {
            verifier.printReport();
        }
        
        outResult = vm.execute(code);
        return true;
    }
};

} // namespace kern
