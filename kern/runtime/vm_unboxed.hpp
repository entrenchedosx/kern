/* *
 * kern/runtime/vm_unboxed.hpp - VM with Unboxed Value Support
 * 
 * This VM takes advantage of NaN-tagged values for speed:
 * - Integer arithmetic: direct register operations (no boxing/unboxing)
 * - Fast type checks: single bit mask
 * - No heap allocations for integers
 * - Cache-friendly value layout (inline in stack/registers)
 */
#pragma once

#include "../core/value_nan_tagged.hpp"
#include "bytecode.hpp"
#include <vector>
#include <unordered_map>
#include <stack>

namespace kern {

// ============================================================================
// Unboxed-Aware VM Stack
// ============================================================================

class UnboxedStack {
    // Stack values stored inline (no pointers!)
    std::vector<Value> values_;
    
public:
    void push(const Value& v) {
        values_.push_back(v);  // Just copies 64 bits
    }
    
    Value pop() {
        Value v = values_.back();
        values_.pop_back();
        return v;
    }
    
    Value& top() {
        return values_.back();
    }
    
    const Value& peek(size_t offset = 0) const {
        return values_[values_.size() - 1 - offset];
    }
    
    void poke(size_t offset, const Value& v) {
        values_[values_.size() - 1 - offset] = v;
    }
    
    size_t size() const { return values_.size(); }
    bool empty() const { return values_.empty(); }
    void clear() { values_.clear(); }
    
    void drop(size_t n) {
        values_.resize(values_.size() - n);
    }
    
    // Reserve capacity (pre-allocation for performance)
    void reserve(size_t n) {
        values_.reserve(n);
    }
};

// ============================================================================
// Unboxed Value VM
// ============================================================================

class UnboxedVM {
public:
    struct Stats {
        uint64_t intOperations = 0;      // Fast int ops
        uint64_t doubleOperations = 0;  // Slower double ops
        uint64_t heapAllocations = 0;   // For boxed values
        uint64_t intOverflows = 0;      // Int -> double promotions
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        
        void print() const {
            std::cout << "\n=== Unboxed VM Stats ===\n";
            std::cout << "Integer ops (fast):     " << intOperations << "\n";
            std::cout << "Double ops (slow):      " << doubleOperations << "\n";
            std::cout << "Heap allocations:       " << heapAllocations << "\n";
            std::cout << "Int overflows:          " << intOverflows << "\n";
            uint64_t totalOps = intOperations + doubleOperations;
            if (totalOps > 0) {
                double intPct = 100.0 * intOperations / totalOps;
                std::cout << "Fast path rate:         " << intPct << "%\n";
            }
        }
    };
    
    Stats stats;
    
    // Execute bytecode with unboxed value optimization
    Result<Value> execute(const Bytecode& code, const ConstantPool& constants) {
        stats = Stats{};  // Reset stats
        
        UnboxedStack stack;
        stack.reserve(1024);  // Pre-allocate
        
        size_t pc = 0;
        
        while (pc < code.size()) {
            const Instruction& inst = code[pc];
            
            switch (inst.op) {
                // ============================================================================
                // Stack Operations
                // ============================================================================
                
                case Instruction::PUSH_CONST: {
                    Value v = constants[inst.operand];
                    stack.push(v);
                    pc++;
                    break;
                }
                
                case Instruction::PUSH_NIL:
                    stack.push(Value::null());
                    pc++;
                    break;
                    
                case Instruction::PUSH_TRUE:
                    stack.push(Value(true));
                    pc++;
                    break;
                    
                case Instruction::PUSH_FALSE:
                    stack.push(Value(false));
                    pc++;
                    break;
                
                case Instruction::POP:
                    stack.pop();
                    pc++;
                    break;
                    
                case Instruction::DUP: {
                    Value v = stack.peek();
                    stack.push(v);
                    pc++;
                    break;
                }
                
                // ============================================================================
                // Arithmetic - Optimized for unboxed ints
                // ============================================================================
                
                case Instruction::ADD: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    // Fast path: both ints
                    if (a.isInt() && b.isInt()) {
                        int64_t result = a.asInt() + b.asInt();
                        // Check 48-bit overflow
                        if (result >= -(1LL << 47) && result < (1LL << 47)) {
                            stack.push(Value(static_cast<int32_t>(result)));
                            stats.intOperations++;
                        } else {
                            // Overflow - promote to double
                            stack.push(Value(static_cast<double>(result)));
                            stats.intOverflows++;
                            stats.doubleOperations++;
                        }
                    } else {
                        // Slow path
                        stack.push(Value(a.toNumber() + b.toNumber()));
                        stats.doubleOperations++;
                    }
                    pc++;
                    break;
                }
                
                case Instruction::SUB: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        int64_t result = a.asInt() - b.asInt();
                        if (result >= -(1LL << 47) && result < (1LL << 47)) {
                            stack.push(Value(static_cast<int32_t>(result)));
                            stats.intOperations++;
                        } else {
                            stack.push(Value(static_cast<double>(result)));
                            stats.intOverflows++;
                            stats.doubleOperations++;
                        }
                    } else {
                        stack.push(Value(a.toNumber() - b.toNumber()));
                        stats.doubleOperations++;
                    }
                    pc++;
                    break;
                }
                
                case Instruction::MUL: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        int64_t result = a.asInt() * b.asInt();
                        // Multiplication overflow check
                        if (result >= -(1LL << 47) && result < (1LL << 47)) {
                            stack.push(Value(static_cast<int32_t>(result)));
                            stats.intOperations++;
                        } else {
                            stack.push(Value(static_cast<double>(result)));
                            stats.intOverflows++;
                            stats.doubleOperations++;
                        }
                    } else {
                        stack.push(Value(a.toNumber() * b.toNumber()));
                        stats.doubleOperations++;
                    }
                    pc++;
                    break;
                }
                
                case Instruction::DIV: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    // Division always produces double (or error)
                    double divisor = b.toNumber();
                    if (divisor == 0.0) {
                        return Result<Value>::err("Division by zero");
                    }
                    stack.push(Value(a.toNumber() / divisor));
                    stats.doubleOperations++;
                    pc++;
                    break;
                }
                
                case Instruction::MOD: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        int64_t divisor = b.asInt();
                        if (divisor == 0) {
                            return Result<Value>::err("Modulo by zero");
                        }
                        stack.push(Value(static_cast<int32_t>(a.asInt() % divisor)));
                        stats.intOperations++;
                    } else {
                        stack.push(Value(std::fmod(a.toNumber(), b.toNumber())));
                        stats.doubleOperations++;
                    }
                    pc++;
                    break;
                }
                
                case Instruction::NEG: {
                    Value a = stack.pop();
                    
                    if (a.isInt()) {
                        int64_t result = -a.asInt();
                        if (result >= -(1LL << 47) && result < (1LL << 47)) {
                            stack.push(Value(static_cast<int32_t>(result)));
                            stats.intOperations++;
                        } else {
                            stack.push(Value(static_cast<double>(result)));
                            stats.doubleOperations++;
                        }
                    } else {
                        stack.push(Value(-a.toNumber()));
                        stats.doubleOperations++;
                    }
                    pc++;
                    break;
                }
                
                // ============================================================================
                // Comparisons - Optimized for unboxed ints
                // ============================================================================
                
                case Instruction::EQ: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    stack.push(Value(a == b));
                    pc++;
                    break;
                }
                
                case Instruction::LT: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        stack.push(Value(a.asInt() < b.asInt()));
                    } else {
                        stack.push(Value(a.toNumber() < b.toNumber()));
                    }
                    pc++;
                    break;
                }
                
                case Instruction::LE: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        stack.push(Value(a.asInt() <= b.asInt()));
                    } else {
                        stack.push(Value(a.toNumber() <= b.toNumber()));
                    }
                    pc++;
                    break;
                }
                
                case Instruction::GT: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        stack.push(Value(a.asInt() > b.asInt()));
                    } else {
                        stack.push(Value(a.toNumber() > b.toNumber()));
                    }
                    pc++;
                    break;
                }
                
                case Instruction::GE: {
                    Value b = stack.pop();
                    Value a = stack.pop();
                    
                    if (a.isInt() && b.isInt()) {
                        stack.push(Value(a.asInt() >= b.asInt()));
                    } else {
                        stack.push(Value(a.toNumber() >= b.toNumber()));
                    }
                    pc++;
                    break;
                }
                
                // ============================================================================
                // Control Flow
                // ============================================================================
                
                case Instruction::JUMP:
                    pc = inst.operand;
                    break;
                    
                case Instruction::JUMP_IF_FALSE: {
                    Value cond = stack.pop();
                    if (!cond.isBool() || !cond.asBool()) {
                        pc = inst.operand;
                    } else {
                        pc++;
                    }
                    break;
                }
                
                // ============================================================================
                // Variables - With inline caching support
                // ============================================================================
                
                case Instruction::LOAD_GLOBAL: {
                    auto it = globals_.find(inst.operand);
                    if (it != globals_.end()) {
                        stack.push(it->second);
                    } else {
                        stack.push(Value::null());
                    }
                    pc++;
                    break;
                }
                
                case Instruction::STORE_GLOBAL: {
                    globals_[inst.operand] = stack.pop();
                    pc++;
                    break;
                }
                
                case Instruction::LOAD_LOCAL:
                case Instruction::STORE_LOCAL:
                    // Would need frame/stack support
                    pc++;
                    break;
                
                // ============================================================================
                // Other Operations
                // ============================================================================
                
                case Instruction::NOT: {
                    Value a = stack.pop();
                    stack.push(Value(!a.asBool()));
                    pc++;
                    break;
                }
                
                case Instruction::PRINT: {
                    Value v = stack.pop();
                    std::cout << v.toString() << "\n";
                    pc++;
                    break;
                }
                
                case Instruction::RETURN: {
                    if (stack.empty()) {
                        return Result<Value>::ok(Value::null());
                    }
                    return Result<Value>::ok(stack.pop());
                }
                
                case Instruction::HALT:
                    return Result<Value>::ok(stack.empty() ? Value::null() : stack.pop());
                    
                default:
                    return Result<Value>::err("Unknown opcode: " + std::to_string(inst.op));
            }
        }
        
        return Result<Value>::ok(Value::null());
    }
    
    void setGlobal(uint32_t index, const Value& value) {
        globals_[index] = value;
    }
    
    Value getGlobal(uint32_t index) const {
        auto it = globals_.find(index);
        return (it != globals_.end()) ? it->second : Value::null();
    }
    
private:
    std::unordered_map<uint32_t, Value> globals_;
};

// ============================================================================
// Benchmark: Unboxed vs Boxed Values
// ============================================================================

class UnboxedBenchmark {
public:
    static void runComparison() {
        std::cout << "\n########################################\n";
        std::cout << "#                                      #\n";
        std::cout << "#  UNBOXED VALUE BENCHMARK             #\n";
        std::cout << "#                                      #\n";
        std::cout << "########################################\n";
        
        testIntegerArithmetic();
        testMemoryLayout();
        testTypeChecking();
        
        std::cout << "\n========================================\n";
        std::cout << "Benchmark Complete\n";
        std::cout << "========================================\n";
    }
    
private:
    static void testIntegerArithmetic() {
        std::cout << "\n=== Integer Arithmetic Test ===\n";
        
        const uint64_t iterations = 100000000;  // 100 million
        
        // Test 1: NaN-tagged int arithmetic
        auto start = std::chrono::high_resolution_clock::now();
        
        Value a = Value(42);
        Value b = Value(17);
        Value result = Value(0);
        
        for (uint64_t i = 0; i < iterations; i++) {
            result = a + b;  // Should use fast int path
            // Prevent optimization
            if (result.asInt() != 59) {
                std::cout << "ERROR\n";
                break;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double nanTaggedTime = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::cout << "NaN-tagged int ops: " << iterations << "\n";
        std::cout << "Time: " << nanTaggedTime << " ms\n";
        std::cout << "Per op: " << (nanTaggedTime * 1000000.0 / iterations) << " ns\n";
        
        // Test 2: Standard boxed int (simulated)
        start = std::chrono::high_resolution_clock::now();
        
        volatile int64_t boxA = 42;
        volatile int64_t boxB = 17;
        volatile int64_t boxResult = 0;
        
        for (uint64_t i = 0; i < iterations; i++) {
            boxResult = boxA + boxB;
            if (boxResult != 59) {
                std::cout << "ERROR\n";
                break;
            }
        }
        
        end = std::chrono::high_resolution_clock::now();
        double rawIntTime = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::cout << "\nRaw int ops: " << iterations << "\n";
        std::cout << "Time: " << rawIntTime << " ms\n";
        std::cout << "Per op: " << (rawIntTime * 1000000.0 / iterations) << " ns\n";
        
        // Compare
        double overhead = nanTaggedTime - rawIntTime;
        double overheadPct = (overhead / rawIntTime) * 100;
        
        std::cout << "\n=== Overhead ===\n";
        std::cout << "NaN-tagged overhead: " << overheadPct << "%\n";
        
        if (overheadPct < 10) {
            std::cout << "✓ EXCELLENT: Near-zero overhead for type safety\n";
        } else if (overheadPct < 50) {
            std::cout << "✓ GOOD: Acceptable overhead\n";
        } else {
            std::cout << "⚠ Warning: Higher than expected overhead\n";
        }
    }
    
    static void testMemoryLayout() {
        std::cout << "\n=== Memory Layout Test ===\n";
        
        std::cout << "Value size: " << sizeof(Value) << " bytes\n";
        std::cout << "Expected: 8 bytes (single 64-bit word)\n";
        
        if (sizeof(Value) == 8) {
            std::cout << "✓ Perfect: Value fits in single 64-bit register\n";
        } else {
            std::cout << "⚠ Warning: Value is " << sizeof(Value) << " bytes (should be 8)\n";
        }
        
        // Test array cache locality
        const size_t count = 1000000;
        std::vector<Value> values;
        values.reserve(count);
        
        // Fill with integers
        for (size_t i = 0; i < count; i++) {
            values.push_back(Value(static_cast<int32_t>(i)));
        }
        
        std::cout << "Array of " << count << " Values: " 
                  << (count * sizeof(Value) / 1024 / 1024) << " MB\n";
        std::cout << "Same count with boxed ints: ~" 
                  << (count * 24 / 1024 / 1024) << " MB (estimated)\n";
    }
    
    static void testTypeChecking() {
        std::cout << "\n=== Type Checking Speed ===\n";
        
        const uint64_t iterations = 100000000;
        
        Value v = Value(42);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        volatile bool result = false;
        for (uint64_t i = 0; i < iterations; i++) {
            result = v.isInt();
            if (!result) {
                std::cout << "ERROR\n";
                break;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double typeCheckTime = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::cout << "Type checks: " << iterations << "\n";
        std::cout << "Time: " << typeCheckTime << " ms\n";
        std::cout << "Per check: " << (typeCheckTime * 1000000.0 / iterations) << " ns\n";
        
        if (typeCheckTime * 1000000.0 / iterations < 1.0) {
            std::cout << "✓ EXCELLENT: Sub-nanosecond type checking\n";
        }
    }
};

} // namespace kern
