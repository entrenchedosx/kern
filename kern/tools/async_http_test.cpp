/* *
 * kern/tools/async_http_test.cpp — Integration test for http_get_async
 *
 * v4.0 Phase 2: Non-blocking Async HTTP Requests
 *
 * Compile + run:
 *   C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:$PATH:/c/Program\ Files/CMake/bin && cd /e/kerncode/build && cmake --build . --target kern_async_http_test 2>&1"
 *   ./build/kern_async_http_test
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

// ─── Helper: check if a ValuePtr is a string ────────────────────────────
static bool isStringValue(const ValuePtr& v) {
    if (!v) return false;
    return v->type == Value::Type::STRING;
}

// ─── Helper: check if a string value is non-empty ───────────────────────
static bool isNonEmptyString(const ValuePtr& v) {
    if (!v) return false;
    if (v->type != Value::Type::STRING) return false;
    return !v->asString().empty();
}

// ─── Helper: check if a string value contains a substring ───────────────
static bool stringContains(const ValuePtr& v, const std::string& needle) {
    if (!v || v->type != Value::Type::STRING) return false;
    return v->asString().find(needle) != std::string::npos;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 1: Basic async HTTP GET from a coroutine
// Fetches http://example.com and checks the response is non-empty.
// ═══════════════════════════════════════════════════════════════════════
static bool testHttpGetBasic() {
    std::cout << "  [test] HTTP GET http://example.com... ";

    std::string source = R"(
        global result = ""

        fn fetcher() {
            result = kern_http_get_async("http://example.com")
        }

        kern_start_coroutine(fetcher, [])
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

    // Poll resumeAll() until done (max ~5 seconds)
    uint64_t tick = 0;
    while (vm.hasActiveCoroutines() && tick < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    auto rg = vm.getGlobal("result");
    bool passed = isNonEmptyString(rg);
    if (!passed) {
        if (rg && rg->type == Value::Type::STRING) {
            std::cout << "FAIL (got empty string)" << std::endl;
        } else if (rg) {
            std::cout << "FAIL (result type=" << rg->typeName() << ", val=" << rg->toString() << ")" << std::endl;
        } else {
            std::cout << "FAIL (result global missing)" << std::endl;
        }
    }

    if (passed) std::cout << "OK (" << rg->asString().size() << " bytes)" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 2: Response contains expected HTML content
// ═══════════════════════════════════════════════════════════════════════
static bool testHttpGetContent() {
    std::cout << "  [test] HTTP GET content check... ";

    std::string source = R"(
        global result = ""

        fn fetcher() {
            result = kern_http_get_async("http://example.com")
        }

        kern_start_coroutine(fetcher, [])
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
    bool passed = stringContains(rg, "Example Domain") || stringContains(rg, "html");
    if (!passed) {
        if (rg && rg->type == Value::Type::STRING) {
            std::cout << "FAIL (response doesn't contain expected content, got " << rg->asString().size() << " bytes)" << std::endl;
            // Print first 200 chars for debugging
            std::cout << "  Response preview: " << rg->asString().substr(0, 200) << std::endl;
        } else if (rg) {
            std::cout << "FAIL (result type=" << rg->typeName() << ")" << std::endl;
        } else {
            std::cout << "FAIL (result global missing)" << std::endl;
        }
    }

    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 3: Invalid URL returns empty string
// ═══════════════════════════════════════════════════════════════════════
static bool testHttpGetInvalidUrl() {
    std::cout << "  [test] Invalid URL returns empty... ";

    std::string source = R"(
        global result = "<unset>"

        fn fetcher() {
            result = kern_http_get_async("http://invalid-host-name-that-does-not-exist.example")
        }

        kern_start_coroutine(fetcher, [])
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
    // Invalid URL should return empty string (error case)
    bool passed = (rg && rg->type == Value::Type::STRING && rg->asString().empty());
    if (!passed) {
        if (rg && rg->type == Value::Type::STRING) {
            std::cout << "FAIL (expected empty string, got '" << rg->asString().substr(0, 100) << "')" << std::endl;
        } else if (rg) {
            std::cout << "FAIL (result type=" << rg->typeName() << ")" << std::endl;
        } else {
            std::cout << "FAIL (result global missing)" << std::endl;
        }
    }

    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 4: Multiple concurrent HTTP GETs
// ═══════════════════════════════════════════════════════════════════════
static bool testHttpGetConcurrent() {
    std::cout << "  [test] Concurrent HTTP GETs... ";

    std::string source = R"(
        global result_a = ""
        global result_b = ""

        fn fetcher_a() {
            result_a = kern_http_get_async("http://example.com")
        }

        fn fetcher_b() {
            result_b = kern_http_get_async("http://example.com")
        }

        kern_start_coroutine(fetcher_a, [])
        kern_start_coroutine(fetcher_b, [])
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

    auto ga = vm.getGlobal("result_a");
    auto gb = vm.getGlobal("result_b");
    bool passed = isNonEmptyString(ga) && isNonEmptyString(gb);
    if (!passed) {
        std::cout << "FAIL (a=";
        if (ga && ga->type == Value::Type::STRING) std::cout << "'" << ga->asString().substr(0, 50) << "'";
        else std::cout << (ga ? ga->toString() : "null");
        std::cout << " b=";
        if (gb && gb->type == Value::Type::STRING) std::cout << "'" << gb->asString().substr(0, 50) << "'";
        else std::cout << (gb ? gb->toString() : "null");
        std::cout << ")" << std::endl;
    }

    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Async HTTP GET Integration Test ===" << std::endl;

    int passed = 0;
    int failed = 0;

    auto run = [&](const char* name, auto fn) {
        if (fn()) passed++;
        else { failed++; std::cout << "  FAILED: " << name << std::endl; }
    };

    run("testHttpGetBasic",       testHttpGetBasic);
    run("testHttpGetContent",     testHttpGetContent);
    run("testHttpGetInvalidUrl",  testHttpGetInvalidUrl);
    run("testHttpGetConcurrent",  testHttpGetConcurrent);

    std::cout << "=== " << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " (" << passed << "/" << (passed + failed) << ") ===" << std::endl;

    return failed > 0 ? 1 : 0;
}
