/* *
 * kern/runtime/vm_metrics.hpp - Performance Metrics & Profiling
 * 
 * Tracks:
 * - Cache hit/miss rates
 * - Instruction frequency
 * - Execution time breakdown
 * - Performance counters
 */
#pragma once

#include <unordered_map>
#include <chrono>
#include <iomanip>

namespace kern {

// Instruction execution counter
struct InstructionProfile {
    std::unordered_map<uint8_t, uint64_t> counts;
    uint64_t total;
    
    InstructionProfile() : total(0) {}
    
    void record(uint8_t opcode) {
        counts[opcode]++;
        total++;
    }
    
    void printTop(int n = 10) const {
        std::cout << "\n=== Instruction Profile ===\n";
        std::cout << "Total instructions: " << total << "\n\n";
        
        // Sort by count
        std::vector<std::pair<uint8_t, uint64_t>> sorted;
        for (const auto& [op, count] : counts) {
            sorted.push_back({op, count});
        }
        std::sort(sorted.begin(), sorted.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        std::cout << std::left << std::setw(6) << "Rank"
                  << std::setw(20) << "Opcode"
                  << std::setw(15) << "Count"
                  << std::setw(10) << "%"
                  << "\n";
        std::cout << std::string(51, '-') << "\n";
        
        int i = 0;
        for (const auto& [op, count] : sorted) {
            if (i++ >= n) break;
            double pct = total > 0 ? (100.0 * count / total) : 0;
            std::cout << std::left << std::setw(6) << i
                      << std::setw(20) << opToString(op)
                      << std::setw(15) << count
                      << std::fixed << std::setprecision(2) << std::setw(10) << pct
                      << "\n";
        }
    }
    
private:
    static std::string opToString(uint8_t op) {
        switch (op) {
            case 0: return "PUSH_CONST";
            case 1: return "PUSH_NIL";
            case 2: return "PUSH_TRUE";
            case 3: return "PUSH_FALSE";
            case 4: return "POP";
            case 5: return "DUP";
            case 6: return "LOAD_LOCAL";
            case 7: return "STORE_LOCAL";
            case 8: return "LOAD_GLOBAL";
            case 9: return "STORE_GLOBAL";
            case 10: return "ADD";
            case 11: return "SUB";
            case 12: return "MUL";
            case 13: return "DIV";
            case 14: return "MOD";
            case 15: return "NEG";
            case 16: return "NOT";
            case 17: return "EQ";
            case 18: return "LT";
            case 19: return "LE";
            case 20: return "GT";
            case 21: return "GE";
            case 22: return "JUMP";
            case 23: return "JUMP_IF_FALSE";
            case 24: return "CALL";
            case 25: return "RETURN";
            case 26: return "PRINT";
            case 27: return "HALT";
            case 32: return "STORE_CONST_LOCAL";
            case 50: return "INC_LOCAL";
            case 51: return "DEC_LOCAL";
            case 56: return "LT_JUMP_IF_FALSE";
            default: return "UNKNOWN(" + std::to_string(op) + ")";
        }
    }
};

// Cache performance metrics
struct CacheMetrics {
    uint64_t hits;
    uint64_t misses;
    uint64_t invalidations;
    uint64_t promotions;     // Mono -> Poly
    uint64_t demotions;      // Poly -> Mono
    
    CacheMetrics() 
        : hits(0), misses(0), invalidations(0), 
          promotions(0), demotions(0) {}
    
    double hitRate() const {
        uint64_t total = hits + misses;
        return total > 0 ? (100.0 * hits / total) : 0.0;
    }
    
    void print() const {
        std::cout << "\n=== Cache Metrics ===\n";
        std::cout << "Cache hits:       " << hits << "\n";
        std::cout << "Cache misses:     " << misses << "\n";
        std::cout << "Hit rate:         " << std::fixed << std::setprecision(2) 
                  << hitRate() << "%\n";
        std::cout << "Invalidations:    " << invalidations << "\n";
        std::cout << "Promotions:       " << promotions << "\n";
        std::cout << "Demotions:        " << demotions << "\n";
        
        if (hitRate() > 95) {
            std::cout << "Status: EXCELLENT ✓\n";
        } else if (hitRate() > 85) {
            std::cout << "Status: GOOD\n";
        } else if (hitRate() > 70) {
            std::cout << "Status: FAIR\n";
        } else {
            std::cout << "Status: POOR (consider optimizing)\n";
        }
    }
};

// Execution time breakdown
struct TimeBreakdown {
    double compileTimeMs;
    double optimizeTimeMs;
    double verifyTimeMs;
    double executeTimeMs;
    double totalTimeMs;
    
    TimeBreakdown() 
        : compileTimeMs(0), optimizeTimeMs(0), 
          verifyTimeMs(0), executeTimeMs(0), totalTimeMs(0) {}
    
    void print() const {
        std::cout << "\n=== Execution Time Breakdown ===\n";
        std::cout << std::left << std::setw(20) << "Phase"
                  << std::setw(15) << "Time (ms)"
                  << std::setw(10) << "%"
                  << "\n";
        std::cout << std::string(45, '-') << "\n";
        
        printPhase("Compile", compileTimeMs);
        printPhase("Optimize", optimizeTimeMs);
        printPhase("Verify", verifyTimeMs);
        printPhase("Execute", executeTimeMs);
        
        std::cout << std::string(45, '-') << "\n";
        std::cout << std::left << std::setw(20) << "TOTAL"
                  << std::fixed << std::setprecision(3) << std::setw(15) << totalTimeMs
                  << std::setw(10) << "100.0"
                  << "\n";
    }
    
private:
    void printPhase(const std::string& name, double time) const {
        double pct = totalTimeMs > 0 ? (100.0 * time / totalTimeMs) : 0;
        std::cout << std::left << std::setw(20) << name
                  << std::fixed << std::setprecision(3) << std::setw(15) << time
                  << std::setw(10) << std::setprecision(1) << pct
                  << "\n";
    }
};

// VM with full metrics collection
class MetricsVM : public LimitedVM {
    InstructionProfile profile;
    CacheMetrics cacheMetrics;
    std::chrono::high_resolution_clock::time_point startTime;
    
public:
    struct Metrics {
        InstructionProfile profile;
        CacheMetrics cache;
        TimeBreakdown time;
        uint64_t instructionsExecuted;
        double instructionsPerMs;
    };
    
    Metrics getMetrics() const {
        Metrics m;
        m.profile = profile;
        m.cache = cacheMetrics;
        m.instructionsExecuted = instructionsExecuted;
        // Would fill time from external tracking
        return m;
    }
    
    void resetMetrics() {
        profile = InstructionProfile();
        cacheMetrics = CacheMetrics();
        instructionsExecuted = 0;
    }
    
    void printMetrics() const {
        auto m = getMetrics();
        
        std::cout << "\n========================================\n";
        std::cout << "VM PERFORMANCE METRICS\n";
        std::cout << "========================================\n";
        
        std::cout << "\nInstructions executed: " << m.instructionsExecuted << "\n";
        std::cout << "Instructions/ms: " << std::fixed << std::setprecision(1) 
                  << m.instructionsPerMs << "\n";
        
        m.profile.printTop(15);
        m.cache.print();
        m.time.print();
        
        std::cout << "\n========================================\n";
    }
    
protected:
    void recordInstruction(uint8_t opcode) {
        profile.record(opcode);
        instructionsExecuted++;
    }
    
    void recordCacheHit() { cacheMetrics.hits++; }
    void recordCacheMiss() { cacheMetrics.misses++; }
    void recordCacheInvalidation() { cacheMetrics.invalidations++; }
};

// Hot path detector - identifies frequently executed code
class HotPathDetector {
    std::unordered_map<size_t, uint64_t> executionCounts;
    size_t threshold;
    
public:
    explicit HotPathDetector(size_t t = 1000) : threshold(t) {}
    
    void recordPC(size_t pc) {
        executionCounts[pc]++;
    }
    
    std::vector<size_t> getHotPaths() const {
        std::vector<size_t> hot;
        for (const auto& [pc, count] : executionCounts) {
            if (count >= threshold) {
                hot.push_back(pc);
            }
        }
        return hot;
    }
    
    void printHotPaths() const {
        auto hot = getHotPaths();
        
        std::cout << "\n=== Hot Paths (threshold: " << threshold << ") ===\n";
        std::cout << "Found " << hot.size() << " hot instruction locations\n";
        
        // Sort by count
        std::vector<std::pair<size_t, uint64_t>> sorted;
        for (const auto& [pc, count] : executionCounts) {
            if (count >= threshold) {
                sorted.push_back({pc, count});
            }
        }
        std::sort(sorted.begin(), sorted.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (const auto& [pc, count] : sorted) {
            std::cout << "  PC " << pc << ": " << count << " executions\n";
        }
    }
    
    // Generate superinstruction suggestions based on hot paths
    std::vector<std::string> suggestSuperinstructions(const Bytecode& code) const {
        std::vector<std::string> suggestions;
        
        auto hot = getHotPaths();
        for (size_t pc : hot) {
            if (pc + 1 < code.size()) {
                // Check what sequences are hot
                uint8_t op1 = code[pc].op;
                uint8_t op2 = code[pc + 1].op;
                
                // Look for common patterns
                if (op1 == 6 && op2 == 10) {  // LOAD_LOCAL + ADD
                    suggestions.push_back("LOAD_LOCAL_ADD at PC " + std::to_string(pc));
                }
                if (op1 == 10 && op2 == 7) {  // ADD + STORE_LOCAL
                    suggestions.push_back("ADD_STORE_LOCAL at PC " + std::to_string(pc));
                }
            }
        }
        
        return suggestions;
    }
};

// Performance regression detector
class PerformanceRegression {
    std::unordered_map<std::string, double> baselineTimes;
    double threshold;  // Percentage change to flag
    
public:
    explicit PerformanceRegression(double t = 10.0) : threshold(t) {}
    
    void setBaseline(const std::string& test, double timeMs) {
        baselineTimes[test] = timeMs;
    }
    
    bool checkRegression(const std::string& test, double currentTimeMs) const {
        auto it = baselineTimes.find(test);
        if (it == baselineTimes.end()) return false;
        
        double change = 100.0 * (currentTimeMs - it->second) / it->second;
        
        if (change > threshold) {
            std::cout << "⚠ PERFORMANCE REGRESSION: " << test << "\n";
            std::cout << "  Baseline: " << it->second << " ms\n";
            std::cout << "  Current:  " << currentTimeMs << " ms\n";
            std::cout << "  Change:   +" << change << "%\n";
            return true;
        }
        
        if (change < -threshold) {
            std::cout << "✓ PERFORMANCE IMPROVEMENT: " << test << "\n";
            std::cout << "  Baseline: " << it->second << " ms\n";
            std::cout << "  Current:  " << currentTimeMs << " ms\n";
            std::cout << "  Change:   " << change << "%\n";
        }
        
        return false;
    }
    
    void saveBaselines(const std::string& filename) const {
        std::ofstream file(filename);
        for (const auto& [test, time] : baselineTimes) {
            file << test << " " << time << "\n";
        }
    }
    
    void loadBaselines(const std::string& filename) {
        std::ifstream file(filename);
        std::string test;
        double time;
        while (file >> test >> time) {
            baselineTimes[test] = time;
        }
    }
};

} // namespace kern
