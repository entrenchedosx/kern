/* *
 * kern/tools/test_web_suite.cpp — Integration test for v4.0 Phase 3
 *
 * Validates:
 *   1. JSON parsing (kern_json_parse) — synchronous, zero-dependency
 *   2. Async HTTP POST (kern_http_post_async) — non-blocking coroutine
 *
 * Compile + run:
 *   C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:$PATH:/c/Program\ Files/CMake/bin && cd /e/kerncode/build && cmake --build . --target kern_web_suite_test 2>&1"
 *   ./build/kern_web_suite_test
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
#include <thread>
#include <chrono>

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
// Test 1: JSON parse a simple object
// ═══════════════════════════════════════════════════════════════════════
static bool testJsonParseObject() {
    std::cout << "  [test] JSON parse object... ";

    std::string source = R"(
        global result = kern_json_parse("{\"player\": \"aawad\", \"score\": 9500}")
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
    if (rg->type != Value::Type::MAP) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }

    // Access fields via the map (as an unordered_map)
    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(rg->data);
    auto itPlayer = m.find("player");
    auto itScore = m.find("score");
    if (itPlayer == m.end() || itScore == m.end()) {
        std::cout << "FAIL (missing fields)" << std::endl; return false;
    }
    if (itPlayer->second->type != Value::Type::STRING || itPlayer->second->asString() != "aawad") {
        std::cout << "FAIL (player: " << valStr(itPlayer->second) << ")" << std::endl; return false;
    }
    if (itScore->second->type != Value::Type::INT || itScore->second->asInt() != 9500) {
        std::cout << "FAIL (score: " << valStr(itScore->second) << ")" << std::endl; return false;
    }

    std::cout << "OK" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 2: JSON parse an array
// ═══════════════════════════════════════════════════════════════════════
static bool testJsonParseArray() {
    std::cout << "  [test] JSON parse array... ";

    std::string source = R"(
        global result = kern_json_parse("[10, 20, 30]")
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
    if (rg->type != Value::Type::ARRAY) { std::cout << "FAIL (type=" << rg->typeName() << ")" << std::endl; return false; }

    auto& arr = std::get<std::vector<ValuePtr>>(rg->data);
    if (arr.size() != 3) { std::cout << "FAIL (size=" << arr.size() << ")" << std::endl; return false; }
    if (arr[0]->type != Value::Type::INT || arr[0]->asInt() != 10) {
        std::cout << "FAIL ([0]: " << valStr(arr[0]) << ")" << std::endl; return false;
    }
    if (arr[1]->type != Value::Type::INT || arr[1]->asInt() != 20) {
        std::cout << "FAIL ([1]: " << valStr(arr[1]) << ")" << std::endl; return false;
    }
    if (arr[2]->type != Value::Type::INT || arr[2]->asInt() != 30) {
        std::cout << "FAIL ([2]: " << valStr(arr[2]) << ")" << std::endl; return false;
    }

    std::cout << "OK" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 3: JSON parse nested structure (object with array)
// ═══════════════════════════════════════════════════════════════════════
static bool testJsonParseNested() {
    std::cout << "  [test] JSON parse nested... ";

    std::string source = R"(
        global result = kern_json_parse("{\"name\": \"test\", \"values\": [1, 2, 3], \"active\": true}")
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
    if (!rg || rg->type != Value::Type::MAP) {
        std::cout << "FAIL (result: " << valStr(rg) << ")" << std::endl;
        return false;
    }

    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(rg->data);
    auto itName = m.find("name");
    auto itVals = m.find("values");
    auto itActive = m.find("active");
    if (itName == m.end() || itVals == m.end() || itActive == m.end()) {
        std::cout << "FAIL (missing fields)" << std::endl; return false;
    }
    if (itName->second->type != Value::Type::STRING || itName->second->asString() != "test") {
        std::cout << "FAIL (name: " << valStr(itName->second) << ")" << std::endl; return false;
    }
    if (itActive->second->type != Value::Type::BOOL || itActive->second->asBool() != true) {
        std::cout << "FAIL (active: " << valStr(itActive->second) << ")" << std::endl; return false;
    }
    if (itVals->second->type != Value::Type::ARRAY) {
        std::cout << "FAIL (values not array)" << std::endl; return false;
    }
    auto& arr = std::get<std::vector<ValuePtr>>(itVals->second->data);
    if (arr.size() != 3 || arr[0]->asInt() != 1) {
        std::cout << "FAIL (values content)" << std::endl; return false;
    }

    std::cout << "OK" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 4: JSON parse invalid input returns nil
// ═══════════════════════════════════════════════════════════════════════
static bool testJsonParseInvalid() {
    std::cout << "  [test] JSON parse invalid returns nil... ";

    std::string source = R"(
        global result = kern_json_parse("{invalid json!!!}")
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
    if (rg->type != Value::Type::NIL) {
        std::cout << "FAIL (expected nil, got " << rg->typeName() << ": " << rg->toString() << ")" << std::endl;
        return false;
    }

    std::cout << "OK" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 5: JSON parse JSON null, true, false
// ═══════════════════════════════════════════════════════════════════════
static bool testJsonParseKeywords() {
    std::cout << "  [test] JSON parse keywords (null/true/false)... ";

    std::string source = R"(
        global a = kern_json_parse("null")
        global b = kern_json_parse("true")
        global c = kern_json_parse("false")
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

    auto ga = vm.getGlobal("a");
    auto gb = vm.getGlobal("b");
    auto gc = vm.getGlobal("c");

    if (!ga || ga->type != Value::Type::NIL) {
        std::cout << "FAIL (null: " << valStr(ga) << ")" << std::endl; return false;
    }
    if (!gb || gb->type != Value::Type::BOOL || gb->asBool() != true) {
        std::cout << "FAIL (true: " << valStr(gb) << ")" << std::endl; return false;
    }
    if (!gc || gc->type != Value::Type::BOOL || gc->asBool() != false) {
        std::cout << "FAIL (false: " << valStr(gc) << ")" << std::endl; return false;
    }

    std::cout << "OK" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 6: Async HTTP POST (basic) — verify it doesn't block and returns
// a non-empty response. Uses http://httpbin.org/post which echoes the body.
// ═══════════════════════════════════════════════════════════════════════
static bool testHttpPostAsync() {
    std::cout << "  [test] HTTP POST async... ";

    std::string source = R"(
        global result = ""

        fn poster() {
            result = kern_http_post_async("http://httpbin.org/post", "{\"hello\": \"world\"}")
        }

        kern_start_coroutine(poster, [])
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

    uint64_t tick = 0;
    while (vm.hasActiveCoroutines() && tick < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    auto rg = vm.getGlobal("result");
    if (!rg) { std::cout << "FAIL (result missing)" << std::endl; return false; }
    if (rg->type != Value::Type::STRING) {
        std::cout << "FAIL (type=" << rg->typeName() << ", val=" << rg->toString() << ")" << std::endl;
        return false;
    }
    std::string body = rg->asString();
    if (body.empty()) {
        std::cout << "FAIL (empty response — network may be down or httpbin.org unreachable)" << std::endl;
        return false;
    }

    // Response from httpbin.org/post should contain the JSON we sent
    bool hasHello = body.find("hello") != std::string::npos;
    if (!hasHello) {
        std::cout << "FAIL (response doesn't contain 'hello', got " << body.size() << " bytes)" << std::endl;
        std::cout << "  Preview: " << body.substr(0, 200) << std::endl;
        return false;
    }

    std::cout << "OK (" << body.size() << " bytes)" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 7: End-to-end: JSON parse + HTTP POST
// ═══════════════════════════════════════════════════════════════════════
static bool testJsonParseAndPost() {
    std::cout << "  [test] JSON parse + HTTP POST pipeline... ";

    std::string source = R"(
        global payload = ""
        global response = ""

        fn worker() {
            # Step 1: Parse JSON to verify it's valid
            let data = kern_json_parse("{\"player\": \"aawad\", \"score\": 9999}")
            # Step 2: Serialize it back (we just construct a new string for now)
            let payloadStr = "{\"player\": \"aawad\", \"score\": 9999}"
            payload = payloadStr
            # Step 3: POST it
            response = kern_http_post_async("http://httpbin.org/post", payloadStr)
        }

        kern_start_coroutine(worker, [])
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

    uint64_t tick = 0;
    while (vm.hasActiveCoroutines() && tick < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    auto rg = vm.getGlobal("response");
    if (!rg || rg->type != Value::Type::STRING) {
        std::cout << "FAIL (response: " << valStr(rg) << ")" << std::endl;
        return false;
    }
    std::string body = rg->asString();
    if (body.empty()) {
        std::cout << "FAIL (empty response)" << std::endl;
        return false;
    }

    // Should contain our payload fields
    bool hasPlayer = body.find("aawad") != std::string::npos;
    bool hasScore = body.find("9999") != std::string::npos;
    if (!hasPlayer || !hasScore) {
        std::cout << "FAIL (response doesn't contain payload, got " << body.size() << "b)" << std::endl;
        std::cout << "  Preview: " << body.substr(0, 200) << std::endl;
        return false;
    }

    std::cout << "OK (" << body.size() << " bytes)" << std::endl;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Web Suite Integration Test (v4.0 Phase 3) ===" << std::endl;

    int passed = 0;
    int failed = 0;

    auto run = [&](const char* name, auto fn) {
        if (fn()) passed++;
        else { failed++; std::cout << "  FAILED: " << name << std::endl; }
    };

    // JSON parsing tests (synchronous, no network)
    run("testJsonParseObject",    testJsonParseObject);
    run("testJsonParseArray",     testJsonParseArray);
    run("testJsonParseNested",    testJsonParseNested);
    run("testJsonParseInvalid",   testJsonParseInvalid);
    run("testJsonParseKeywords",  testJsonParseKeywords);

    // Async HTTP POST tests (require network)
    run("testHttpPostAsync",      testHttpPostAsync);
    run("testJsonParseAndPost",   testJsonParseAndPost);

    std::cout << "=== " << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " (" << passed << "/" << (passed + failed) << ") ===" << std::endl;

    return failed > 0 ? 1 : 0;
}
