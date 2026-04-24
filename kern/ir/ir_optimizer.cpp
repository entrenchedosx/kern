/* *
 * kern/ir/ir_optimizer.cpp - IR Optimization Passes
 * 
 * Implements:
 * - Constant Folding
 * - Dead Code Elimination
 * - Copy Propagation
 * - Simple constant propagation
 */
#include "linear_ir.hpp"
#include <unordered_map>
#include <unordered_set>
#include <queue>

namespace kern {
namespace ir {

// ============================================================================
// Constant Folding
// ============================================================================

static IrValue foldUnary(IrOp op, const IrValue& val) {
    if (!val.isConst()) return IrValue();  // Can't fold
    
    switch (op) {
        case IrOp::NEG:
            if (val.type == IrValue::CONST_INT) {
                return IrValue(-val.intVal);
            } else if (val.type == IrValue::CONST_FLOAT) {
                return IrValue(-val.floatVal);
            }
            break;
        case IrOp::NOT:
            if (val.type == IrValue::CONST_BOOL) {
                return IrValue(!val.boolVal);
            }
            break;
        default:
            break;
    }
    return IrValue();
}

static IrValue foldBinary(IrOp op, const IrValue& left, const IrValue& right) {
    if (!left.isConst() || !right.isConst()) {
        return IrValue();  // Can't fold
    }
    
    // Arithmetic
    if (op == IrOp::ADD) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal + right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l + r);
        }
    }
    
    if (op == IrOp::SUB) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal - right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l - r);
        }
    }
    
    if (op == IrOp::MUL) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal * right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l * r);
        }
    }
    
    if (op == IrOp::DIV) {
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            if (r == 0) return IrValue();  // Division by zero
            return IrValue(l / r);
        }
    }
    
    if (op == IrOp::MOD) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            if (right.intVal == 0) return IrValue();
            return IrValue(left.intVal % right.intVal);
        }
    }
    
    // Comparisons
    if (op == IrOp::EQ) {
        if (left.type == right.type) {
            bool result = false;
            switch (left.type) {
                case IrValue::CONST_INT: result = left.intVal == right.intVal; break;
                case IrValue::CONST_FLOAT: result = left.floatVal == right.floatVal; break;
                case IrValue::CONST_BOOL: result = left.boolVal == right.boolVal; break;
                case IrValue::CONST_STRING: result = left.stringVal == right.stringVal; break;
                default: break;
            }
            return IrValue(result);
        }
    }
    
    if (op == IrOp::LT) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal < right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l < r);
        }
    }
    
    if (op == IrOp::LE) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal <= right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l <= r);
        }
    }
    
    if (op == IrOp::GT) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal > right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l > r);
        }
    }
    
    if (op == IrOp::GE) {
        if (left.type == IrValue::CONST_INT && right.type == IrValue::CONST_INT) {
            return IrValue(left.intVal >= right.intVal);
        }
        if ((left.type == IrValue::CONST_INT || left.type == IrValue::CONST_FLOAT) &&
            (right.type == IrValue::CONST_INT || right.type == IrValue::CONST_FLOAT)) {
            double l = (left.type == IrValue::CONST_INT) ? left.intVal : left.floatVal;
            double r = (right.type == IrValue::CONST_INT) ? right.intVal : right.floatVal;
            return IrValue(l >= r);
        }
    }
    
    // Logical
    if (op == IrOp::AND) {
        if (left.type == IrValue::CONST_BOOL && right.type == IrValue::CONST_BOOL) {
            return IrValue(left.boolVal && right.boolVal);
        }
    }
    
    if (op == IrOp::OR) {
        if (left.type == IrValue::CONST_BOOL && right.type == IrValue::CONST_BOOL) {
            return IrValue(left.boolVal || right.boolVal);
        }
    }
    
    return IrValue();
}

void IrOptimizer::foldConstants(IrFunction& func) {
    bool changed = true;
    int iterations = 0;
    const int MAX_ITERATIONS = 10;  // Prevent infinite loops
    
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        iterations++;
        
        for (auto& instr : func.instructions) {
            // Try to fold unary operations
            if (instr.op == IrOp::NEG || instr.op == IrOp::NOT) {
                IrValue folded = foldUnary(instr.op, instr.srcA);
                if (!folded.isNil()) {
                    instr.op = IrOp::LOAD_CONST;
                    instr.srcA = folded;
                    instr.srcB = IrValue();
                    changed = true;
                }
            }
            
            // Try to fold binary operations
            if (instr.op == IrOp::ADD || instr.op == IrOp::SUB || instr.op == IrOp::MUL ||
                instr.op == IrOp::DIV || instr.op == IrOp::MOD ||
                instr.op == IrOp::EQ || instr.op == IrOp::LT || instr.op == IrOp::LE ||
                instr.op == IrOp::GT || instr.op == IrOp::GE ||
                instr.op == IrOp::AND || instr.op == IrOp::OR) {
                
                IrValue folded = foldBinary(instr.op, instr.srcA, instr.srcB);
                if (!folded.isNil()) {
                    instr.op = IrOp::LOAD_CONST;
                    instr.srcA = folded;
                    instr.srcB = IrValue();
                    changed = true;
                }
            }
        }
    }
}

// ============================================================================
// Dead Code Elimination
// ============================================================================

// Track which registers are used
static std::unordered_set<Reg> findUsedRegisters(IrFunction& func) {
    std::unordered_set<Reg> used;
    
    // Mark all registers that are read
    for (const auto& instr : func.instructions) {
        // Source registers are used
        if (instr.srcA.type == IrValue::REG) {
            used.insert(instr.srcA.reg);
        }
        if (instr.srcB.type == IrValue::REG) {
            used.insert(instr.srcB.reg);
        }
        
        // Side-effect operations keep their dest alive
        if (instr.op == IrOp::CALL || instr.op == IrOp::RETURN ||
            instr.op == IrOp::STORE_LOCAL || instr.op == IrOp::STORE_GLOBAL ||
            instr.op == IrOp::PRINT || instr.op == IrOp::JUMP ||
            instr.op == IrOp::JUMP_IF_TRUE || instr.op == IrOp::JUMP_IF_FALSE ||
            instr.op == IrOp::SET_INDEX || instr.op == IrOp::SET_FIELD) {
            if (instr.dest != NO_REG) {
                used.insert(instr.dest);
            }
        }
    }
    
    return used;
}

// Check if instruction has side effects
static bool hasSideEffects(const IrInstr& instr) {
    switch (instr.op) {
        case IrOp::STORE_LOCAL:
        case IrOp::STORE_GLOBAL:
        case IrOp::STORE_UPVALUE:
        case IrOp::SET_INDEX:
        case IrOp::SET_FIELD:
        case IrOp::CALL:
        case IrOp::RETURN:
        case IrOp::PRINT:
        case IrOp::JUMP:
        case IrOp::JUMP_IF_TRUE:
        case IrOp::JUMP_IF_FALSE:
            return true;
        default:
            return false;
    }
}

void IrOptimizer::eliminateDeadCode(IrFunction& func) {
    bool changed = true;
    int iterations = 0;
    const int MAX_ITERATIONS = 5;
    
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        iterations++;
        
        auto used = findUsedRegisters(func);
        
        // Remove dead stores (dest not used, no side effects)
        auto it = func.instructions.begin();
        while (it != func.instructions.end()) {
            bool isDead = false;
            
            // Instruction is dead if:
            // 1. It writes to a register
            // 2. That register is never read
            // 3. It has no side effects
            if (it->dest != NO_REG && 
                used.find(it->dest) == used.end() &&
                !hasSideEffects(*it)) {
                isDead = true;
            }
            
            // Or: LOAD_CONST to dead register
            if (it->op == IrOp::LOAD_CONST && 
                it->dest != NO_REG &&
                used.find(it->dest) == used.end()) {
                isDead = true;
            }
            
            if (isDead) {
                it = func.instructions.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
}

// ============================================================================
// Copy Propagation
// ============================================================================

void IrOptimizer::eliminateCommonSubexpressions(IrFunction& func) {
    // Simple copy propagation: MOVE r1, r2 followed by use of r1 -> use r2
    std::unordered_map<Reg, IrValue> copies;  // dest -> source
    
    for (auto& instr : func.instructions) {
        // Apply existing copies to sources
        if (instr.srcA.type == IrValue::REG && copies.count(instr.srcA.reg)) {
            instr.srcA = copies[instr.srcA.reg];
        }
        if (instr.srcB.type == IrValue::REG && copies.count(instr.srcB.reg)) {
            instr.srcB = copies[instr.srcB.reg];
        }
        
        // Track new copies
        if (instr.op == IrOp::MOVE) {
            // Check if we're copying another copy (chain)
            IrValue src = instr.srcA;
            while (src.type == IrValue::REG && copies.count(src.reg)) {
                src = copies[src.reg];
            }
            copies[instr.dest] = src;
        }
        
        // Invalidate copies that depend on modified registers
        if (instr.dest != NO_REG && instr.op != IrOp::MOVE) {
            // Any copy whose source is this register is invalidated
            for (auto it = copies.begin(); it != copies.end();) {
                if ((it->second.type == IrValue::REG && it->second.reg == instr.dest)) {
                    it = copies.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        // Memory operations invalidate everything (conservative)
        if (hasSideEffects(instr)) {
            copies.clear();
        }
    }
    
    // Remove the MOVE instructions that are no longer needed
    // (this is done in dead code elimination)
}

// ============================================================================
// Strength Reduction
// ============================================================================

void IrOptimizer::strengthReduction(IrFunction& func) {
    for (auto& instr : func.instructions) {
        // x * 2 -> x + x  (not really better, but example)
        // Better: x * 2 -> x << 1 (when we have shift)
        
        // x ** 2 -> x * x (for small powers)
        // Not applicable without POW op
        
        // x * 0 -> 0
        if (instr.op == IrOp::MUL) {
            if (instr.srcA.type == IrValue::CONST_INT && instr.srcA.intVal == 0) {
                instr.op = IrOp::LOAD_CONST;
                instr.srcA = IrValue(0);
                instr.srcB = IrValue();
            } else if (instr.srcB.type == IrValue::CONST_INT && instr.srcB.intVal == 0) {
                instr.op = IrOp::LOAD_CONST;
                instr.srcA = IrValue(0);
                instr.srcB = IrValue();
            }
            // x * 1 -> x
            else if (instr.srcA.type == IrValue::CONST_INT && instr.srcA.intVal == 1) {
                instr.op = IrOp::MOVE;
                instr.srcA = instr.srcB;
                instr.srcB = IrValue();
            } else if (instr.srcB.type == IrValue::CONST_INT && instr.srcB.intVal == 1) {
                instr.op = IrOp::MOVE;
                instr.srcB = IrValue();
            }
        }
        
        // x + 0 -> x
        if (instr.op == IrOp::ADD) {
            if (instr.srcA.type == IrValue::CONST_INT && instr.srcA.intVal == 0) {
                instr.op = IrOp::MOVE;
                instr.srcA = instr.srcB;
                instr.srcB = IrValue();
            } else if (instr.srcB.type == IrValue::CONST_INT && instr.srcB.intVal == 0) {
                instr.op = IrOp::MOVE;
                instr.srcB = IrValue();
            }
        }
        
        // x - 0 -> x
        if (instr.op == IrOp::SUB) {
            if (instr.srcB.type == IrValue::CONST_INT && instr.srcB.intVal == 0) {
                instr.op = IrOp::MOVE;
                instr.srcB = IrValue();
            }
        }
        
        // x / 1 -> x
        if (instr.op == IrOp::DIV) {
            if (instr.srcB.type == IrValue::CONST_INT && instr.srcB.intVal == 1) {
                instr.op = IrOp::MOVE;
                instr.srcB = IrValue();
            }
        }
    }
}

// ============================================================================
// Main Optimize Function
// ============================================================================

void IrOptimizer::optimize(IrFunction& func) {
    // Run passes in order until no more changes
    bool changed = true;
    int iterations = 0;
    const int MAX_ITERATIONS = 5;
    
    while (changed && iterations < MAX_ITERATIONS) {
        size_t beforeSize = func.instructions.size();
        
        foldConstants(func);
        strengthReduction(func);  // New: run before DCE
        eliminateCommonSubexpressions(func);
        eliminateDeadCode(func);
        
        size_t afterSize = func.instructions.size();
        changed = (afterSize != beforeSize);
        iterations++;
    }
}

// Print optimization stats
void IrOptimizer::printStats(const IrFunction& before, const IrFunction& after) {
    std::cout << "Optimization results:" << std::endl;
    std::cout << "  Instructions: " << before.instructions.size() 
              << " -> " << after.instructions.size();
    
    if (before.instructions.size() > 0) {
        int reduction = (int)((1.0 - (double)after.instructions.size() / before.instructions.size()) * 100);
        std::cout << " (" << reduction << "% reduction)";
    }
    std::cout << std::endl;
    
    std::cout << "  Registers: " << before.registerCount 
              << " -> " << after.registerCount << std::endl;
}

} // namespace ir
} // namespace kern
