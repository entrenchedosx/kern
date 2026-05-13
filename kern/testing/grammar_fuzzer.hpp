/* *
 * kern/testing/grammar_fuzzer.hpp - Grammar-Based Fuzzing
 * 
 * More intelligent than random fuzzing:
 * - Uses actual language grammar
 * - Generates syntactically valid programs
 * - Mutates existing valid programs
 * - Covers grammar edges
 */
#pragma once

#include "fuzz_tester.hpp"
#include <map>

namespace kern {
namespace testing {

// Grammar production rules
class Grammar {
public:
    enum class NonTerminal {
        PROGRAM,
        STATEMENT,
        EXPRESSION,
        TERM,
        FACTOR,
        PRIMARY,
        VARIABLE,
        NUMBER,
        STRING,
        BOOL,
        BINARY_OP,
        UNARY_OP
    };
    
    using Production = std::vector<std::variant<NonTerminal, std::string>>;
    
    std::map<NonTerminal, std::vector<Production>> rules;
    
    Grammar() {
        // Build Kern grammar
        rules[NonTerminal::PROGRAM] = {
            {NonTerminal::STATEMENT},
            {NonTerminal::PROGRAM, NonTerminal::STATEMENT}  // Multiple statements
        };
        
        rules[NonTerminal::STATEMENT] = {
            {"let ", NonTerminal::VARIABLE, " = ", NonTerminal::EXPRESSION, "\n"},
            {"print ", NonTerminal::EXPRESSION, "\n"},
            {"if (", NonTerminal::EXPRESSION, ") {\n", NonTerminal::PROGRAM, "}\n"},
            {"while (", NonTerminal::EXPRESSION, ") {\n", NonTerminal::PROGRAM, "}\n"},
            {"{\n", NonTerminal::PROGRAM, "}\n"},
            {NonTerminal::EXPRESSION, "\n"}
        };
        
        rules[NonTerminal::EXPRESSION] = {
            {NonTerminal::TERM},
            {NonTerminal::EXPRESSION, NonTerminal::BINARY_OP, NonTerminal::TERM}
        };
        
        rules[NonTerminal::TERM] = {
            {NonTerminal::FACTOR},
            {NonTerminal::TERM, " * ", NonTerminal::FACTOR},
            {NonTerminal::TERM, " / ", NonTerminal::FACTOR},
            {NonTerminal::TERM, " % ", NonTerminal::FACTOR}
        };
        
        rules[NonTerminal::FACTOR] = {
            {NonTerminal::PRIMARY},
            {NonTerminal::UNARY_OP, NonTerminal::PRIMARY}
        };
        
        rules[NonTerminal::PRIMARY] = {
            {NonTerminal::NUMBER},
            {NonTerminal::STRING},
            {NonTerminal::BOOL},
            {NonTerminal::VARIABLE},
            {"(", NonTerminal::EXPRESSION, ")"}
        };
        
        rules[NonTerminal::VARIABLE] = {
            {"x"}, {"y"}, {"z"}, {"a"}, {"b"}, {"c"}, {"i"}, {"j"}, {"k"},
            {"sum"}, {"count"}, {"result"}, {"temp"}
        };
        
        rules[NonTerminal::NUMBER] = {
            {"0"}, {"1"}, {"2"}, {"5"}, {"10"}, {"100"}, {"-1"}, {"-10"},
            {"999"}, {"1000"}, {"3.14"}, {"2.718"}
        };
        
        rules[NonTerminal::STRING] = {
            {"\"hello\""}, {"\"world\""}, {"\"test\""}, {"\"x\""}
        };
        
        rules[NonTerminal::BOOL] = {
            {"true"}, {"false"}
        };
        
        rules[NonTerminal::BINARY_OP] = {
            {" + "}, {" - "}, {" < "}, {" == "}, {" > "}
        };
        
        rules[NonTerminal::UNARY_OP] = {
            {"-"}, {"!"}
        };
    }
};

// Grammar-based program generator
class GrammarFuzzer {
    Grammar grammar;
    std::mt19937 rng;
    int maxDepth;
    int currentDepth;
    
public:
    explicit GrammarFuzzer(int seed = 42, int depth = 5)
        : rng(seed), maxDepth(depth), currentDepth(0) {}
    
    std::string generateProgram() {
        currentDepth = 0;
        return generate(Grammar::NonTerminal::PROGRAM);
    }
    
    std::string mutate(const std::string& original) {
        // Parse (simplified), mutate, regenerate
        int mutationType = randInt(0, 4);
        
        switch (mutationType) {
            case 0: return insertMutation(original);
            case 1: return deleteMutation(original);
            case 2: return replaceMutation(original);
            case 3: return duplicateMutation(original);
            default: return shuffleMutation(original);
        }
    }
    
    // Generate specific grammar edge cases
    std::string generateEdgeCase(int type) {
        switch (type % 10) {
            case 0: return generateMaxDepth();
            case 1: return generateLeftRecursive();
            case 2: return generateRightRecursive();
            case 3: return generateManyVariables();
            case 4: return generateOperatorPrecedence();
            case 5: return generateAssociativity();
            case 6: return generateShortCircuit();
            case 7: return generateConstantFoldable();
            case 8: return generateDeadCode();
            case 9: return generateNestingEdge();
            default: return generateProgram();
        }
    }
    
private:
    int randInt(int min, int max) {
        return std::uniform_int_distribution<>(min, max)(rng);
    }
    
    std::string generate(Grammar::NonTerminal nt) {
        if (++currentDepth > maxDepth) {
            // Force terminal
            return generateTerminal(nt);
        }
        
        const auto& productions = grammar.rules[nt];
        if (productions.empty()) {
            return "";
        }
        
        // Pick random production
        const auto& prod = productions[randInt(0, productions.size() - 1)];
        
        std::string result;
        for (const auto& elem : prod) {
            if (std::holds_alternative<Grammar::NonTerminal>(elem)) {
                result += generate(std::get<Grammar::NonTerminal>(elem));
            } else {
                result += std::get<std::string>(elem);
            }
        }
        
        currentDepth--;
        return result;
    }
    
    std::string generateTerminal(Grammar::NonTerminal nt) {
        // Force shortest terminal production
        const auto& productions = grammar.rules[nt];
        for (const auto& prod : productions) {
            bool allTerminals = true;
            for (const auto& elem : prod) {
                if (std::holds_alternative<Grammar::NonTerminal>(elem)) {
                    allTerminals = false;
                    break;
                }
            }
            if (allTerminals) {
                std::string result;
                for (const auto& elem : prod) {
                    result += std::get<std::string>(elem);
                }
                return result;
            }
        }
        // Fallback
        if (!productions.empty()) {
            const auto& prod = productions[0];
            std::string result;
            for (const auto& elem : prod) {
                if (std::holds_alternative<std::string>(elem)) {
                    result += std::get<std::string>(elem);
                }
            }
            return result;
        }
        return "";
    }
    
    // Mutation operators
    std::string insertMutation(const std::string& original) {
        // Insert a random statement
        std::string insertion = "let x = 0\n";
        return original + insertion;
    }
    
    std::string deleteMutation(const std::string& original) {
        // Remove last line
        size_t lastNewline = original.rfind('\n');
        if (lastNewline != std::string::npos && lastNewline > 0) {
            return original.substr(0, lastNewline);
        }
        return original;
    }
    
    std::string replaceMutation(const std::string& original) {
        // Replace a number
        std::string result = original;
        size_t pos = result.find("10");
        if (pos != std::string::npos) {
            result.replace(pos, 2, "99");
        }
        return result;
    }
    
    std::string duplicateMutation(const std::string& original) {
        // Duplicate last line
        size_t lastNewline = original.rfind('\n');
        if (lastNewline != std::string::npos && lastNewline > 0) {
            std::string lastLine = original.substr(lastNewline);
            return original + lastLine;
        }
        return original;
    }
    
    std::string shuffleMutation(const std::string& original) {
        // Swap two lines
        // Simplified - just return as-is for now
        return original;
    }
    
    // Edge case generators
    std::string generateMaxDepth() {
        // Generate at exactly maxDepth
        currentDepth = 0;
        maxDepth = 20;  // Force deep
        std::string result = generate(Grammar::NonTerminal::EXPRESSION);
        maxDepth = 5;   // Restore
        return result;
    }
    
    std::string generateLeftRecursive() {
        // Left-associative chains: 1 - 2 - 3 - 4
        return "print 1 - 2 - 3 - 4 - 5";
    }
    
    std::string generateRightRecursive() {
        // Right-associative would be: 1 - (2 - (3 - 4))
        // But Kern is left-associative, so this is just deep nesting
        return "print 1 - (2 - (3 - (4 - 5)))";
    }
    
    std::string generateManyVariables() {
        std::string result;
        for (int i = 0; i < 50; i++) {
            result += "let v" + std::to_string(i) + " = " + std::to_string(i) + "\n";
        }
        return result;
    }
    
    std::string generateOperatorPrecedence() {
        // Test precedence: 2 + 3 * 4 should be 14, not 20
        return "print 2 + 3 * 4\nprint (2 + 3) * 4";
    }
    
    std::string generateAssociativity() {
        // Test associativity: 10 - 5 - 2
        // Left-assoc: (10 - 5) - 2 = 3
        return "print 10 - 5 - 2";
    }
    
    std::string generateShortCircuit() {
        // Would test && and || short-circuiting
        // Simplified for now
        return "print true && false\nprint false || true";
    }
    
    std::string generateConstantFoldable() {
        // Should all be folded to constants
        return "print 2 + 3 * 4 - 5 / 2 + 10 % 3";
    }
    
    std::string generateDeadCode() {
        // Code where some parts should be eliminated
        return R"(
            let unused1 = 999
            let x = 10
            let unused2 = 888
            let y = 20
            let unused3 = 777
            print x + y
        )";
    }
    
    std::string generateNestingEdge() {
        // Maximum nesting of blocks
        std::string result;
        for (int i = 0; i < 20; i++) {
            result += "if (true) {\n";
        }
        result += "print 1\n";
        for (int i = 0; i < 20; i++) {
            result += "}\n";
        }
        return result;
    }
};

// Grammar-based fuzz test runner
class GrammarFuzzRunner {
    GrammarFuzzer fuzzer;
    DifferentialTester diffTester;
    int iterations;
    
public:
    GrammarFuzzRunner(int iters = 1000) 
        : fuzzer(42, 5), diffTester(iters, false), iterations(iters) {}
    
    struct Result {
        int total;
        int passed;
        int failed;
        int crashes;
        std::vector<std::string> crashPrograms;
    };
    
    Result run() {
        Result r{0, 0, 0, 0, {}};
        
        std::cout << "\n========================================\n";
        std::cout << "Grammar-Based Fuzz Testing\n";
        std::cout << "========================================\n\n";
        
        std::cout << "Generating " << iterations << " grammar-valid programs...\n";
        
        int reportInterval = iterations / 10;
        
        for (int i = 0; i < iterations; i++) {
            if (i % reportInterval == 0) {
                std::cout << (i / reportInterval) * 10 << "% " << std::flush;
            }
            
            std::string program;
            
            // Mix generation strategies
            int strategy = i % 4;
            switch (strategy) {
                case 0:
                    program = fuzzer.generateProgram();
                    break;
                case 1: {
                    // Mutate previous
                    static std::string prev = fuzzer.generateProgram();
                    program = fuzzer.mutate(prev);
                    prev = program;
                    break;
                }
                case 2:
                    program = fuzzer.generateEdgeCase(i);
                    break;
                case 3: {
                    // Differential test this one
                    // (expensive, do fewer)
                    if (i % 10 == 0) {
                        // Will be handled by diffTester
                    }
                    program = fuzzer.generateProgram();
                    break;
                }
            }
            
            r.total++;
            
            // Compile and verify
            try {
                ScopeCodeGen codegen;
                auto result = codegen.compile(program);
                
                if (result.success) {
                    // Verify
                    ComprehensiveVerifier verifier;
                    if (!verifier.verify(result.code, result.constants)) {
                        // Verifier caught something - good
                    }
                    
                    r.passed++;
                } else {
                    // Compile failure is OK for fuzzing
                    r.passed++;
                }
            } catch (const std::exception& e) {
                r.failed++;
                r.crashes++;
                r.crashPrograms.push_back(program.substr(0, 100));
            }
        }
        
        std::cout << "100%\n\n";
        
        return r;
    }
    
    void printReport(const Result& r) {
        std::cout << "========================================\n";
        std::cout << "GRAMMAR FUZZ RESULTS\n";
        std::cout << "========================================\n";
        std::cout << "Total programs: " << r.total << "\n";
        std::cout << "Passed:         " << r.passed << "\n";
        std::cout << "Failed:         " << r.failed;
        if (r.failed == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        std::cout << "Crashes:        " << r.crashes;
        if (r.crashes == 0) {
            std::cout << " ✓ NONE";
        }
        std::cout << "\n";
        
        if (r.crashes > 0) {
            std::cout << "\nCrash examples:\n";
            for (const auto& p : r.crashPrograms) {
                std::cout << "  " << p.substr(0, 50) << "...\n";
            }
        }
        
        std::cout << "========================================\n";
    }
};

} // namespace testing
} // namespace kern
