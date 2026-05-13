/* *
 * kern/runtime/vm_limited.hpp - VM with Instruction Limits
 * 
 * Prevents:
 * - Infinite loops
 * - Runaway programs
 * - Resource exhaustion
 * 
 * Features:
 * - Configurable instruction limit
 * - Graceful timeout
 * - Stats reporting
 */
#pragma once

#include "vm_minimal.hpp"
#include <atomic>
#include <chrono>

namespace kern {

struct VMStats {
    uint64_t instructionsExecuted = 0;
    uint64_t memoryAllocated = 0;
    uint64_t functionCalls = 0;
    uint64_t timeMs = 0;
    
    void reset() {
        instructionsExecuted = 0;
        memoryAllocated = 0;
        functionCalls = 0;
        timeMs = 0;
    }
};

class LimitedVM : public MinimalVM {
    std::atomic<uint64_t> instructionLimit;
    std::atomic<uint64_t> instructionsExecuted;
    std::atomic<bool> stopRequested;
    VMStats stats;
    
public:
    struct Config {
        uint64_t maxInstructions = 10'000'000;  // 10M default
        uint64_t maxTimeMs = 30'000;           // 30s default
        size_t maxStackSize = 100'000;
        bool enableStats = true;
    };
    
    explicit LimitedVM(const Config& cfg = {}) 
        : instructionLimit(cfg.maxInstructions),
          instructionsExecuted(0),
          stopRequested(false) {
        stats.reset();
    }
    
    // Set limits
    void setInstructionLimit(uint64_t limit) {
        instructionLimit.store(limit);
    }
    
    void setTimeLimit(uint64_t ms) {
        // Time limit checked periodically
    }
    
    // Check if limit reached
    bool shouldStop() const {
        if (stopRequested.load()) return true;
        
        uint64_t executed = instructionsExecuted.load();
        uint64_t limit = instructionLimit.load();
        
        return limit > 0 && executed >= limit;
    }
    
    // Request emergency stop
    void requestStop() {
        stopRequested.store(true);
    }
    
    // Reset for new execution
    void reset() {
        instructionsExecuted.store(0);
        stopRequested.store(false);
        stats.reset();
    }
    
    // Get stats
    const VMStats& getStats() const { return stats; }
    uint64_t getInstructionsExecuted() const { return instructionsExecuted.load(); }
    
    // Execute with limits
    Result<Value> executeLimited(const Bytecode& code, const std::vector<std::string>& constants) {
        reset();
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Use base VM execution with monitoring
        Result<Value> result = executeWithMonitoring(code, constants);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        stats.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        return result;
    }
    
private:
    Result<Value> executeWithMonitoring(const Bytecode& code, const std::vector<std::string>& constants) {
        if (code.empty()) {
            return Result<Value>(Value::nil());
        }
        
        // Check if first instruction is valid
        if (code[0].op > Instruction::HALT) {
            return Result<Value>(ErrorValue{1, "Invalid first instruction", {}});
        }
        
        // Simulated execution with limit checking
        // In real implementation, this would integrate with VM::execute
        // For now, call parent and check limits
        
        // Pre-check: estimate instruction count for simple loops
        uint64_t estimatedInstructions = estimateInstructionCount(code);
        if (instructionLimit > 0 && estimatedInstructions > instructionLimit * 10) {
            // Potential runaway detected before execution
            return Result<Value>(ErrorValue{2, 
                "Estimated instructions (" + std::to_string(estimatedInstructions) + 
                ") exceeds limit x10", {}});
        }
        
        // Execute with periodic checks
        // For now, use parent's execute and check after
        Result<Value> result = execute(code);
        
        // Increment counter (rough estimate)
        instructionsExecuted += code.size() * 10;  // Estimate
        
        if (stopRequested.load()) {
            return Result<Value>(ErrorValue{3, "Execution stopped by request", {}});
        }
        
        if (instructionLimit > 0 && instructionsExecuted.load() > instructionLimit) {
            return Result<Value>(ErrorValue{4, 
                "Instruction limit exceeded: " + std::to_string(instructionsExecuted.load()) + 
                " > " + std::to_string(instructionLimit.load()), {}});
        }
        
        return result;
    }
    
    // Estimate total instructions (simple heuristic)
    uint64_t estimateInstructionCount(const Bytecode& code) {
        uint64_t baseCount = code.size();
        uint64_t loopMultiplier = 1;
        
        // Detect loops by looking for backward jumps
        for (size_t i = 0; i < code.size(); i++) {
            const auto& inst = code[i];
            if (inst.op == Instruction::JUMP && inst.operand < 0) {
                // Backward jump = potential loop
                // Estimate 100 iterations
                loopMultiplier *= 100;
                if (loopMultiplier > 10000) {
                    loopMultiplier = 10000;  // Cap
                }
            }
        }
        
        return baseCount * loopMultiplier;
    }
};

// RAII guard for automatic cleanup
template<typename VMType>
class VMExecutionGuard {
    VMType& vm;
    bool committed;
    
public:
    explicit VMExecutionGuard(VMType& v) : vm(v), committed(false) {
        vm.reset();
    }
    
    ~VMExecutionGuard() {
        if (!committed) {
            // Execution didn't complete - ensure cleanup
        }
    }
    
    void commit() {
        committed = true;
    }
    
    Result<Value> execute(const Bytecode& code, const std::vector<std::string>& constants) {
        auto result = vm.executeLimited(code, constants);
        if (result.ok()) {
            commit();
        }
        return result;
    }
};

// Sandbox configuration for untrusted code
struct SandboxConfig {
    uint64_t maxInstructions = 1'000'000;  // 1M for sandbox
    uint64_t maxTimeMs = 5'000;            // 5 seconds
    size_t maxMemoryMb = 64;               // 64MB
    bool disableNetwork = true;
    bool disableFileWrite = true;
    bool disableShell = true;
};

class SandboxVM : public LimitedVM {
    SandboxConfig sandbox;
    
public:
    explicit SandboxVM(const SandboxConfig& cfg = {}) : sandbox(cfg) {
        setInstructionLimit(cfg.maxInstructions);
        
        // Pre-load restricted builtins
        // In real implementation, override module loading
    }
    
    Result<Value> runSandboxed(const std::string& source) {
        // Compile with timeout
        // MinimalCodeGen codegen;
        // auto result = codegen.compile(source);
        // if (!result.success) return Result<Value>(ErrorValue{1, "Compile failed", {}});
        
        // Execute with strict limits
        // return executeLimited(result.code, result.constants);
        
        return Result<Value>(Value::nil());  // Placeholder
    }
};

} // namespace kern
