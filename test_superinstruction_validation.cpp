/* *
 * test_superinstruction_validation.cpp - Superinstruction Correctness
 * 
 * Validates that superinstructions produce identical results to
 * the original instruction sequences they replace.
 */
#include <iostream>
#include <sstream>
#include <iomanip>
#include "kern/compiler/scope_codegen.hpp"
#include "kern/runtime/vm_superinstructions.hpp"
#include "kern/runtime/vm_minimal.hpp"
#include "kern/testing/fuzz_tester.hpp"

using namespace kern;

struct SuperInstrTest {
    std::string name;
    std::string code;
    std::string expectedOutput;
};

class SuperinstructionValidator {
    std::vector<SuperInstrTest> tests;
    int passed = 0;
    int failed = 0;
    
public:
    void addTests() {
        // Test 1: STORE_CONST_LOCAL pattern
        tests.push_back({
            "STORE_CONST_LOCAL",
            "let x = 42\nprint x",
            "42\n"
        });
        
        // Test 2: INC_LOCAL pattern
        tests.push_back({
            "INC_LOCAL",
            "let i = 0\nlet i = i + 1\nprint i",
            "1\n"
        });
        
        // Test 3: DEC_LOCAL pattern
        tests.push_back({
            "DEC_LOCAL", 
            "let i = 5\nlet i = i - 1\nprint i",
            "4\n"
        });
        
        // Test 4: Multiple increments
        tests.push_back({
            "Multiple INC_LOCAL",
            "let x = 0\nlet x = x + 1\nlet x = x + 1\nlet x = x + 1\nprint x",
            "3\n"
        });
        
        // Test 5: Loop with increment
        tests.push_back({
            "Loop with INC_LOCAL",
            "let i = 0\nwhile (i < 5) {\n  let i = i + 1\n}\nprint i",
            "5\n"
        });
        
        // Test 6: Arithmetic chain
        tests.push_back({
            "Arithmetic chain",
            "let a = 10\nlet b = 20\nlet c = a + b\nprint c",
            "30\n"
        });
        
        // Test 7: Complex expression
        tests.push_back({
            "Complex expression",
            "let x = 1 + 2 * 3 - 4 / 2\nprint x",
            "5\n"  // 1 + 6 - 2 = 5
        });
        
        // Test 8: Variable initialization
        tests.push_back({
            "Multiple initializations",
            "let a = 1\nlet b = 2\nlet c = 3\nprint a + b + c",
            "6\n"
        });
        
        // Test 9: Comparison and jump
        tests.push_back({
            "Comparison loop",
            "let i = 0\nwhile (i < 10) {\n  let i = i + 1\n}\nprint i",
            "10\n"
        });
        
        // Test 10: Nested blocks
        tests.push_back({
            "Nested blocks",
            "let x = 1\n{\n  let x = 2\n  print x\n}\nprint x",
            "2\n1\n"
        });
    }
    
    bool runAll() {
        std::cout << "\n========================================\n";
        std::cout << "SUPERINSTRUCTION VALIDATION\n";
        std::cout << "========================================\n\n";
        
        std::cout << "Testing " << tests.size() << " cases...\n\n";
        
        for (const auto& test : tests) {
            runTest(test);
        }
        
        printSummary();
        
        return failed == 0;
    }
    
private:
    void runTest(const SuperInstrTest& test) {
        std::cout << "Testing " << std::left << std::setw(30) << test.name << "... " << std::flush;
        
        // Compile
        ScopeCodeGen codegen;
        auto compileResult = codegen.compile(test.code);
        
        if (!compileResult.success) {
            std::cout << "COMPILE FAILED\n";
            failed++;
            return;
        }
        
        // Run without superinstructions
        MinimalVM baseVM;
        auto baseResult = baseVM.execute(compileResult.code, compileResult.constants);
        
        // Optimize with superinstructions
        auto superCode = PeepholeOptimizer::optimize(compileResult.code);
        
        // Print optimization stats for first few tests
        static int showStats = 3;
        if (showStats-- > 0) {
            PeepholeOptimizer::printStats(compileResult.code, superCode);
        }
        
        // Run with superinstructions
        SuperinstructionVM superVM;
        auto superResult = superVM.executeSuper(superCode, compileResult.constants);
        
        // Compare results
        bool resultsMatch = compareResults(baseResult, superResult);
        
        if (resultsMatch) {
            std::cout << "✓ PASS\n";
            passed++;
        } else {
            std::cout << "✗ FAIL\n";
            std::cout << "  Base output:   " << resultToString(baseResult) << "\n";
            std::cout << "  Super output:  " << resultToString(superResult) << "\n";
            failed++;
        }
    }
    
    bool compareResults(const Result<Value>& a, const Result<Value>& b) {
        if (a.ok() != b.ok()) {
            return false;  // One succeeded, one failed
        }
        
        if (!a.ok()) {
            return true;  // Both failed (same error type not checked)
        }
        
        // Compare values
        // For now, just check toString equality
        return a.value().toString() == b.value().toString();
    }
    
    std::string resultToString(const Result<Value>& r) {
        if (!r.ok()) {
            return "ERROR: " + r.error().message;
        }
        return r.value().toString();
    }
    
    void printSummary() {
        std::cout << "\n========================================\n";
        std::cout << "SUPERINSTRUCTION VALIDATION SUMMARY\n";
        std::cout << "========================================\n";
        std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";
        
        if (failed == 0) {
            std::cout << "✓ ALL SUPERINSTRUCTIONS CORRECT\n";
        } else {
            std::cout << "✗ " << failed << " TESTS FAILED\n";
        }
        
        std::cout << "========================================\n";
    }
};

// Peephole pattern detection test
class PatternDetectionTest {
public:
    void run() {
        std::cout << "\n========================================\n";
        std::cout << "PEEPHOLE PATTERN DETECTION\n";
        std::cout << "========================================\n\n";
        
        // Test code with known patterns
        std::string code = R"(
            let x = 10
            let i = 0
            while (i < 5) {
                let x = x + 1
                let i = i + 1
            }
            print x
        )";
        
        ScopeCodeGen codegen;
        auto result = codegen.compile(code);
        
        if (!result.success) {
            std::cout << "Compile failed\n";
            return;
        }
        
        std::cout << "Original bytecode: " << result.code.size() << " instructions\n";
        
        // Count patterns manually
        int storeConstLocal = 0;
        int incLocal = 0;
        int loadLocalPair = 0;
        
        for (size_t i = 0; i + 1 < result.code.size(); i++) {
            // Check for STORE_CONST_LOCAL
            if (result.code[i].op == Instruction::PUSH_CONST &&
                result.code[i+1].op == Instruction::STORE_LOCAL) {
                storeConstLocal++;
                std::cout << "Found STORE_CONST_LOCAL pattern at instruction " << i << "\n";
            }
            
            // Check for LOAD_LOCAL_PAIR
            if (result.code[i].op == Instruction::LOAD_LOCAL &&
                result.code[i+1].op == Instruction::LOAD_LOCAL) {
                loadLocalPair++;
                std::cout << "Found LOAD_LOCAL_PAIR pattern at instruction " << i << "\n";
            }
        }
        
        // Check for INC_LOCAL (4 instruction sequence)
        for (size_t i = 0; i + 3 < result.code.size(); i++) {
            if (result.code[i].op == Instruction::LOAD_LOCAL &&
                result.code[i+1].op == Instruction::PUSH_CONST &&
                result.code[i+2].op == Instruction::ADD &&
                result.code[i+3].op == Instruction::STORE_LOCAL &&
                result.code[i].operand == result.code[i+3].operand) {
                incLocal++;
                std::cout << "Found INC_LOCAL pattern at instruction " << i << "\n";
            }
        }
        
        std::cout << "\nPatterns detected:\n";
        std::cout << "  STORE_CONST_LOCAL: " << storeConstLocal << "\n";
        std::cout << "  INC_LOCAL: " << incLocal << "\n";
        std::cout << "  LOAD_LOCAL_PAIR: " << loadLocalPair << "\n";
        
        // Now run optimizer and check
        auto superCode = PeepholeOptimizer::optimize(result.code);
        
        std::cout << "\nAfter optimization:\n";
        std::cout << "  Superinstructions used: " << countSuperinstructions(superCode) << "\n";
        
        int reduction = result.code.size() - superCode.size();
        double pct = result.code.size() > 0 ? (100.0 * reduction / result.code.size()) : 0;
        std::cout << "  Size reduction: " << reduction << " instructions (" 
                  << std::fixed << std::setprecision(1) << pct << "%)\n";
    }
    
private:
    int countSuperinstructions(const SuperBytecode& code) {
        int count = 0;
        for (const auto& inst : code) {
            if (inst.op >= 32) count++;  // Superinstructions start at 32
        }
        return count;
    }
};

// Fuzz test with superinstructions
class SuperinstructionFuzzTest {
    ProgramGenerator gen;
    
public:
    void run(int iterations = 100) {
        std::cout << "\n========================================\n";
        std::cout << "SUPERINSTRUCTION FUZZ TEST\n";
        std::cout << "========================================\n\n";
        
        std::cout << "Testing " << iterations << " random programs...\n";
        
        int mismatches = 0;
        int errors = 0;
        
        for (int i = 0; i < iterations; i++) {
            // Generate valid-ish program
            std::string code = gen.generateRandomProgram();
            
            ScopeCodeGen codegen;
            auto result = codegen.compile(code);
            
            if (!result.success) continue;  // Compile failure is OK for fuzzing
            
            // Run both versions
            MinimalVM baseVM;
            auto baseResult = baseVM.execute(result.code, result.constants);
            
            auto superCode = PeepholeOptimizer::optimize(result.code);
            SuperinstructionVM superVM;
            auto superResult = superVM.executeSuper(superCode, result.constants);
            
            // Check for mismatch
            if (baseResult.ok() != superResult.ok()) {
                mismatches++;
                if (mismatches <= 3) {
                    std::cout << "Mismatch found!\n";
                    std::cout << "Code: " << code.substr(0, 50) << "\n";
                }
            } else if (!baseResult.ok()) {
                // Both failed - that's fine
            } else if (baseResult.value().toString() != superResult.value().toString()) {
                mismatches++;
                if (mismatches <= 3) {
                    std::cout << "Output mismatch!\n";
                    std::cout << "Code: " << code.substr(0, 50) << "\n";
                    std::cout << "Base: " << baseResult.value().toString() << "\n";
                    std::cout << "Super: " << superResult.value().toString() << "\n";
                }
            }
        }
        
        std::cout << "\nFuzz test complete:\n";
        std::cout << "  Iterations: " << iterations << "\n";
        std::cout << "  Mismatches: " << mismatches;
        if (mismatches == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        
        if (mismatches == 0) {
            std::cout << "\n✓ SUPERINSTRUCTION FUZZ TEST PASSED\n";
        } else {
            std::cout << "\n✗ FOUND " << mismatches << " MISMATCHES\n";
        }
    }
};

int main() {
    std::cout << "\n########################################\n";
    std::cout << "#                                      #\n";
    std::cout << "#  SUPERINSTRUCTION VALIDATION SUITE   #\n";
    std::cout << "#              v2.0.2                  #\n";
    std::cout << "#                                      #\n";
    std::cout << "########################################\n";
    
    // 1. Validation tests
    SuperinstructionValidator validator;
    validator.addTests();
    bool validationPassed = validator.runAll();
    
    // 2. Pattern detection
    PatternDetectionTest patternTest;
    patternTest.run();
    
    // 3. Fuzz testing
    SuperinstructionFuzzTest fuzzTest;
    fuzzTest.run(200);
    
    std::cout << "\n\n========================================\n";
    if (validationPassed) {
        std::cout << "✓ ALL VALIDATION PASSED\n";
        return 0;
    } else {
        std::cout << "✗ VALIDATION FAILED\n";
        return 1;
    }
}
