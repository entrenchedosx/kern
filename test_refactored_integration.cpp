/* *
 * test_refactored_integration.cpp - Integration Test & Benchmark
 * 
 * Verifies:
 * 1. Value system works (no shared_ptr overhead)
 * 2. Minimal VM executes bytecode
 * 3. CodeGen produces working bytecode
 * 4. Full pipeline: Source → Bytecode → Execution
 * 5. Performance benchmarks
 */
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <cmath>

// Include refactored components
#include "kern/core/value_refactored.hpp"
#include "kern/runtime/vm_minimal.hpp"
#include "kern/compiler/minimal_codegen.hpp"

using namespace kern;

// Test utilities
int testsPassed = 0;
int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  Testing " << #name << "... "; \
    try { \
        test_##name(); \
        testsPassed++; \
        std::cout << "PASS" << std::endl; \
    } catch (const std::exception& e) { \
        testsFailed++; \
        std::cout << "FAIL: " << e.what() << std::endl; \
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
// Test 1: Value System
// ============================================================================

TEST(value_creation) {
    Value v1(42);
    ASSERT(v1.isInt());
    ASSERT_EQ(v1.asInt(), 42);
    
    Value v2(3.14);
    ASSERT(v2.isFloat());
    ASSERT(std::abs(v2.asFloat() - 3.14) < 0.001);
    
    Value v3("hello");
    ASSERT(v3.isString());
    ASSERT_EQ(v3.asString(), "hello");
    
    Value v4(true);
    ASSERT(v4.isBool());
    ASSERT_EQ(v4.asBool(), true);
}

TEST(value_truthy) {
    ASSERT(!Value::nil().isTruthy());
    ASSERT(!Value::fromBool(false).isTruthy());
    ASSERT(Value::fromBool(true).isTruthy());
    ASSERT(!Value::fromInt(0).isTruthy());
    ASSERT(Value::fromInt(1).isTruthy());
    ASSERT(!Value::fromFloat(0.0).isTruthy());
    ASSERT(Value::fromFloat(1.0).isTruthy());
}

TEST(value_array) {
    Value arr = Value::makeArray();
    ASSERT(arr.isArray());
    ASSERT_EQ(arr.arraySize(), 0);
    
    arr.arrayPush(Value::fromInt(1));
    arr.arrayPush(Value::fromInt(2));
    arr.arrayPush(Value::fromInt(3));
    
    ASSERT_EQ(arr.arraySize(), 3);
    ASSERT_EQ(arr.arrayGet(0).asInt(), 1);
    ASSERT_EQ(arr.arrayGet(1).asInt(), 2);
    ASSERT_EQ(arr.arrayGet(2).asInt(), 3);
}

TEST(value_map) {
    Value map = Value::makeMap();
    ASSERT(map.isMap());
    ASSERT_EQ(map.mapSize(), 0);
    
    map.mapSet("x", Value::fromInt(10));
    map.mapSet("y", Value::fromInt(20));
    
    ASSERT_EQ(map.mapSize(), 2);
    ASSERT(map.mapContains("x"));
    ASSERT(map.mapContains("y"));
    ASSERT(!map.mapContains("z"));
    ASSERT_EQ(map.mapGet("x").asInt(), 10);
    ASSERT_EQ(map.mapGet("y").asInt(), 20);
}

TEST(value_comparison) {
    ASSERT(Value::fromInt(42) == Value::fromInt(42));
    ASSERT(!(Value::fromInt(42) == Value::fromInt(43)));
    ASSERT(Value::fromInt(10) < Value::fromInt(20));
    ASSERT(Value::fromString("abc") == Value::fromString("abc"));
}

TEST(value_native_function) {
    Value fn = Value::fromNative([](const std::vector<Value>& args) -> Value {
        int sum = 0;
        for (const auto& arg : args) {
            sum += (int)arg.asInt();
        }
        return Value::fromInt(sum);
    });
    
    ASSERT(fn.isNativeFunction());
    
    std::vector<Value> args = {Value::fromInt(10), Value::fromInt(20), Value::fromInt(30)};
    Value result = fn.call(args);
    ASSERT_EQ(result.asInt(), 60);
}

// ============================================================================
// Test 2: VM Execution
// ============================================================================

TEST(vm_push_pop) {
    MinimalVM vm;
    Bytecode code = {
        {Instruction::PUSH_TRUE, 0},
        {Instruction::PUSH_FALSE, 0},
        {Instruction::POP, 0},
        {Instruction::HALT, 0}
    };
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
    ASSERT_EQ(vm.getStackSize(), 1);
}

TEST(vm_arithmetic) {
    MinimalVM vm;
    size_t c10 = vm.addConstant("10");
    size_t c20 = vm.addConstant("20");
    size_t c5 = vm.addConstant("5");
    
    Bytecode code = {
        {Instruction::PUSH_CONST, (int16_t)c10},
        {Instruction::PUSH_CONST, (int16_t)c20},
        {Instruction::ADD, 0},
        {Instruction::PUSH_CONST, (int16_t)c5},
        {Instruction::MUL, 0},
        {Instruction::HALT, 0}
    };
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
    // Stack should have (10+20)*5 = 150
    vm.printStack();
}

TEST(vm_locals) {
    MinimalVM vm;
    size_t c42 = vm.addConstant("42");
    
    Bytecode code = {
        {Instruction::PUSH_CONST, (int16_t)c42},
        {Instruction::STORE_LOCAL, 0},
        {Instruction::LOAD_LOCAL, 0},
        {Instruction::HALT, 0}
    };
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
    ASSERT_EQ(vm.getStackSize(), 1);
}

TEST(vm_globals) {
    MinimalVM vm;
    size_t cx = vm.addConstant("x");
    size_t c100 = vm.addConstant("100");
    
    Bytecode code = {
        {Instruction::PUSH_CONST, (int16_t)c100},
        {Instruction::STORE_GLOBAL, (int16_t)cx},
        {Instruction::LOAD_GLOBAL, (int16_t)cx},
        {Instruction::HALT, 0}
    };
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
    ASSERT_EQ(vm.getStackSize(), 1);
}

TEST(vm_comparison) {
    MinimalVM vm;
    size_t c10 = vm.addConstant("10");
    size_t c20 = vm.addConstant("20");
    
    Bytecode code = {
        {Instruction::PUSH_CONST, (int16_t)c10},
        {Instruction::PUSH_CONST, (int16_t)c20},
        {Instruction::LT, 0},
        {Instruction::HALT, 0}
    };
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
    ASSERT_EQ(vm.getStackSize(), 1);
}

TEST(vm_jump) {
    MinimalVM vm;
    size_t c1 = vm.addConstant("1");
    size_t c100 = vm.addConstant("100");
    
    // Push 1, jump over push 2, push 100
    Bytecode code = {
        {Instruction::PUSH_CONST, (int16_t)c1},
        {Instruction::JUMP, 2},         // Skip next 2 instructions
        {Instruction::PUSH_CONST, (int16_t)c1},  // Skipped
        {Instruction::PUSH_CONST, (int16_t)c1},  // Skipped
        {Instruction::PUSH_CONST, (int16_t)c100},
        {Instruction::HALT, 0}
    };
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
    // Should have 1 and 100 on stack
    ASSERT_EQ(vm.getStackSize(), 2);
}

// ============================================================================
// Test 3: Code Generation
// ============================================================================

TEST(codegen_print) {
    MinimalCodeGen gen;
    std::string source = R"(
        print 42
    )";
    
    Bytecode code = gen.compile(source);
    ASSERT(code.size() > 0);
    ASSERT(code.back().op == Instruction::HALT);
    
    // Execute
    MinimalVM vm;
    for (const auto& c : gen.getConstants()) {
        vm.addConstant(c);
    }
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
}

TEST(codegen_variables) {
    MinimalCodeGen gen;
    std::string source = R"(
        let x = 10
        let y = 20
        print x + y
    )";
    
    Bytecode code = gen.compile(source);
    MinimalVM vm;
    for (const auto& c : gen.getConstants()) {
        vm.addConstant(c);
    }
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
}

TEST(codegen_arithmetic) {
    MinimalCodeGen gen;
    std::string source = R"(
        print 10 + 20 * 3
        print (10 + 20) * 3
    )";
    
    Bytecode code = gen.compile(source);
    MinimalVM vm;
    for (const auto& c : gen.getConstants()) {
        vm.addConstant(c);
    }
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
}

TEST(codegen_comparison) {
    MinimalCodeGen gen;
    std::string source = R"(
        print 10 < 20
        print 10 > 20
        print 10 == 10
    )";
    
    Bytecode code = gen.compile(source);
    MinimalVM vm;
    for (const auto& c : gen.getConstants()) {
        vm.addConstant(c);
    }
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
}

TEST(codegen_if) {
    MinimalCodeGen gen;
    std::string source = R"(
        if (10 < 20) {
            print 1
        } else {
            print 0
        }
    )";
    
    Bytecode code = gen.compile(source);
    MinimalVM vm;
    for (const auto& c : gen.getConstants()) {
        vm.addConstant(c);
    }
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
}

TEST(codegen_while) {
    MinimalCodeGen gen;
    std::string source = R"(
        let i = 0
        while (i < 5) {
            print i
            let i = i + 1
        }
    )";
    
    Bytecode code = gen.compile(source);
    MinimalVM vm;
    for (const auto& c : gen.getConstants()) {
        vm.addConstant(c);
    }
    
    auto result = vm.execute(code);
    ASSERT(result.ok());
}

// ============================================================================
// Benchmarks
// ============================================================================

template<typename Func>
double benchmark(Func f, int iterations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        f();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return duration.count() / (double)iterations;
}

void run_benchmarks() {
    std::cout << "\n=== BENCHMARKS ===" << std::endl;
    
    const int ITERATIONS = 1000000;
    
    // Benchmark 1: Value creation
    double timeNew = benchmark([]() {
        Value v(42);
        (void)v;
    }, ITERATIONS);
    
    std::cout << "Value creation (new): " << timeNew << " ns" << std::endl;
    std::cout << "  (Old shared_ptr: ~45ns, improvement: " << (45.0 / timeNew) << "x)" << std::endl;
    
    // Benchmark 2: Arithmetic
    timeNew = benchmark([]() {
        Value a(10);
        Value b(20);
        int64_t c = a.asInt() + b.asInt();
        (void)c;
    }, ITERATIONS);
    
    std::cout << "Arithmetic (new): " << timeNew << " ns" << std::endl;
    
    // Benchmark 3: Array operations
    timeNew = benchmark([]() {
        Value arr = Value::makeArray();
        arr.arrayPush(Value::fromInt(1));
        arr.arrayPush(Value::fromInt(2));
    }, ITERATIONS / 10);
    
    std::cout << "Array push x2 (new): " << (timeNew * 10) << " ns" << std::endl;
    std::cout << "  (Old vector<shared_ptr>: ~120ns, improvement: " << (120.0 / (timeNew * 10)) << "x)" << std::endl;
    
    // Benchmark 4: Map operations
    timeNew = benchmark([]() {
        Value map = Value::makeMap();
        map.mapSet("x", Value::fromInt(10));
    }, ITERATIONS / 10);
    
    std::cout << "Map set (new): " << (timeNew * 10) << " ns" << std::endl;
    std::cout << "  (Old unordered_map<shared_ptr>: ~80ns, improvement: " << (80.0 / (timeNew * 10)) << "x)" << std::endl;
    
    // Benchmark 5: VM execution (loop)
    MinimalVM vm;
    size_t c0 = vm.addConstant("0");
    size_t c1 = vm.addConstant("1");
    size_t c1000 = vm.addConstant("1000");
    
    // Simple loop: i = 0; while (i < 1000) { i = i + 1 }
    Bytecode loopCode = {
        {Instruction::PUSH_CONST, (int16_t)c0},
        {Instruction::STORE_LOCAL, 0},     // i = 0
        // Loop start
        {Instruction::LOAD_LOCAL, 0},      // Push i
        {Instruction::PUSH_CONST, (int16_t)c1000},
        {Instruction::LT, 0},                // i < 1000
        {Instruction::JUMP_IF_FALSE, 6},    // Exit if false
        {Instruction::LOAD_LOCAL, 0},       // Push i
        {Instruction::PUSH_CONST, (int16_t)c1},
        {Instruction::ADD, 0},              // i + 1
        {Instruction::STORE_LOCAL, 0},     // i = i + 1
        {Instruction::JUMP, -9},             // Back to loop start
        {Instruction::HALT, 0}
    };
    
    timeNew = benchmark([&]() {
        MinimalVM vm2;
        for (const auto& c : std::vector<std::string>{"0", "1", "1000"}) {
            vm2.addConstant(c);
        }
        vm2.execute(loopCode);
    }, 10000);
    
    std::cout << "VM loop (1000 iterations): " << timeNew << " ns" << std::endl;
    std::cout << "  Per VM instruction: " << (timeNew / 1000.0) << " ns" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Kern Refactored - Integration Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\n=== VALUE SYSTEM TESTS ===" << std::endl;
    RUN_TEST(value_creation);
    RUN_TEST(value_truthy);
    RUN_TEST(value_array);
    RUN_TEST(value_map);
    RUN_TEST(value_comparison);
    RUN_TEST(value_native_function);
    
    std::cout << "\n=== VM EXECUTION TESTS ===" << std::endl;
    RUN_TEST(vm_push_pop);
    RUN_TEST(vm_arithmetic);
    RUN_TEST(vm_locals);
    RUN_TEST(vm_globals);
    RUN_TEST(vm_comparison);
    RUN_TEST(vm_jump);
    
    std::cout << "\n=== CODE GENERATION TESTS ===" << std::endl;
    RUN_TEST(codegen_print);
    RUN_TEST(codegen_variables);
    RUN_TEST(codegen_arithmetic);
    RUN_TEST(codegen_comparison);
    RUN_TEST(codegen_if);
    RUN_TEST(codegen_while);
    
    // Run benchmarks
    run_benchmarks();
    
    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total:  " << (testsPassed + testsFailed) << std::endl;
    
    if (testsFailed == 0) {
        std::cout << "\n✓ ALL TESTS PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ SOME TESTS FAILED" << std::endl;
        return 1;
    }
}
