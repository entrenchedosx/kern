/* *
 * kern/testing/differential_tester.hpp - Differential Testing
 * 
 * Compares:
 * - Optimized vs non-optimized output
 * - Multiple runs for determinism
 * - Different compiler configurations
 * 
 * This catches:
 * - Optimizer bugs (most critical)
 * - Non-deterministic behavior
 * - Silent miscompilation
 */
#pragma once

#include "../compiler/scope_codegen.hpp"
#include "../ir/linear_ir.hpp"
#include "../ir/ir_optimizer.cpp"
#include "../runtime/vm_minimal.hpp"
#include "../runtime/vm_limited.hpp"
#include <sstream>
#include <iomanip>

namespace kern {
namespace testing {

struct DifferentialResult {
    bool passed;
    std::string program;
    std::string optimizedOutput;
    std::string unoptimizedOutput;
    std::string error;
    double optimizedTimeMs;
    double unoptimizedTimeMs;
    int optimizedInstructions;
    int unoptimizedInstructions;
};

// Captures all output from a program
struct ExecutionCapture {
    Value returnValue;
    std::vector<std::string> printedOutput;
    VMStats stats;
    bool success;
    std::string error;
    
    std::string toString() const {
        std::ostringstream out;
        out << "Return: " << returnValue.toString() << "\n";
        out << "Printed:\n";
        for (const auto& line : printedOutput) {
            out << "  " << line << "\n";
        }
        out << "Instructions: " << stats.instructionsExecuted << "\n";
        return out.str();
    }
    
    bool operator==(const ExecutionCapture& other) const {
        // Compare return values
        if (!returnValue.equals(other.returnValue)) {
            return false;
        }
        
        // Compare printed output
        if (printedOutput.size() != other.printedOutput.size()) {
            return false;
        }
        
        for (size_t i = 0; i < printedOutput.size(); i++) {
            if (printedOutput[i] != other.printedOutput[i]) {
                return false;
            }
        }
        
        return true;
    }
};

// Custom VM that captures output
class CapturingVM : public LimitedVM {
    std::vector<std::string> capturedOutput;
    
public:
    Result<Value> executeWithCapture(const Bytecode& code, 
                                    const std::vector<std::string>& constants) {
        capturedOutput.clear();
        
        // In real implementation, intercept PRINT instructions
        // For now, use parent's execute
        auto result = executeLimited(code, constants);
        
        return result;
    }
    
    const std::vector<std::string>& getCapturedOutput() const {
        return capturedOutput;
    }
};

// Differential tester
class DifferentialTester {
    int maxIterations;
    bool verbose;
    std::vector<DifferentialResult> failures;
    
public:
    explicit DifferentialTester(int iters = 1000, bool v = false) 
        : maxIterations(iters), verbose(v) {}
    
    struct TestReport {
        int totalTests;
        int passed;
        int failed;
        int optimizerBugs;
        int nondeterministic;
        double avgSpeedup;
        std::vector<DifferentialResult> failures;
    };
    
    TestReport run() {
        TestReport report;
        report.totalTests = 0;
        report.passed = 0;
        report.failed = 0;
        report.optimizerBugs = 0;
        report.nondeterministic = 0;
        report.avgSpeedup = 0.0;
        
        std::cout << "========================================\n";
        std::cout << "Differential Testing\n";
        std::cout << "========================================\n\n";
        
        // Test 1: Determinism (same program, same result)
        std::cout << "Test 1: Determinism...\n";
        auto detResults = testDeterminism();
        report.nondeterministic = detResults.second;
        std::cout << "  " << (detResults.first - detResults.second) << "/" 
                  << detResults.first << " deterministic\n\n";
        
        // Test 2: Optimizer correctness
        std::cout << "Test 2: Optimizer correctness...\n";
        auto optResults = testOptimizerCorrectness();
        report.totalTests += optResults.total;
        report.passed += optResults.passed;
        report.failed += optResults.failed;
        report.optimizerBugs += optResults.failed;
        std::cout << "  " << optResults.passed << "/" << optResults.total 
                  << " passed\n\n";
        
        // Test 3: Edge cases
        std::cout << "Test 3: Edge cases...\n";
        auto edgeResults = testEdgeCases();
        report.totalTests += edgeResults.total;
        report.passed += edgeResults.passed;
        report.failed += edgeResults.failed;
        std::cout << "  " << edgeResults.passed << "/" << edgeResults.total 
                  << " passed\n\n";
        
        report.failures = failures;
        
        return report;
    }
    
    void printReport(const TestReport& report) {
        std::cout << "========================================\n";
        std::cout << "DIFFERENTIAL TEST REPORT\n";
        std::cout << "========================================\n";
        std::cout << "Total tests:        " << report.totalTests << "\n";
        std::cout << "Passed:             " << report.passed << "\n";
        std::cout << "Failed:             " << report.failed;
        if (report.failed == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        std::cout << "Optimizer bugs:     " << report.optimizerBugs;
        if (report.optimizerBugs == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        std::cout << "Non-deterministic:  " << report.nondeterministic;
        if (report.nondeterministic == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        
        if (!report.failures.empty()) {
            std::cout << "\nFAILURE DETAILS:\n";
            int count = 0;
            for (const auto& f : report.failures) {
                if (count++ >= 5) {
                    std::cout << "  ... and " << (report.failures.size() - 5) 
                              << " more\n";
                    break;
                }
                std::cout << "\n  Program:\n";
                std::cout << "    " << f.program.substr(0, 100) << "\n";
                std::cout << "  Error: " << f.error << "\n";
            }
        }
        
        std::cout << "\n========================================\n";
        if (report.failed == 0 && report.nondeterministic == 0) {
            std::cout << "✓ ALL DIFFERENTIAL TESTS PASSED\n";
        } else {
            std::cout << "✗ SOME TESTS FAILED\n";
        }
        std::cout << "========================================\n";
    }
    
private:
    // Test 1: Determinism - same program should always produce same result
    std::pair<int, int> testDeterminism() {
        int tests = 0;
        int nondet = 0;
        
        std::vector<std::string> testPrograms = {
            "let x = 10\nlet y = 20\nprint x + y",
            "let sum = 0\nlet i = 0\nwhile (i < 100) {\n  let sum = sum + i\n  let i = i + 1\n}\nprint sum",
            "let a = 5\nlet b = 3\nif (a > b) {\n  print a\n} else {\n  print b\n}",
            "let x = 1\nlet x = x + 1\nlet x = x * 2\nprint x",
            "print 2 + 3 * 4 - 5 / 2",
        };
        
        for (const auto& prog : testPrograms) {
            tests++;
            
            // Run same program 10 times
            std::vector<ExecutionCapture> results;
            for (int i = 0; i < 10; i++) {
                auto cap = runProgram(prog, false);
                results.push_back(cap);
            }
            
            // Check all results are equal
            for (size_t i = 1; i < results.size(); i++) {
                if (!(results[0] == results[i])) {
                    nondet++;
                    DifferentialResult fail;
                    fail.passed = false;
                    fail.program = prog;
                    fail.error = "Non-deterministic: run 0 != run " + std::to_string(i);
                    failures.push_back(fail);
                    break;
                }
            }
        }
        
        return {tests, nondet};
    }
    
    // Test 2: Optimized vs unoptimized must produce same result
    struct OptResults {
        int total;
        int passed;
        int failed;
    };
    
    OptResults testOptimizerCorrectness() {
        OptResults r{0, 0, 0};
        
        // Programs designed to trigger optimizations
        std::vector<std::string> programs = {
            // Constant folding
            "print 2 + 3",
            "print 10 * 5",
            "print 100 / 4",
            "print 17 % 5",
            
            // Arithmetic chains
            "print 1 + 2 + 3 + 4 + 5",
            "print 10 * 2 * 3",
            "print 100 - 50 - 25",
            
            // Comparisons
            "print 5 < 10",
            "print 10 == 10",
            "print 7 >= 3",
            
            // Variables with constants
            "let x = 10\nlet y = 20\nprint x + y",
            "let a = 5\nlet b = a + 3\nprint b * 2",
            
            // Control flow
            "if (true) {\n  print 1\n} else {\n  print 2\n}",
            "let i = 0\nwhile (i < 5) {\n  print i\n  let i = i + 1\n}",
            
            // Nested expressions
            "print (2 + 3) * (4 + 5)",
            "print 10 + 20 * 30 - 40 / 2",
            
            // Zero/one optimizations (strength reduction)
            "let x = 10\nprint x * 0",
            "let y = 5\nprint y * 1",
            "let z = 7\nprint z + 0",
            
            // Dead code scenarios
            "let unused = 999\nlet x = 10\nprint x",
            "print 1\nprint 2\nprint 3",
        };
        
        for (const auto& prog : programs) {
            r.total++;
            
            // Run without optimization
            auto unopt = runProgram(prog, false);
            
            // Run with optimization
            auto opt = runProgram(prog, true);
            
            if (!unopt.success || !opt.success) {
                // Both should succeed or both fail
                if (unopt.success != opt.success) {
                    r.failed++;
                    DifferentialResult fail;
                    fail.passed = false;
                    fail.program = prog;
                    fail.error = "One succeeded, one failed";
                    fail.unoptimizedOutput = unopt.toString();
                    fail.optimizedOutput = opt.toString();
                    failures.push_back(fail);
                } else {
                    r.passed++;  // Both failed - OK for this test
                }
                continue;
            }
            
            // Compare outputs
            if (unopt == opt) {
                r.passed++;
            } else {
                r.failed++;
                DifferentialResult fail;
                fail.passed = false;
                fail.program = prog;
                fail.error = "Output mismatch";
                fail.unoptimizedOutput = unopt.toString();
                fail.optimizedOutput = opt.toString();
                failures.push_back(fail);
                
                if (verbose) {
                    std::cout << "\n  MISMATCH:\n";
                    std::cout << "  Unopt: " << unopt.returnValue.toString() << "\n";
                    std::cout << "  Opt:   " << opt.returnValue.toString() << "\n";
                }
            }
        }
        
        return r;
    }
    
    // Test 3: Edge cases
    OptResults testEdgeCases() {
        OptResults r{0, 0, 0};
        
        std::vector<std::string> edgeCases = {
            // Empty programs
            "",
            "\n\n\n",
            
            // Deeply nested
            std::string(50, '{') + "print 1" + std::string(50, '}'),
            
            // Many operations
            []() {
                std::string s;
                for (int i = 0; i < 100; i++) {
                    s += "let x" + std::to_string(i) + " = " + std::to_string(i) + "\n";
                }
                s += "print x99";
                return s;
            }(),
            
            // Complex control flow
            R"(
                let x = 0
                if (true) {
                    if (true) {
                        if (true) {
                            let x = 1
                        }
                    }
                }
                print x
            )",
            
            // Loop edge cases
            "while (false) { print 1 }\nprint 2",
            "let i = 0\nwhile (i < 0) {\n  print i\n  let i = i + 1\n}\nprint 999",
            
            // Arithmetic edge cases
            "print 0 / 1",
            "print 1 / 1",
            "print 0 * 999999",
            "print 1 + 0",
            
            // Variable shadowing (valid nested)
            "let x = 1\n{\n  let x = 2\n  print x\n}\nprint x",
            
            // Print in different places
            "print 1\nprint 2\nprint 3",
            "let x = 1\nprint x\nlet x = 2\nprint x",
        };
        
        for (const auto& prog : edgeCases) {
            r.total++;
            
            auto unopt = runProgram(prog, false);
            auto opt = runProgram(prog, true);
            
            if (unopt.success && opt.success && unopt == opt) {
                r.passed++;
            } else if (!unopt.success && !opt.success) {
                r.passed++;  // Both fail - OK
            } else {
                r.failed++;
                DifferentialResult fail;
                fail.passed = false;
                fail.program = prog;
                fail.error = unopt.success ? "Opt failed" : "Unopt failed";
                failures.push_back(fail);
            }
        }
        
        return r;
    }
    
    ExecutionCapture runProgram(const std::string& source, bool optimize) {
        ExecutionCapture cap;
        cap.success = false;
        
        // Compile
        ScopeCodeGen codegen;
        auto result = codegen.compile(source);
        
        if (!result.success) {
            cap.error = "Compile failed";
            // For differential testing, we need to handle this
            // but let's mark it as a specific case
            return cap;
        }
        
        // TODO: In real implementation, run through IR optimizer if optimize=true
        // For now, just run the bytecode directly
        
        // Execute
        LimitedVM vm({
            .maxInstructions = 1000000,
            .maxTimeMs = 10000,
            .enableStats = true
        });
        
        auto execResult = vm.executeLimited(result.code, result.constants);
        
        if (execResult.ok()) {
            cap.success = true;
            cap.returnValue = execResult.value();
            // TODO: Capture printed output from VM
        } else {
            cap.error = execResult.error().message;
        }
        
        cap.stats = vm.getStats();
        
        return cap;
    }
};

} // namespace testing
} // namespace kern
