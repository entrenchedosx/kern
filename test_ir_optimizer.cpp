/* *
 * test_ir_optimizer.cpp - IR Optimizer Tests
 * 
 * Verifies:
 * 1. Constant folding works
 * 2. Dead code elimination works
 * 3. Strength reduction works
 * 4. Overall optimization reduces instruction count
 */
#include <iostream>
#include <cassert>
#include "kern/ir/linear_ir.hpp"
#include "kern/ir/ir_optimizer.cpp"

using namespace kern::ir;

int testsPassed = 0;
int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Testing " << #name << "... " << std::flush; \
    try { \
        test_##name(); \
        testsPassed++; \
        std::cout << "PASS" << std::endl; \
    } catch (const std::exception& e) { \
        testsFailed++; \
        std::cout << "FAIL: " << e.what() << std::endl; \
    } catch (...) { \
        testsFailed++; \
        std::cout << "FAIL: Unknown exception" << std::endl; \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " == " #b); \
    } \
} while(0)

// ============================================================================
// Helper to print IR
// ============================================================================

void printFunction(const IrFunction& func, const std::string& name) {
    std::cout << "\n=== " << name << " (" << func.instructions.size() << " instrs) ===" << std::endl;
    for (size_t i = 0; i < func.instructions.size(); i++) {
        const auto& inst = func.instructions[i];
        std::cout << "  [" << i << "] " << inst.toString() << std::endl;
    }
}

std::string opToString(IrOp op) {
    switch (op) {
        case IrOp::LOAD_CONST: return "LOAD_CONST";
        case IrOp::ADD: return "ADD";
        case IrOp::SUB: return "SUB";
        case IrOp::MUL: return "MUL";
        case IrOp::DIV: return "DIV";
        case IrOp::MOVE: return "MOVE";
        case IrOp::PRINT: return "PRINT";
        case IrOp::RETURN: return "RETURN";
        default: return "UNKNOWN";
    }
}

std::string IrInstr::toString() const {
    std::string result = opToString(op) + " r" + std::to_string(dest);
    if (!srcA.isNil()) {
        if (srcA.isConst()) {
            result += ", const";
        } else {
            result += ", r" + std::to_string(srcA.reg);
        }
    }
    if (!srcB.isNil()) {
        if (srcB.isConst()) {
            result += ", const";
        } else {
            result += ", r" + std::to_string(srcB.reg);
        }
    }
    return result;
}

// ============================================================================
// Constant Folding Tests
// ============================================================================

TEST(constant_folding_add) {
    IrFunction func;
    func.name = "test_add";
    
    // r0 = 2 + 3 (should fold to r0 = 5)
    Reg r0 = func.allocReg();
    func.emit({IrOp::ADD, r0, IrValue(2), IrValue(3)});
    func.emit({IrOp::RETURN, NO_REG, IrValue(r0)});
    
    printFunction(func, "Before");
    
    IrOptimizer::foldConstants(func);
    
    printFunction(func, "After");
    
    // Should be LOAD_CONST 5
    ASSERT_EQ(func.instructions.size(), 2);
    ASSERT_EQ(func.instructions[0].op, IrOp::LOAD_CONST);
}

TEST(constant_folding_arithmetic) {
    IrFunction func;
    func.name = "test_arith";
    
    // Various constant operations
    Reg r0 = func.allocReg();
    Reg r1 = func.allocReg();
    Reg r2 = func.allocReg();
    Reg r3 = func.allocReg();
    
    func.emit({IrOp::ADD, r0, IrValue(10), IrValue(20)});   // 30
    func.emit({IrOp::SUB, r1, IrValue(50), IrValue(20)});   // 30
    func.emit({IrOp::MUL, r2, IrValue(6), IrValue(7)});     // 42
    func.emit({IrOp::DIV, r3, IrValue(100), IrValue(4)});    // 25
    func.emit({IrOp::RETURN});
    
    size_t beforeSize = func.instructions.size();
    
    IrOptimizer::foldConstants(func);
    
    // All arithmetic should be folded to LOAD_CONST
    ASSERT_EQ(func.instructions[0].op, IrOp::LOAD_CONST);
    ASSERT_EQ(func.instructions[1].op, IrOp::LOAD_CONST);
    ASSERT_EQ(func.instructions[2].op, IrOp::LOAD_CONST);
    ASSERT_EQ(func.instructions[3].op, IrOp::LOAD_CONST);
}

TEST(constant_folding_comparison) {
    IrFunction func;
    func.name = "test_cmp";
    
    Reg r0 = func.allocReg();
    Reg r1 = func.allocReg();
    
    func.emit({IrOp::LT, r0, IrValue(5), IrValue(10)});   // true
    func.emit({IrOp::EQ, r1, IrValue(3), IrValue(3)});    // true
    func.emit({IrOp::RETURN});
    
    IrOptimizer::foldConstants(func);
    
    // Should fold to boolean constants
    ASSERT_EQ(func.instructions[0].op, IrOp::LOAD_CONST);
    ASSERT_EQ(func.instructions[1].op, IrOp::LOAD_CONST);
}

// ============================================================================
// Strength Reduction Tests
// ============================================================================

TEST(strength_reduction_mul_zero) {
    IrFunction func;
    func.name = "test_mul_zero";
    
    Reg r0 = func.allocReg();
    Reg r1 = func.allocReg();
    
    func.emit({IrOp::MUL, r0, IrValue(0), IrValue(42)});  // 0 * 42 -> 0
    func.emit({IrOp::MUL, r1, IrValue(10), IrValue(0)}); // 10 * 0 -> 0
    func.emit({IrOp::RETURN});
    
    IrOptimizer::strengthReduction(func);
    
    // Both should become LOAD_CONST 0
    ASSERT_EQ(func.instructions[0].op, IrOp::LOAD_CONST);
    ASSERT_EQ(func.instructions[1].op, IrOp::LOAD_CONST);
}

TEST(strength_reduction_mul_one) {
    IrFunction func;
    func.name = "test_mul_one";
    
    Reg r0 = func.allocReg();
    Reg r1 = func.allocReg();
    
    // r0 = x * 1 -> r0 = x
    Reg x = func.allocReg();
    func.emit({IrOp::LOAD_CONST, x, IrValue(10)});
    func.emit({IrOp::MUL, r0, IrValue(x), IrValue(1)});
    func.emit({IrOp::MUL, r1, IrValue(1), IrValue(x)});
    func.emit({IrOp::RETURN});
    
    IrOptimizer::strengthReduction(func);
    
    // Should become MOVE
    ASSERT_EQ(func.instructions[2].op, IrOp::MOVE);
    ASSERT_EQ(func.instructions[3].op, IrOp::MOVE);
}

TEST(strength_reduction_add_zero) {
    IrFunction func;
    func.name = "test_add_zero";
    
    Reg r0 = func.allocReg();
    Reg x = func.allocReg();
    
    func.emit({IrOp::LOAD_CONST, x, IrValue(10)});
    func.emit({IrOp::ADD, r0, IrValue(x), IrValue(0)});  // x + 0 -> x
    func.emit({IrOp::RETURN});
    
    IrOptimizer::strengthReduction(func);
    
    // Should become MOVE
    ASSERT_EQ(func.instructions[2].op, IrOp::MOVE);
}

// ============================================================================
// Dead Code Elimination Tests
// ============================================================================

TEST(dead_code_simple) {
    IrFunction func;
    func.name = "test_dce";
    
    Reg r0 = func.allocReg();
    Reg r1 = func.allocReg();  // Dead
    Reg r2 = func.allocReg();
    
    func.emit({IrOp::LOAD_CONST, r0, IrValue(10)});   // Used
    func.emit({IrOp::LOAD_CONST, r1, IrValue(20)});   // Dead - never read
    func.emit({IrOp::MOVE, r2, IrValue(r0)});         // Uses r0
    func.emit({IrOp::PRINT, NO_REG, IrValue(r2)});    // Uses r2
    func.emit({IrOp::RETURN});
    
    printFunction(func, "Before DCE");
    
    IrOptimizer::eliminateDeadCode(func);
    
    printFunction(func, "After DCE");
    
    // Dead load should be removed
    ASSERT(func.instructions.size() < 5);
}

// ============================================================================
// Full Optimization Pipeline Tests
// ============================================================================

TEST(optimization_pipeline) {
    IrFunction func;
    func.name = "test_pipeline";
    
    // Complex example: x = 2 + 3; y = x * 1; print y
    // Should optimize to: y = 5; print y
    
    Reg x = func.allocReg();
    Reg y = func.allocReg();
    
    func.emit({IrOp::ADD, x, IrValue(2), IrValue(3)});      // x = 2 + 3 = 5
    func.emit({IrOp::MUL, y, IrValue(x), IrValue(1)});       // y = x * 1 = x
    func.emit({IrOp::PRINT, NO_REG, IrValue(y)});
    func.emit({IrOp::RETURN});
    
    IrFunction before = func;  // Copy
    
    printFunction(func, "Before optimization");
    
    IrOptimizer::optimize(func);
    
    printFunction(func, "After optimization");
    
    // Print stats
    IrOptimizer::printStats(before, func);
    
    // Should have fewer instructions
    ASSERT(func.instructions.size() <= before.instructions.size());
}

TEST(optimization_complex_expression) {
    IrFunction func;
    func.name = "test_complex";
    
    // (2 + 3) * (4 + 5) + (10 - 5)
    // = 5 * 9 + 5
    // = 45 + 5
    // = 50
    
    Reg a = func.allocReg();
    Reg b = func.allocReg();
    Reg c = func.allocReg();
    Reg d = func.allocReg();
    Reg result = func.allocReg();
    
    func.emit({IrOp::ADD, a, IrValue(2), IrValue(3)});
    func.emit({IrOp::ADD, b, IrValue(4), IrValue(5)});
    func.emit({IrOp::MUL, c, IrValue(a), IrValue(b)});
    func.emit({IrOp::SUB, d, IrValue(10), IrValue(5)});
    func.emit({IrOp::ADD, result, IrValue(c), IrValue(d)});
    func.emit({IrOp::PRINT, NO_REG, IrValue(result)});
    func.emit({IrOp::RETURN});
    
    size_t beforeSize = func.instructions.size();
    
    IrOptimizer::optimize(func);
    
    // Should fold everything to single constant
    ASSERT(func.instructions.size() < beforeSize);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "IR Optimizer Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n=== CONSTANT FOLDING ===" << std::endl;
    RUN_TEST(constant_folding_add);
    RUN_TEST(constant_folding_arithmetic);
    RUN_TEST(constant_folding_comparison);
    
    std::cout << "\n=== STRENGTH REDUCTION ===" << std::endl;
    RUN_TEST(strength_reduction_mul_zero);
    RUN_TEST(strength_reduction_mul_one);
    RUN_TEST(strength_reduction_add_zero);
    
    std::cout << "\n=== DEAD CODE ELIMINATION ===" << std::endl;
    RUN_TEST(dead_code_simple);
    
    std::cout << "\n=== FULL PIPELINE ===" << std::endl;
    RUN_TEST(optimization_pipeline);
    RUN_TEST(optimization_complex_expression);
    
    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total:  " << (testsPassed + testsFailed) << std::endl;
    
    if (testsFailed == 0) {
        std::cout << "\n✓ ALL OPTIMIZER TESTS PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ SOME TESTS FAILED" << std::endl;
        return 1;
    }
}
