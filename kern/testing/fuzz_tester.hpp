/* *
 * kern/testing/fuzz_tester.hpp - Fuzz Testing System
 * 
 * Generates random valid/invalid programs to stress test:
 * - Parser (shouldn't crash on garbage)
 * - Compiler (shouldn't crash on edge cases)
 * - Verifier (must reject invalid bytecode)
 * - VM (must not crash under any input)
 */
#pragma once

#include "../compiler/minimal_codegen.hpp"
#include "../compiler/scope_codegen.hpp"
#include "../runtime/vm_minimal.hpp"
#include "../runtime/bytecode_verifier.hpp"
#include "../runtime/bytecode_verifier_v2.hpp"
#include "../runtime/vm_limited.hpp"
#include <random>
#include <sstream>
#include <chrono>
#include <fstream>

namespace kern {
namespace testing {

struct FuzzResult {
    int iterations;
    int crashes;
    int verifierRejects;
    int compileFails;
    int timeoutHits;
    std::vector<std::string> crashCases;
    std::vector<std::string> findings;
    double durationSeconds;
};

// Random program generator
class ProgramGenerator {
    std::mt19937 rng;
    std::uniform_int_distribution<> dist;
    int maxDepth;
    int currentDepth;
    
public:
    explicit ProgramGenerator(int seed = 0, int depth = 5) 
        : rng(seed), dist(0, 100), maxDepth(depth), currentDepth(0) {}
    
    void reseed(int seed) {
        rng.seed(seed);
    }
    
    // Generate completely random garbage
    std::string generateGarbage(int length = 100) {
        std::string garbage;
        garbage.reserve(length);
        
        const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{}|;':\",./<>?\n\t ";
        std::uniform_int_distribution<> charDist(0, strlen(chars) - 1);
        
        for (int i = 0; i < length; i++) {
            garbage += chars[charDist(rng)];
        }
        
        return garbage;
    }
    
    // Generate random but syntactically valid-ish Kern
    std::string generateRandomProgram() {
        currentDepth = 0;
        return generateStatement();
    }
    
    // Generate specific crash-triggering patterns
    std::string generateCrashPattern(int pattern) {
        switch (pattern % 10) {
            case 0: return generateDeepNesting();
            case 1: return generateManyVariables();
            case 2: return generateUnbalancedBraces();
            case 3: return generateInvalidTokens();
            case 4: return generateLongExpression();
            case 5: return generateEmptyLoops();
            case 6: return generateDivisionByZero();
            case 7: return generateStringToNumber();
            case 8: return generateInfiniteLoop();
            case 9: return generateReservedWords();
            default: return generateGarbage();
        }
    }
    
private:
    int randInt(int min, int max) {
        return std::uniform_int_distribution<>(min, max)(rng);
    }
    
    std::string generateStatement() {
        if (++currentDepth > maxDepth) {
            return generateExpression() + "\n";
        }
        
        int choice = randInt(0, 6);
        switch (choice) {
            case 0: return "let " + generateIdentifier() + " = " + generateExpression() + "\n";
            case 1: return "print " + generateExpression() + "\n";
            case 2: return "if (" + generateExpression() + ") { " + generateStatement() + " }\n";
            case 3: return "while (" + generateExpression() + ") { " + generateStatement() + " }\n";
            case 4: return "{ " + generateStatement() + generateStatement() + " }\n";
            case 5: return generateExpression() + "\n";
            default: return "\n";
        }
    }
    
    std::string generateExpression() {
        int choice = randInt(0, 8);
        switch (choice) {
            case 0: return std::to_string(randInt(-1000, 1000));
            case 1: return "\"" + generateIdentifier() + "\"";
            case 2: return generateIdentifier();
            case 3: return "(" + generateExpression() + ")";
            case 4: return generateExpression() + " + " + generateExpression();
            case 5: return generateExpression() + " * " + generateExpression();
            case 6: return generateExpression() + " < " + generateExpression();
            case 7: return "-" + generateExpression();
            default: return "true";
        }
    }
    
    std::string generateIdentifier() {
        const char* names[] = {"x", "y", "z", "a", "b", "c", "foo", "bar", "baz", "i", "j", "k", 
                               "sum", "count", "result", "temp", "val", "data"};
        return names[randInt(0, 17)];
    }
    
    std::string generateDeepNesting() {
        std::string result = "let x = 0\n";
        for (int i = 0; i < 100; i++) {
            result += "if (true) {\n";
        }
        result += "let x = x + 1\n";
        for (int i = 0; i < 100; i++) {
            result += "}\n";
        }
        return result;
    }
    
    std::string generateManyVariables() {
        std::string result;
        for (int i = 0; i < 200; i++) {
            result += "let var" + std::to_string(i) + " = " + std::to_string(i) + "\n";
        }
        return result;
    }
    
    std::string generateUnbalancedBraces() {
        int choice = randInt(0, 3);
        switch (choice) {
            case 0: return "if (true) { print 1";  // Missing close
            case 1: return "if (true) print 1 }";  // Extra close
            case 2: return "{ { { } }";  // Unbalanced nesting
            default: return "} else {";  // Nonsense
        }
    }
    
    std::string generateInvalidTokens() {
        const char* invalid[] = {
            "@#$%^", "`~", "\\", "|", "??", "!!", "##",
            "0xGG", "0b2", "999999999999999999999999999999"
        };
        return invalid[randInt(0, 8)];
    }
    
    std::string generateLongExpression() {
        std::string expr = "1";
        for (int i = 0; i < 1000; i++) {
            expr += " + 1";
        }
        return "print " + expr;
    }
    
    std::string generateEmptyLoops() {
        return "while (false) { }\nwhile (true) { break }\nfor (;;) { }";
    }
    
    std::string generateDivisionByZero() {
        return "print 1 / 0\nprint 10 % 0";
    }
    
    std::string generateStringToNumber() {
        return "let x = \"hello\" + 5\nlet y = 10 * \"world\"";
    }
    
    std::string generateInfiniteLoop() {
        return "while (true) { print 1 }";
    }
    
    std::string generateReservedWords() {
        const char* reserved[] = {"let let", "if if", "while while", "print print",
                                   "true false", "nil nil", "else else"};
        return reserved[randInt(0, 6)];
    }
};

// Fuzz test runner
class FuzzTester {
    ProgramGenerator gen;
    int iterations;
    int seed;
    bool saveCrashes;
    std::string crashDir;
    
public:
    FuzzTester(int iters = 10000, int s = 42, bool save = true) 
        : gen(s), iterations(iters), seed(s), saveCrashes(save) {
        crashDir = "fuzz_crashes_" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    FuzzResult run() {
        FuzzResult result;
        result.iterations = iterations;
        result.crashes = 0;
        result.verifierRejects = 0;
        result.compileFails = 0;
        result.timeoutHits = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::cout << "Starting fuzz test: " << iterations << " iterations\n";
        std::cout << "Seed: " << seed << "\n";
        std::cout << "Progress: " << std::flush;
        
        int reportInterval = iterations / 10;
        
        for (int i = 0; i < iterations; i++) {
            if (i % reportInterval == 0) {
                std::cout << (i / reportInterval) * 10 << "% " << std::flush;
            }
            
            // Generate test case
            std::string testCase;
            int mode = i % 3;
            
            switch (mode) {
                case 0:
                    testCase = gen.generateRandomProgram();
                    break;
                case 1:
                    testCase = gen.generateGarbage(rand() % 500);
                    break;
                case 2:
                    testCase = gen.generateCrashPattern(i);
                    break;
            }
            
            // Run test
            try {
                runTestCase(testCase, result);
            } catch (const std::exception& e) {
                result.crashes++;
                std::string caseId = "crash_" + std::to_string(i) + "_" + 
                                    std::to_string(result.crashes);
                result.crashCases.push_back(caseId + ": " + e.what());
                
                if (saveCrashes) {
                    saveCrashCase(caseId, testCase, e.what());
                }
            } catch (...) {
                result.crashes++;
                std::string caseId = "crash_" + std::to_string(i) + "_unknown";
                result.crashCases.push_back(caseId + ": Unknown exception");
                
                if (saveCrashes) {
                    saveCrashCase(caseId, testCase, "Unknown");
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        result.durationSeconds = std::chrono::duration<double>(end - start).count();
        
        std::cout << "100%\n";
        
        return result;
    }
    
    void printReport(const FuzzResult& result) {
        std::cout << "\n========================================\n";
        std::cout << "FUZZ TEST RESULTS\n";
        std::cout << "========================================\n";
        std::cout << "Iterations:      " << result.iterations << "\n";
        std::cout << "Duration:        " << result.durationSeconds << "s\n";
        std::cout << "Iterations/sec:  " << (result.iterations / result.durationSeconds) << "\n";
        std::cout << "\n";
        std::cout << "Crashes:         " << result.crashes;
        if (result.crashes == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        std::cout << "Verifier rejects: " << result.verifierRejects << "\n";
        std::cout << "Compile failures: " << result.compileFails << "\n";
        std::cout << "Timeouts:        " << result.timeoutHits << "\n";
        std::cout << "\n";
        
        if (!result.crashCases.empty()) {
            std::cout << "CRASH DETAILS:\n";
            for (const auto& c : result.crashCases) {
                std::cout << "  " << c << "\n";
            }
        }
        
        if (!result.findings.empty()) {
            std::cout << "\nOTHER FINDINGS:\n";
            for (const auto& f : result.findings) {
                std::cout << "  " << f << "\n";
            }
        }
        
        if (result.crashes == 0 && result.findings.empty()) {
            std::cout << "✓ NO BUGS FOUND\n";
        }
        
        std::cout << "========================================\n";
    }
    
private:
    void runTestCase(const std::string& code, FuzzResult& result) {
        // Try compiling
        ScopeCodeGen codegen;
        auto compileResult = codegen.compile(code);
        
        if (!compileResult.success) {
            result.compileFails++;
            // This is expected for garbage input
            return;
        }
        
        // Try verifying bytecode
        ComprehensiveVerifier verifier;
        if (!verifier.verify(compileResult.code, compileResult.constants)) {
            result.verifierRejects++;
            // This is good - verifier caught something
            return;
        }
        
        // Try executing with limits
        LimitedVM vm({
            .maxInstructions = 100000,
            .maxTimeMs = 1000,
            .enableStats = false
        });
        
        auto execResult = vm.executeLimited(compileResult.code, compileResult.constants);
        
        if (!execResult.ok()) {
            // Check if it's a timeout
            std::string err = execResult.error().message;
            if (err.find("limit") != std::string::npos ||
                err.find("time") != std::string::npos) {
                result.timeoutHits++;
            }
            // Non-ok results are fine as long as no crash
        }
    }
    
    void saveCrashCase(const std::string& id, const std::string& code, const std::string& reason) {
        // In real implementation, create directory and save files
        // For now, just log
        (void)id;
        (void)code;
        (void)reason;
    }
};

} // namespace testing
} // namespace kern
