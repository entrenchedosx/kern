/* *
 * kern/tools/async_fs_test.cpp — Integration test for fs_read_async
 *
 * v4.0 Phase 1: Non-blocking Async File Reading
 *
 * Compile + run:
 *   C:\msys64\usr\bin\bash.exe -c "export PATH=/mingw64/bin:$PATH:/c/Program\ Files/CMake/bin && cd /e/kerncode/build && cmake --build . --target kern_async_fs_test 2>&1"
 *   ./build/kern_async_fs_test
 */

#include "../runtime/vm/vm.hpp"
#include "../core/compiler/lexer.hpp"
#include "../core/compiler/parser.hpp"
#include "../core/compiler/codegen.hpp"
#include "../core/bytecode/value.hpp"
#include "../runtime/vm/builtins.hpp"
#include <iostream>
#include <fstream>
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

// ─── Helper: write a text file ──────────────────────────────────────────
static bool writeTestFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return f.good();
}

// ─── Helper: delete a file ──────────────────────────────────────────────
static void deleteTestFile(const std::string& path) {
    std::filesystem::remove(path);
}

// ─── Helper: check if a ValuePtr is a string with given content ─────────
static bool isStringValue(const ValuePtr& v, const std::string& expected) {
    if (!v) return false;
    if (v->type != Value::Type::STRING) return false;
    return v->asString() == expected;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 1: Basic async file read from a coroutine
// ═══════════════════════════════════════════════════════════════════════
static bool testAsyncFileRead() {
    std::cout << "  [test] Async file read from coroutine... ";

    const std::string testPath = "async_fs_test_data.txt";
    const std::string testContent = "Hello from async fs_read_async!\nLine 2.";

    if (!writeTestFile(testPath, testContent)) {
        std::cout << "FAIL (cannot write test file)" << std::endl;
        return false;
    }

    std::string source = R"(
        global result = ""

        fn reader() {
            result = kern_fs_read_async(")" + testPath + R"(")
        }

        kern_start_coroutine(reader, [])
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        deleteTestFile(testPath);
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);

    // Load via hotReload (initial load path)
    vm.hotReload(code, strings, values);

    // Poll resumeAll() with a mock clock until done
    uint64_t tick = 0;
    while (vm.hasActiveCoroutines() && tick < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    bool passed = isStringValue(vm.getGlobal("result"), testContent);
    if (!passed) {
        auto rg = vm.getGlobal("result");
        if (rg && rg->type == Value::Type::STRING) {
            std::cout << "FAIL (expected '" << testContent << "', got '" << rg->asString() << "')" << std::endl;
        } else if (rg) {
            std::cout << "FAIL (result type=" << rg->typeName() << ", val=" << rg->toString() << ")" << std::endl;
        } else {
            std::cout << "FAIL (result global missing)" << std::endl;
        }
    }

    deleteTestFile(testPath);
    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 2: Empty file read
// ═══════════════════════════════════════════════════════════════════════
static bool testEmptyFileRead() {
    std::cout << "  [test] Empty file read... ";

    const std::string testPath = "async_fs_empty.txt";

    if (!writeTestFile(testPath, "")) {
        std::cout << "FAIL (cannot write test file)" << std::endl;
        return false;
    }

    std::string source = R"(
        global result = "<unset>"

        fn reader() {
            result = kern_fs_read_async(")" + testPath + R"(")
        }

        kern_start_coroutine(reader, [])
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        deleteTestFile(testPath);
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);

    uint64_t tick = 0;
    while (vm.hasActiveCoroutines() && tick < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    bool passed = isStringValue(vm.getGlobal("result"), "");
    if (!passed) {
        auto rg = vm.getGlobal("result");
        if (rg && rg->type == Value::Type::STRING) {
            std::cout << "FAIL (expected empty, got '" << rg->asString() << "')" << std::endl;
        } else {
            std::cout << "FAIL (result not a string)" << std::endl;
        }
    }

    deleteTestFile(testPath);
    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 3: Non-existent file returns empty string
// ═══════════════════════════════════════════════════════════════════════
static bool testNonexistentFile() {
    std::cout << "  [test] Non-existent file returns empty... ";

    const std::string testPath = "async_fs_nonexistent_NONEXISTENT.txt";

    // Ensure it doesn't exist
    deleteTestFile(testPath);

    std::string source = R"(
        global result = "<unset>"

        fn reader() {
            result = kern_fs_read_async(")" + testPath + R"(")
        }

        kern_start_coroutine(reader, [])
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
    while (vm.hasActiveCoroutines() && tick < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    bool passed = isStringValue(vm.getGlobal("result"), "");
    if (!passed) {
        auto rg = vm.getGlobal("result");
        if (rg && rg->type == Value::Type::STRING) {
            std::cout << "FAIL (expected empty, got '" << rg->asString() << "')" << std::endl;
        } else {
            std::cout << "FAIL (result not a string)" << std::endl;
        }
    }

    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// Test 4: Multiple concurrent async reads
// ═══════════════════════════════════════════════════════════════════════
static bool testConcurrentReads() {
    std::cout << "  [test] Concurrent async reads... ";

    const std::string pathA = "async_fs_concurrent_a.txt";
    const std::string pathB = "async_fs_concurrent_b.txt";
    const std::string contentA = "AAAA";
    const std::string contentB = "BBBB";

    if (!writeTestFile(pathA, contentA) || !writeTestFile(pathB, contentB)) {
        std::cout << "FAIL (cannot write test files)" << std::endl;
        return false;
    }

    std::string source = R"(
        global result_a = ""
        global result_b = ""

        fn reader_a() {
            result_a = kern_fs_read_async(")" + pathA + R"(")
        }

        fn reader_b() {
            result_b = kern_fs_read_async(")" + pathB + R"(")
        }

        kern_start_coroutine(reader_a, [])
        kern_start_coroutine(reader_b, [])
    )";

    Bytecode code;
    std::vector<std::string> strings;
    std::vector<Value> values;
    std::string error;

    if (!compileSource(source, code, strings, values, error)) {
        std::cout << "FAIL (compile: " << error << ")" << std::endl;
        deleteTestFile(pathA);
        deleteTestFile(pathB);
        return false;
    }

    VMConfig config;
    VM vm(config);
    registerAllBuiltins(vm);
    vm.hotReload(code, strings, values);

    uint64_t tick = 0;
    while (vm.hasActiveCoroutines() && tick < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        vm.resumeAll(tick);
        tick++;
    }

    bool passed = isStringValue(vm.getGlobal("result_a"), contentA)
               && isStringValue(vm.getGlobal("result_b"), contentB);
    if (!passed) {
        auto ga = vm.getGlobal("result_a");
        auto gb = vm.getGlobal("result_b");
        std::cout << "FAIL (a=";
        if (ga && ga->type == Value::Type::STRING) std::cout << "'" << ga->asString() << "'";
        else std::cout << (ga ? ga->toString() : "null");
        std::cout << " b=";
        if (gb && gb->type == Value::Type::STRING) std::cout << "'" << gb->asString() << "'";
        else std::cout << (gb ? gb->toString() : "null");
        std::cout << ")" << std::endl;
    }

    deleteTestFile(pathA);
    deleteTestFile(pathB);
    if (passed) std::cout << "OK" << std::endl;
    return passed;
}

// ═══════════════════════════════════════════════════════════════════════
// main
// ═══════════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Async FS Read Integration Test ===" << std::endl;

    int passed = 0;
    int failed = 0;

    auto run = [&](const char* name, auto fn) {
        if (fn()) passed++;
        else { failed++; std::cout << "  FAILED: " << name << std::endl; }
    };

    run("testAsyncFileRead",       testAsyncFileRead);
    run("testEmptyFileRead",       testEmptyFileRead);
    run("testNonexistentFile",     testNonexistentFile);
    run("testConcurrentReads",     testConcurrentReads);

    std::cout << "=== " << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED")
              << " (" << passed << "/" << (passed + failed) << ") ===" << std::endl;

    return failed > 0 ? 1 : 0;
}
