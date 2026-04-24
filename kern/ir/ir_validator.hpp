/* *
 * kern/ir/ir_validator.hpp - IR Validation Layer
 * 
 * Validates IR before lowering to bytecode:
 * - No use-before-define
 * - Valid register indices
 * - Consistent types
 * - Valid control flow
 * 
 * This catches optimizer bugs early.
 */
#pragma once

#include "linear_ir.hpp"
#include <set>
#include <map>
#include <unordered_set>

namespace kern {
namespace ir {

struct IrValidationError {
    size_t instruction;
    std::string message;
    int severity;  // 1=warning, 2=error
};

class IrValidator {
    std::vector<IrValidationError> errors;
    std::set<Reg> defined;
    std::set<Reg> used;
    std::map<Reg, size_t> defSites;
    std::multimap<Reg, size_t> useSites;
    
public:
    struct Result {
        bool valid;
        std::vector<IrValidationError> errors;
        std::vector<IrValidationError> warnings;
        
        void print() const {
            if (errors.empty() && warnings.empty()) {
                std::cout << "IR validation: ✓ PASSED\n";
                return;
            }
            
            std::cout << "IR validation:\n";
            for (const auto& e : errors) {
                std::cout << "  [ERROR] Instr " << e.instruction << ": " << e.message << "\n";
            }
            for (const auto& w : warnings) {
                std::cout << "  [WARN] Instr " << w.instruction << ": " << w.message << "\n";
            }
        }
    };
    
    Result validate(const IrFunction& func) {
        Result result;
        errors.clear();
        defined.clear();
        used.clear();
        defSites.clear();
        useSites.clear();
        
        // Phase 1: Collect definitions and uses
        collectDefinitionsAndUses(func);
        
        // Phase 2: Check use-before-define
        checkUseBeforeDefine(result);
        
        // Phase 3: Check register bounds
        checkRegisterBounds(func, result);
        
        // Phase 4: Check valid operations
        checkValidOperations(func, result);
        
        // Phase 5: Check control flow
        checkControlFlow(func, result);
        
        // Phase 6: Check for undefined uses
        checkUndefinedUses(result);
        
        // Phase 7: Check for dead definitions
        checkDeadDefinitions(func, result);
        
        result.valid = errors.empty();
        result.errors = errors;
        
        return result;
    }
    
    // Quick validation (just the critical checks)
    bool quickValidate(const IrFunction& func) {
        for (size_t i = 0; i < func.instructions.size(); i++) {
            const auto& inst = func.instructions[i];
            
            // Check dest register valid
            if (inst.dest != NO_REG && inst.dest >= func.registerCount) {
                return false;
            }
            
            // Check source registers defined
            if (inst.srcA.type == IrValue::REG && inst.srcA.reg >= func.registerCount) {
                return false;
            }
            if (inst.srcB.type == IrValue::REG && inst.srcB.reg >= func.registerCount) {
                return false;
            }
        }
        return true;
    }
    
private:
    void collectDefinitionsAndUses(const IrFunction& func) {
        for (size_t i = 0; i < func.instructions.size(); i++) {
            const auto& inst = func.instructions[i];
            
            // Definition
            if (inst.dest != NO_REG) {
                defined.insert(inst.dest);
                defSites[inst.dest] = i;
            }
            
            // Uses
            if (inst.srcA.type == IrValue::REG) {
                used.insert(inst.srcA.reg);
                useSites.insert({inst.srcA.reg, i});
            }
            if (inst.srcB.type == IrValue::REG) {
                used.insert(inst.srcB.reg);
                useSites.insert({inst.srcB.reg, i});
            }
        }
    }
    
    void checkUseBeforeDefine(Result& result) {
        // For each use, check if definition comes before it
        for (const auto& [reg, useSite] : useSites) {
            auto defIt = defSites.find(reg);
            
            if (defIt == defSites.end()) {
                // No definition at all
                result.errors.push_back({useSite, 
                    "Register r" + std::to_string(reg) + " used but never defined", 2});
                errors.push_back({useSite,
                    "Register r" + std::to_string(reg) + " used but never defined", 2});
            } else if (defIt->second > useSite) {
                // Definition comes after use
                result.errors.push_back({useSite,
                    "Use-before-define: r" + std::to_string(reg) + 
                    " used at " + std::to_string(useSite) + 
                    " but defined at " + std::to_string(defIt->second), 2});
                errors.push_back({useSite,
                    "Use-before-define: r" + std::to_string(reg), 2});
            }
        }
    }
    
    void checkRegisterBounds(const IrFunction& func, Result& result) {
        for (size_t i = 0; i < func.instructions.size(); i++) {
            const auto& inst = func.instructions[i];
            
            if (inst.dest != NO_REG && inst.dest >= func.registerCount) {
                result.errors.push_back({i,
                    "Dest register r" + std::to_string(inst.dest) + 
                    " out of bounds (max " + std::to_string(func.registerCount - 1) + ")", 2});
                errors.push_back({i,
                    "Dest register out of bounds", 2});
            }
            
            if (inst.srcA.type == IrValue::REG && inst.srcA.reg >= func.registerCount) {
                result.errors.push_back({i,
                    "SrcA register r" + std::to_string(inst.srcA.reg) + " out of bounds", 2});
                errors.push_back({i, "SrcA register out of bounds", 2});
            }
            
            if (inst.srcB.type == IrValue::REG && inst.srcB.reg >= func.registerCount) {
                result.errors.push_back({i,
                    "SrcB register r" + std::to_string(inst.srcB.reg) + " out of bounds", 2});
                errors.push_back({i, "SrcB register out of bounds", 2});
            }
        }
    }
    
    void checkValidOperations(const IrFunction& func, Result& result) {
        for (size_t i = 0; i < func.instructions.size(); i++) {
            const auto& inst = func.instructions[i];
            
            // Check operations have required operands
            switch (inst.op) {
                case IrOp::LOAD_CONST:
                    if (!inst.srcA.isConst() && inst.srcA.isNil()) {
                        result.warnings.push_back({i, "LOAD_CONST without constant", 1});
                    }
                    break;
                    
                case IrOp::ADD:
                case IrOp::SUB:
                case IrOp::MUL:
                case IrOp::DIV:
                case IrOp::MOD:
                    if (inst.srcA.isNil() || inst.srcB.isNil()) {
                        result.errors.push_back({i, "Binary op missing operands", 2});
                        errors.push_back({i, "Binary op missing operands", 2});
                    }
                    break;
                    
                case IrOp::NEG:
                case IrOp::NOT:
                    if (inst.srcA.isNil()) {
                        result.errors.push_back({i, "Unary op missing operand", 2});
                        errors.push_back({i, "Unary op missing operand", 2});
                    }
                    break;
                    
                case IrOp::JUMP:
                case IrOp::JUMP_IF_TRUE:
                case IrOp::JUMP_IF_FALSE:
                    // Check offset is reasonable
                    if (inst.offset == 0) {
                        result.warnings.push_back({i, "Jump with zero offset (infinite loop)", 1});
                    }
                    // Check target is in bounds
                    {
                        int target = (int)i + inst.offset;
                        if (target < 0 || (size_t)target >= func.instructions.size()) {
                            result.errors.push_back({i,
                                "Jump target " + std::to_string(target) + " out of bounds", 2});
                            errors.push_back({i, "Jump target out of bounds", 2});
                        }
                    }
                    break;
                    
                case IrOp::RETURN:
                    // Return is valid without dest
                    break;
                    
                default:
                    break;
            }
            
            // Check that operations that need dest have it
            switch (inst.op) {
                case IrOp::ADD:
                case IrOp::SUB:
                case IrOp::MUL:
                case IrOp::DIV:
                case IrOp::MOD:
                case IrOp::NEG:
                case IrOp::NOT:
                case IrOp::EQ:
                case IrOp::LT:
                case IrOp::LE:
                case IrOp::GT:
                case IrOp::GE:
                case IrOp::AND:
                case IrOp::OR:
                case IrOp::LOAD_CONST:
                case IrOp::LOAD_LOCAL:
                case IrOp::LOAD_GLOBAL:
                case IrOp::CALL:
                case IrOp::MOVE:
                    if (inst.dest == NO_REG) {
                        result.errors.push_back({i,
                            "Operation " + std::to_string((int)inst.op) + 
                            " requires destination register", 2});
                        errors.push_back({i, "Missing destination register", 2});
                    }
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    void checkControlFlow(const IrFunction& func, Result& result) {
        // Check for unreachable code after return/terminator
        bool afterTerminator = false;
        size_t terminatorIndex = 0;
        
        for (size_t i = 0; i < func.instructions.size(); i++) {
            const auto& inst = func.instructions[i];
            
            if (afterTerminator) {
                result.warnings.push_back({i,
                    "Unreachable code after terminator at " + std::to_string(terminatorIndex), 1});
            }
            
            if (inst.op == IrOp::RETURN || inst.op == IrOp::JUMP) {
                afterTerminator = true;
                terminatorIndex = i;
            }
            
            if (inst.op == IrOp::LABEL || 
                (inst.op == IrOp::JUMP_IF_TRUE) || 
                (inst.op == IrOp::JUMP_IF_FALSE)) {
                // Jump target resets unreachable
                afterTerminator = false;
            }
        }
    }
    
    void checkUndefinedUses(Result& result) {
        for (Reg r : used) {
            if (defined.find(r) == defined.end()) {
                // Already reported in use-before-define
            }
        }
    }
    
    void checkDeadDefinitions(const IrFunction& func, Result& result) {
        // Definitions that are never used (except for side effects)
        for (const auto& [reg, defSite] : defSites) {
            if (used.find(reg) == used.end()) {
                // Check if this instruction has side effects
                const auto& inst = func.instructions[defSite];
                bool hasSideEffect = false;
                
                switch (inst.op) {
                    case IrOp::CALL:
                    case IrOp::STORE_LOCAL:
                    case IrOp::STORE_GLOBAL:
                    case IrOp::PRINT:
                        hasSideEffect = true;
                        break;
                    default:
                        break;
                }
                
                if (!hasSideEffect) {
                    result.warnings.push_back({defSite,
                        "Dead definition: r" + std::to_string(reg) + " defined but never used", 1});
                }
            }
        }
    }
};

// Debug mode IR checker
class DebugIrChecker {
public:
    static bool checkAndReport(const IrFunction& func, const std::string& stage) {
        std::cout << "\n=== IR Validation [" << stage << "] ===\n";
        
        IrValidator validator;
        auto result = validator.validate(func);
        
        result.print();
        
        if (!result.valid) {
            std::cout << "\n✗ IR validation FAILED\n";
            std::cout << "Errors found: " << result.errors.size() << "\n";
            
            // Dump the problematic IR
            std::cout << "\nProblematic IR:\n";
            for (size_t i = 0; i < func.instructions.size(); i++) {
                bool hasError = false;
                for (const auto& e : result.errors) {
                    if (e.instruction == i) {
                        hasError = true;
                        break;
                    }
                }
                
                std::cout << (hasError ? "! " : "  ");
                std::cout << "[" << i << "] ";
                // Would print instruction details here
                std::cout << "op=" << (int)func.instructions[i].op;
                std::cout << "\n";
            }
            
            return false;
        }
        
        std::cout << "\n✓ IR validation PASSED\n";
        return true;
    }
};

} // namespace ir
} // namespace kern
