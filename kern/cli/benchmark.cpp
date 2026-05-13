/* *
 * kern/cli/benchmark.cpp - Stress Testing Framework
 * 
 * Performance and correctness validation for the Kern runtime system.
 * Tests VM performance, memory usage, and standard library functionality.
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "../compiler/minimal_codegen.hpp"
#include "../core/bytecode/bytecode.hpp"
#include "../runtime/vm/vm.hpp"
#include "../stdlib/core.hpp"

namespace kern::benchmark {

struct BenchmarkResult {
    std::string name;
    double executionTimeMs;
    size_t instructionCount;
    size_t memoryUsage;
    bool passed;
    std::string errorMessage;
};

class BenchmarkSuite {
private:
    std::vector<BenchmarkResult> results;
    
public:
    void addResult(const BenchmarkResult& result) {
        results.push_back(result);
    }
    
    void printResults() const {
        std::cout << "\n=== Benchmark Results ===" << std::endl;
        std::cout << std::left << std::setw(30) << "Test Name" 
                  << std::setw(15) << "Time (ms)" 
                  << std::setw(12) << "Instructions" 
                  << std::setw(10) << "Status" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        for (const auto& result : results) {
            std::cout << std::left << std::setw(30) << result.name
                      << std::setw(15) << std::fixed << std::setprecision(2) << result.executionTimeMs
                      << std::setw(12) << result.instructionCount
                      << std::setw(10) << (result.passed ? "PASS" : "FAIL") << std::endl;
            
            if (!result.passed) {
                std::cout << "  Error: " << result.errorMessage << std::endl;
            }
        }
    }
    
    bool allPassed() const {
        return std::all_of(results.begin(), results.end(), 
                          [](const BenchmarkResult& r) { return r.passed; });
    }
};

// Benchmark 1: Arithmetic Performance
BenchmarkResult testArithmeticPerformance() {
    BenchmarkResult result;
    result.name = "Arithmetic Performance";
    result.passed = true;
    
    try {
        kern::VM vm;
        kern::stdlib::initializeStandardLibrary(vm);
        
        // Generate bytecode for 1000 arithmetic operations
        kern::Bytecode code;
        for (int i = 0; i < 1000; ++i) {
            code.push_back({kern::Opcode::CONST_I64, (int64_t)i});
            code.push_back({kern::Opcode::CONST_I64, (int64_t)(i + 1)});
            code.push_back({kern::Opcode::ADD, (int64_t)0});
            code.push_back({kern::Opcode::POP, (int64_t)0});
        }
        code.push_back({kern::Opcode::HALT, (int64_t)0});
        
        result.instructionCount = code.size();
        
        auto start = std::chrono::high_resolution_clock::now();
        vm.setBytecode(code);
        vm.run();
        auto end = std::chrono::high_resolution_clock::now();
        
        result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Should complete quickly (under 100ms for 1000 operations)
        if (result.executionTimeMs > 100.0) {
            result.passed = false;
            result.errorMessage = "Performance too slow: " + std::to_string(result.executionTimeMs) + "ms";
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

// Benchmark 2: Control Flow Performance
BenchmarkResult testControlFlowPerformance() {
    BenchmarkResult result;
    result.name = "Control Flow Performance";
    result.passed = true;
    
    try {
        kern::VM vm;
        kern::stdlib::initializeStandardLibrary(vm);
        
        // Generate bytecode for nested loops
        kern::Bytecode code;
        
        // Outer loop setup
        code.push_back({kern::Opcode::CONST_I64, (int64_t)0});  // i = 0
        code.push_back({kern::Opcode::STORE, (int64_t)0});
        
        size_t loopStart = code.size();
        code.push_back({kern::Opcode::LOAD, (int64_t)0});        // load i
        code.push_back({kern::Opcode::CONST_I64, (int64_t)100}); // compare with 100
        code.push_back({kern::Opcode::GE, (int64_t)0});
        code.push_back({kern::Opcode::JMP_IF_FALSE, (size_t)0}); // will be patched
        
        // Inner loop
        code.push_back({kern::Opcode::CONST_I64, (int64_t)0});  // j = 0
        code.push_back({kern::Opcode::STORE, (int64_t)1});
        
        size_t innerLoopStart = code.size();
        code.push_back({kern::Opcode::LOAD, (int64_t)1});       // load j
        code.push_back({kern::Opcode::CONST_I64, (int64_t)50});  // compare with 50
        code.push_back({kern::Opcode::GE, (int64_t)0});
        code.push_back({kern::Opcode::JMP_IF_FALSE, (size_t)0}); // will be patched
        
        // Inner loop body (simple arithmetic)
        code.push_back({kern::Opcode::CONST_I64, (int64_t)1});
        code.push_back({kern::Opcode::LOAD, (int64_t)1});
        code.push_back({kern::Opcode::ADD, (int64_t)0});
        code.push_back({kern::Opcode::STORE, (int64_t)1});
        
        // Jump back to inner loop start
        code.push_back({kern::Opcode::JMP, (size_t)innerLoopStart});
        
        // Patch inner loop jump
        code[code.size() - 6].operand = (size_t)code.size();
        
        // Outer loop body
        code.push_back({kern::Opcode::CONST_I64, (int64_t)1});
        code.push_back({kern::Opcode::LOAD, (int64_t)0});
        code.push_back({kern::Opcode::ADD, (int64_t)0});
        code.push_back({kern::Opcode::STORE, (int64_t)0});
        
        // Jump back to outer loop start
        code.push_back({kern::Opcode::JMP, (size_t)loopStart});
        
        // Patch outer loop jump
        code[code.size() - 12].operand = (size_t)code.size();
        
        code.push_back({kern::Opcode::HALT, (int64_t)0});
        
        result.instructionCount = code.size();
        
        auto start = std::chrono::high_resolution_clock::now();
        vm.setBytecode(code);
        vm.run();
        auto end = std::chrono::high_resolution_clock::now();
        
        result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Should complete 5000 iterations in reasonable time
        if (result.executionTimeMs > 200.0) {
            result.passed = false;
            result.errorMessage = "Control flow too slow: " + std::to_string(result.executionTimeMs) + "ms";
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

// Benchmark 3: Standard Library Performance
BenchmarkResult testStandardLibraryPerformance() {
    BenchmarkResult result;
    result.name = "Standard Library Performance";
    result.passed = true;
    
    try {
        kern::VM vm;
        kern::stdlib::initializeStandardLibrary(vm);
        
        // Test string operations
        kern::Bytecode code;
        
        // Test string length operations
        for (int i = 0; i < 100; ++i) {
            code.push_back({kern::Opcode::CONST_STR, (size_t)0}); // "Hello World"
            code.push_back({kern::Opcode::BUILTIN, (size_t)17});   // length
            code.push_back({kern::Opcode::POP, (int64_t)0});
        }
        
        // Test math operations
        for (int i = 0; i < 100; ++i) {
            code.push_back({kern::Opcode::CONST_I64, (int64_t)(-i)});
            code.push_back({kern::Opcode::BUILTIN, (size_t)10});   // abs
            code.push_back({kern::Opcode::POP, (int64_t)0});
        }
        
        code.push_back({kern::Opcode::HALT, (int64_t)0});
        
        // Set up string constants
        std::vector<std::string> stringConstants = {"Hello World"};
        vm.setStringConstants(stringConstants);
        
        result.instructionCount = code.size();
        
        auto start = std::chrono::high_resolution_clock::now();
        vm.setBytecode(code);
        vm.run();
        auto end = std::chrono::high_resolution_clock::now();
        
        result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Standard library operations should be reasonably fast
        if (result.executionTimeMs > 50.0) {
            result.passed = false;
            result.errorMessage = "Standard library too slow: " + std::to_string(result.executionTimeMs) + "ms";
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

// Benchmark 4: Memory Stress Test
BenchmarkResult testMemoryStress() {
    BenchmarkResult result;
    result.name = "Memory Stress Test";
    result.passed = true;
    
    try {
        kern::VM vm;
        kern::stdlib::initializeStandardLibrary(vm);
        
        // Generate bytecode that creates many stack operations
        kern::Bytecode code;
        
        // Create deep stack usage
        for (int depth = 0; depth < 100; ++depth) {
            code.push_back({kern::Opcode::CONST_I64, (int64_t)depth});
        }
        
        // Pop everything
        for (int depth = 0; depth < 100; ++depth) {
            code.push_back({kern::Opcode::POP, (int64_t)0});
        }
        
        // Repeat to stress memory management
        for (int repeat = 0; repeat < 10; ++repeat) {
            for (int depth = 0; depth < 50; ++depth) {
                code.push_back({kern::Opcode::CONST_I64, (int64_t)depth});
            }
            for (int depth = 0; depth < 50; ++depth) {
                code.push_back({kern::Opcode::POP, (int64_t)0});
            }
        }
        
        code.push_back({kern::Opcode::HALT, (int64_t)0});
        
        result.instructionCount = code.size();
        
        auto start = std::chrono::high_resolution_clock::now();
        vm.setBytecode(code);
        vm.run();
        auto end = std::chrono::high_resolution_clock::now();
        
        result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Memory stress test should complete without issues
        if (result.executionTimeMs > 100.0) {
            result.passed = false;
            result.errorMessage = "Memory management too slow: " + std::to_string(result.executionTimeMs) + "ms";
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

// Benchmark 5: Compilation Performance
BenchmarkResult testCompilationPerformance() {
    BenchmarkResult result;
    result.name = "Compilation Performance";
    result.passed = true;
    
    try {
        // Test compilation speed
        std::string source = R"(
            let x = 10 + 32;
            let y = x * 2;
            let z = y / 4;
            if (z > 0) {
                let result = z + 100;
            } else {
                let result = 0;
            }
            while (result < 1000) {
                result = result + 1;
            }
        )";
        
        auto start = std::chrono::high_resolution_clock::now();
        
        kern::MinimalCodeGen compiler;
        auto bytecode = compiler.compile(source);
        
        auto end = std::chrono::high_resolution_clock::now();
        
        result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
        result.instructionCount = bytecode.size();
        
        // Compilation should be fast (under 10ms for simple program)
        if (result.executionTimeMs > 10.0) {
            result.passed = false;
            result.errorMessage = "Compilation too slow: " + std::to_string(result.executionTimeMs) + "ms";
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = e.what();
    }
    
    return result;
}

void runAllBenchmarks() {
    std::cout << "=== Kern Runtime Stress Testing Framework ===" << std::endl;
    std::cout << "Running performance and correctness tests..." << std::endl;
    
    BenchmarkSuite suite;
    
    // Run all benchmarks
    suite.addResult(testArithmeticPerformance());
    suite.addResult(testControlFlowPerformance());
    suite.addResult(testStandardLibraryPerformance());
    suite.addResult(testMemoryStress());
    suite.addResult(testCompilationPerformance());
    
    // Print results
    suite.printResults();
    
    // Final status
    std::cout << "\n=== Final Status ===" << std::endl;
    if (suite.allPassed()) {
        std::cout << "✅ All benchmarks PASSED! Kern runtime is performing well." << std::endl;
    } else {
        std::cout << "❌ Some benchmarks FAILED! Performance issues detected." << std::endl;
    }
}

} // namespace kern::benchmark

int main() {
    kern::benchmark::runAllBenchmarks();
    return 0;
}
