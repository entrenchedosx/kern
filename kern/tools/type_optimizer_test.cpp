/* *
 * kern/tools/type_optimizer_test.cpp — Integration test for v5.0 Phase 1
 *
 * Validates:
 *   1. Typed int arithmetic emits fast-path opcodes (ADD_INT, SUB_INT, etc.)
 *   2. Typed float arithmetic emits fast-path opcodes (ADD_FLOAT, SUB_FLOAT, etc.)
 *   3. Untyped variables still work (backward compatibility)
 *   4. Mixed types fall back to generic opcodes
 *
 * Compile + run:
 *   C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:$PATH:/c/Program\ Files/CMake/bin && cd /e/kerncode/build && cmake --build . --target kern_type_optimizer_test 2>&1"
 *   ./build/kern_type_optimizer_test
 */

#include "../runtime/vm/vm.hpp"
#include "../core/compiler/lexer.hpp"
#include "../core/compiler/parser.hpp"
#include "../core/compiler/codegen.hpp"
#include "../core/bytecode/value.hpp"
#include "../runtime/vm/builtins.hpp"
#include <iostream>
#include <string>
#include <cstdlib>
#include <cmath>

using namespace kern;

// ─── Helper: compile source text into VM-ready form ─────────────────────
static bool compileSource(const std::string& source,
                          Bytecode& outCode,
                          std::vector<std::string>& outStrings,
                          std::vector<Value>& outValues,
                          std::string& errMsg) {
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto program = parser.parse();
        CodeGenerator gen;
        outCode = gen.generate(std::move(program));
        outStrings = gen.getConstants();
        outValues = gen.getValueConstants();
        return true;
    } catch (const std::exception& e) {
        errMsg = e.what();
        return false;
    }
}

// ─── Helper: check Value type ──────────────────────────────────────────
static std::string valStr(const ValuePtr& v) {
    if (!v) return "<nullptr>";
    return v->toString() + " (type=" + v->typeName() + ")";
}

// ═══════════════════════════════════════════════════════════════════════
// Test 1: Typed int addition
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedIntAdd() {
    std::cout << "  [test] typed int ADD... ";

    std::string source = R"(
        let int x = 5
        let int y = 3
        global result = x + y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::INT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (rg->asInt() != 8) { std::cout << "FAIL (value=" << rg->asInt() << ")" << std::endl; return false; }

    std::cout << "OK (" << rg->asInt() << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 2: Typed int subtraction
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedIntSub() {
    std::cout << "  [test] typed int SUB... ";

    std::string source = R"(
        let int x = 10
        let int y = 4
        global result = x - y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::INT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (rg->asInt() != 6) { std::cout << "FAIL (value=" << rg->asInt() << ")" << std::endl; return false; }

    std::cout << "OK (" << rg->asInt() << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 3: Typed int multiplication
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedIntMul() {
    std::cout << "  [test] typed int MUL... ";

    std::string source = R"(
        let int x = 6
        let int y = 7
        global result = x * y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::INT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (rg->asInt() != 42) { std::cout << "FAIL (value=" << rg->asInt() << ")" << std::endl; return false; }

    std::cout << "OK (" << rg->asInt() << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 4: Typed int division
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedIntDiv() {
    std::cout << "  [test] typed int DIV... ";

    std::string source = R"(
        let int x = 20
        let int y = 5
        global result = x / y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::INT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (rg->asInt() != 4) { std::cout << "FAIL (value=" << rg->asInt() << ")" << std::endl; return false; }

    std::cout << "OK (" << rg->asInt() << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 5: Typed float addition
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedFloatAdd() {
    std::cout << "  [test] typed float ADD... ";

    std::string source = R"(
        let float a = 2.5
        let float b = 1.5
        global result = a + b
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::FLOAT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    double expected = 4.0;
    double actual = rg->asFloat();
    if (std::abs(actual - expected) > 0.0001) { std::cout << "FAIL (value=" << actual << ")" << std::endl; return false; }

    std::cout << "OK (" << actual << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 6: Typed float subtraction
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedFloatSub() {
    std::cout << "  [test] typed float SUB... ";

    std::string source = R"(
        let float a = 10.0
        let float b = 3.5
        global result = a - b
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::FLOAT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    double expected = 6.5;
    double actual = rg->asFloat();
    if (std::abs(actual - expected) > 0.0001) { std::cout << "FAIL (value=" << actual << ")" << std::endl; return false; }

    std::cout << "OK (" << actual << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 7: Typed float multiplication
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedFloatMul() {
    std::cout << "  [test] typed float MUL... ";

    std::string source = R"(
        let float a = 3.0
        let float b = 4.5
        global result = a * b
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::FLOAT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    double expected = 13.5;
    double actual = rg->asFloat();
    if (std::abs(actual - expected) > 0.0001) { std::cout << "FAIL (value=" << actual << ")" << std::endl; return false; }

    std::cout << "OK (" << actual << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 8: Typed float division
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedFloatDiv() {
    std::cout << "  [test] typed float DIV... ";

    std::string source = R"(
        let float a = 7.5
        let float b = 2.5
        global result = a / b
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::FLOAT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    double expected = 3.0;
    double actual = rg->asFloat();
    if (std::abs(actual - expected) > 0.0001) { std::cout << "FAIL (value=" << actual << ")" << std::endl; return false; }

    std::cout << "OK (" << actual << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 9: Untyped variables still work (backward compatibility)
// ═══════════════════════════════════════════════════════════════════════
static bool testUntypedBackwardCompat() {
    std::cout << "  [test] untyped backward compat... ";

    std::string source = R"(
        let x = 5
        let y = 3
        global result = x + y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::INT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (rg->asInt() != 8) { std::cout << "FAIL (value=" << rg->asInt() << ")" << std::endl; return false; }

    std::cout << "OK (" << rg->asInt() << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 10: Typed int equality comparison
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedIntEq() {
    std::cout << "  [test] typed int EQ... ";

    std::string source = R"(
        let int x = 5
        let int y = 5
        global result = x == y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::BOOL) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (!rg->asBool()) { std::cout << "FAIL (value=false)" << std::endl; return false; }

    std::cout << "OK (true)" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 11: Typed int less-than comparison
// ═══════════════════════════════════════════════════════════════════════
static bool testTypedIntLt() {
    std::cout << "  [test] typed int LT... ";

    std::string source = R"(
        let int x = 3
        let int y = 7
        global result = x < y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::BOOL) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    if (!rg->asBool()) { std::cout << "FAIL (value=false)" << std::endl; return false; }

    std::cout << "OK (true)" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 12: Mixed types fall back (int + float → generic)
// ═══════════════════════════════════════════════════════════════════════
static bool testMixedTypeFallback() {
    std::cout << "  [test] mixed int+float fallback... ";

    std::string source = R"(
        let int x = 5
        let float y = 2.0
        global result = x + y
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);
    vm.resumeAll(0);

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::FLOAT) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }
    double expected = 7.0;
    double actual = rg->asFloat();
    if (std::abs(actual - expected) > 0.0001) { std::cout << "FAIL (value=" << actual << ")" << std::endl; return false; }

    std::cout << "OK (" << actual << ")" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Type Optimizer Integration Test (v5.0 Phase 1) ===" << std::endl;

    int passed = 0;
    int failed = 0;

    auto run = [&](const char* name, auto fn) {
        if (fn()) passed++;
        else { failed++; std::cout << "  FAILED: " << name << std::endl; }
    };

    // Typed int arithmetic
    run("testTypedIntAdd",    testTypedIntAdd);
    run("testTypedIntSub",    testTypedIntSub);
    run("testTypedIntMul",    testTypedIntMul);
    run("testTypedIntDiv",    testTypedIntDiv);

    // Typed float arithmetic
    run("testTypedFloatAdd",  testTypedFloatAdd);
    run("testTypedFloatSub",  testTypedFloatSub);
    run("testTypedFloatMul",  testTypedFloatMul);
    run("testTypedFloatDiv",  testTypedFloatDiv);

    // Backward compatibility and edge cases
    run("testUntypedBackwardCompat", testUntypedBackwardCompat);
    run("testTypedIntEq",            testTypedIntEq);
    run("testTypedIntLt",            testTypedIntLt);
    run("testMixedTypeFallback",     testMixedTypeFallback);

    std::cout << "=== " << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " (" << passed << "/" << (passed + failed) << ") ===" << std::endl;

    return failed > 0 ? 1 : 0;
}
