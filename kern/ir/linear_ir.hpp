/* *
 * kern/ir/linear_ir.hpp - Linear IR with Virtual Registers
 * 
 * Design goals:
 * - Simple linear sequence (no basic blocks for now)
 * - Virtual registers (unlimited, mapped to stack/registers later)
 * - Easy to optimize (constant fold, DCE, inline)
 * - Straightforward lowering to bytecode
 */
#pragma once

#include <vector>
#include <string>
#include <variant>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace kern {
namespace ir {

// Virtual register (just an index)
using Reg = uint32_t;
constexpr Reg NO_REG = UINT32_MAX;

// IR value - either a register or a constant
struct IrValue {
    enum Type { REG, CONST_INT, CONST_FLOAT, CONST_STRING, CONST_BOOL, NIL } type;
    
    union {
        Reg reg;
        int64_t intVal;
        double floatVal;
        bool boolVal;
    };
    std::string stringVal;  // For string constants
    
    IrValue() : type(NIL), reg(0) {}
    explicit IrValue(Reg r) : type(REG), reg(r) {}
    explicit IrValue(int64_t i) : type(CONST_INT), intVal(i) {}
    explicit IrValue(double f) : type(CONST_FLOAT), floatVal(f) {}
    explicit IrValue(const std::string& s) : type(CONST_STRING), stringVal(s) {}
    explicit IrValue(bool b) : type(CONST_BOOL), boolVal(b) {}
    
    bool isConst() const {
        return type == CONST_INT || type == CONST_FLOAT || 
               type == CONST_STRING || type == CONST_BOOL || type == NIL;
    }
    
    bool isNil() const { return type == NIL; }
    
    std::string toString() const;
};

// IR Instruction types
enum class IrOp : uint8_t {
    // Constants
    LOAD_CONST,      // dest = const
    LOAD_NIL,        // dest = nil
    LOAD_BOOL,       // dest = bool
    
    // Moves
    MOVE,            // dest = src
    
    // Arithmetic
    ADD,             // dest = left + right
    SUB,             // dest = left - right
    MUL,             // dest = left * right
    DIV,             // dest = left / right
    MOD,             // dest = left % right
    NEG,             // dest = -src
    
    // Comparison
    EQ,              // dest = left == right
    NE,              // dest = left != right
    LT,              // dest = left < right
    LE,              // dest = left <= right
    GT,              // dest = left > right
    GE,              // dest = left >= right
    
    // Logical
    NOT,             // dest = !src
    AND,             // dest = left && right
    OR,              // dest = left || right
    
    // Variables
    LOAD_LOCAL,      // dest = locals[idx]
    STORE_LOCAL,     // locals[idx] = src
    LOAD_GLOBAL,     // dest = globals[name]
    STORE_GLOBAL,    // globals[name] = src
    LOAD_UPVALUE,    // dest = upvalues[idx]
    STORE_UPVALUE,   // upvalues[idx] = src
    
    // Control flow
    JUMP,            // pc += offset
    JUMP_IF_TRUE,    // if (cond) pc += offset
    JUMP_IF_FALSE,   // if (!cond) pc += offset
    
    // Calls
    CALL,            // dest = func(args...)
    RETURN,          // return src (or nil)
    
    // Collections
    MAKE_ARRAY,      // dest = []
    MAKE_MAP,        // dest = {}
    GET_INDEX,       // dest = base[index]
    SET_INDEX,       // base[index] = src
    GET_FIELD,       // dest = base.field
    SET_FIELD,       // base.field = src
    
    // Phi for SSA (future)
    PHI,             // dest = phi(alternatives...)
    
    // Debug
    PRINT,           // print src
    
    // Special
    NOP,             // no-op
};

// Single IR instruction
struct IrInstr {
    IrOp op;
    Reg dest;           // Destination register (NO_REG if none)
    IrValue srcA;       // First operand
    IrValue srcB;       // Second operand (optional)
    int16_t offset;     // For jumps
    std::string name;   // For globals/fields
    
    IrInstr(IrOp o, Reg d = NO_REG, IrValue a = IrValue(), IrValue b = IrValue())
        : op(o), dest(d), srcA(a), srcB(b), offset(0) {}
    
    std::string toString() const;
};

// Function IR
struct IrFunction {
    std::string name;
    std::vector<IrInstr> instructions;
    uint32_t registerCount = 0;     // Total virtual registers used
    uint32_t paramCount = 0;        // Number of parameters
    uint32_t localCount = 0;        // Number of locals
    std::vector<std::string> upvalues;
    std::vector<std::string> constants;  // String constants referenced
    
    // Allocate a new virtual register
    Reg allocReg() { return registerCount++; }
    
    // Add instruction
    void emit(IrInstr instr) {
        instructions.push_back(std::move(instr));
    }
    
    // Size
    size_t size() const { return instructions.size(); }
    
    // Print for debugging
    void dump() const;
};

// Scope tracking for correct variable resolution
class Scope {
    Scope* parent;
    std::unordered_map<std::string, Reg> locals;
    std::unordered_map<std::string, uint32_t> localIndices;
    int nextLocalIdx;
    
public:
    explicit Scope(Scope* p = nullptr) : parent(p), nextLocalIdx(0) {}
    
    // Declare a new local (fails if already exists in this scope)
    bool declare(const std::string& name, Reg reg) {
        if (locals.count(name)) return false;  // Shadowing not allowed
        locals[name] = reg;
        localIndices[name] = nextLocalIdx++;
        return true;
    }
    
    // Look up variable (walks up scope chain)
    std::pair<Reg, int32_t> lookup(const std::string& name) const {
        auto it = locals.find(name);
        if (it != locals.end()) {
            return {it->second, localIndices.at(name)};
        }
        if (parent) {
            auto [reg, depth] = parent->lookup(name);
            if (reg != NO_REG) {
                return {reg, depth - 1};  // Negative = upvalue
            }
        }
        return {NO_REG, -1};
    }
    
    // Check if name exists in current scope
    bool contains(const std::string& name) const {
        return locals.count(name) > 0;
    }
    
    int getLocalCount() const { return nextLocalIdx; }
};

// IR Builder - generates IR from AST-like structures
class IrBuilder {
    IrFunction* currentFunc;
    Scope* currentScope;
    std::vector<std::unique_ptr<Scope>> scopeStack;
    std::unordered_set<std::string> globals;
    
public:
    explicit IrBuilder(IrFunction* func) 
        : currentFunc(func), currentScope(nullptr) {
        pushScope();  // Global scope
    }
    
    // Scope management
    void pushScope() {
        scopeStack.push_back(std::make_unique<Scope>(currentScope));
        currentScope = scopeStack.back().get();
    }
    
    void popScope() {
        if (currentScope && currentScope->getLocalCount() > 0) {
            // Track max locals
            if ((uint32_t)currentScope->getLocalCount() > currentFunc->localCount) {
                currentFunc->localCount = currentScope->getLocalCount();
            }
        }
        currentScope = currentScope ? currentScope->lookup("").first == NO_REG ? nullptr : nullptr : nullptr;
        // Actually handle parent correctly
        if (!scopeStack.empty()) {
            scopeStack.pop_back();
            currentScope = scopeStack.empty() ? nullptr : scopeStack.back().get();
        }
    }
    
    // Variable handling
    Reg declareLocal(const std::string& name);
    std::pair<Reg, bool> resolveVariable(const std::string& name);
    void declareGlobal(const std::string& name);
    bool isGlobal(const std::string& name) const;
    
    // Code generation helpers
    Reg emitLoad(const IrValue& value);
    Reg emitBinOp(IrOp op, Reg left, Reg right);
    Reg emitBinOp(IrOp op, const IrValue& left, const IrValue& right);
    Reg emitUnOp(IrOp op, Reg src);
    void emitJump(IrOp op, Reg cond, int16_t offset);
    void emitStoreLocal(int32_t idx, Reg src);
    Reg emitLoadLocal(int32_t idx);
    void emitStoreGlobal(const std::string& name, Reg src);
    Reg emitLoadGlobal(const std::string& name);
    void emitPrint(Reg src);
    void emitReturn(Reg src = NO_REG);
    
    // Peephole optimizations (during generation)
    void tryConstantFold(IrInstr& instr);
    bool lastInstrWas(IrOp op) const;
    void removeLastInstr();
    
    // Get current function
    IrFunction* getFunction() const { return currentFunc; }
};

// Simple optimizations
class IrOptimizer {
public:
    // Constant folding
    static void foldConstants(IrFunction& func);
    
    // Dead code elimination
    static void eliminateDeadCode(IrFunction& func);
    
    // Copy propagation
    static void eliminateCommonSubexpressions(IrFunction& func);
    
    // Strength reduction (e.g., x * 0 -> 0)
    static void strengthReduction(IrFunction& func);
    
    // Run all optimizations
    static void optimize(IrFunction& func);
    
    // Print stats
    static void printStats(const IrFunction& before, const IrFunction& after);
};

// Lower IR to bytecode
class IrToBytecode {
public:
    static Bytecode lower(const IrFunction& func, std::vector<std::string>& outConstants);
};

} // namespace ir
} // namespace kern
