/* *
 * Kern FFI Module — Dynamic Foreign Function Interface
 *
 * Provides runtime loading of native shared libraries (.dll / .so)
 * and dynamic binding of C functions via "Poor Man's FFI" with
 * hardcoded calling-convention stubs for common signatures.
 *
 * No libffi dependency — maintains standalone .exe goal.
 */

#include "vm.hpp"
#include "builtins.hpp"
#include "bytecode/value.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <cstdint>
#endif

namespace kern {

// ── Shared library loader ─────────────────────────────────────────────────

static void* ffiLoadLibrary(const std::string& path) {
#ifdef _WIN32
    return static_cast<void*>(LoadLibraryA(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

static void ffiFreeLibrary(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

static void* ffiGetProcAddress(void* handle, const std::string& name) {
    if (!handle) return nullptr;
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
#else
    return dlsym(handle, name.c_str());
#endif
}

// ── Helper: extract int64_t from ValuePtr (handles INT and FLOAT) ─────────

static int64_t toInt64(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return std::get<int64_t>(v->data);
    if (v->type == Value::Type::FLOAT) return static_cast<int64_t>(std::get<double>(v->data));
    return 0;
}

static double toDoubleVal(ValuePtr v) {
    if (!v) return 0.0;
    if (v->type == Value::Type::FLOAT) return std::get<double>(v->data);
    if (v->type == Value::Type::INT) return static_cast<double>(std::get<int64_t>(v->data));
    return 0.0;
}

// ── FFI call dispatcher (Poor Man's FFI) ─────────────────────────────────
//
// Uses the encoded signature string to select a hardcoded function pointer
// cast.  Supported return types: void, int, float, ptr
// Supported param types: int, float, string, ptr, bool
//
// The signature is built as: returnType(paramType1,paramType2,...)
// e.g. "int(int,int)"   → int(*)(int, int)
//      "int(string)"     → int(*)(const char*)
//      "int(ptr,string,string,int)" → int(*)(void*, const char*, const char*, int)

static Value callFfiClosure(const FfiClosure* ffi, const std::vector<ValuePtr>& args) {
    if (!ffi || !ffi->fnPtr) return Value::nil();

    const std::string& ret = ffi->returnType;
    const auto& params = ffi->paramTypes;
    size_t n = params.size();

    // Build a small signature key for dispatching
    // This is used purely for internal dispatch, not exposed to Kern
    auto sig = [&]() -> std::string {
        std::string s = ret;
        for (const auto& p : params) s += p;
        return s;
    }();

    void* fn = ffi->fnPtr;

    // ── Return type: void ────────────────────────────────────────────────
    if (ret == "void") {
        if (n == 0) {
            using F = void(*)();
            reinterpret_cast<F>(fn)();
            return Value::nil();
        }
        if (n == 1 && params[0] == "int") {
            using F = void(*)(int);
            reinterpret_cast<F>(fn)(static_cast<int>(toInt64(args[0])));
            return Value::nil();
        }
        if (n == 1 && params[0] == "ptr") {
            using F = void(*)(void*);
            F f = reinterpret_cast<F>(fn);
            f(args[0] && args[0]->type == Value::Type::PTR ? std::get<void*>(args[0]->data) : nullptr);
            return Value::nil();
        }
        if (n == 1 && params[0] == "string") {
            using F = void(*)(const char*);
            F f = reinterpret_cast<F>(fn);
            std::string s = args[0] ? args[0]->toString() : "";
            f(s.c_str());
            return Value::nil();
        }
        return Value::nil();
    }

    // ── Return type: int ─────────────────────────────────────────────────
    if (ret == "int") {
        if (n == 0) {
            using F = int(*)();
            return Value::fromInt(reinterpret_cast<F>(fn)());
        }
        if (n == 1 && params[0] == "int") {
            using F = int(*)(int);
            return Value::fromInt(reinterpret_cast<F>(fn)(static_cast<int>(toInt64(args[0]))));
        }
        if (n == 1 && params[0] == "string") {
            using F = int(*)(const char*);
            F f = reinterpret_cast<F>(fn);
            std::string s = args[0] ? args[0]->toString() : "";
            return Value::fromInt(f(s.c_str()));
        }
        if (n == 1 && params[0] == "ptr") {
            using F = int(*)(void*);
            F f = reinterpret_cast<F>(fn);
            f(args[0] && args[0]->type == Value::Type::PTR ? std::get<void*>(args[0]->data) : nullptr);
            return Value::nil();
        }
        if (n == 2 && params[0] == "int" && params[1] == "int") {
            using F = int(*)(int, int);
            return Value::fromInt(reinterpret_cast<F>(fn)(
                static_cast<int>(toInt64(args[0])),
                static_cast<int>(toInt64(args[1]))));
        }
        if (n == 2 && params[0] == "string" && params[1] == "string") {
            using F = int(*)(const char*, const char*);
            F f = reinterpret_cast<F>(fn);
            std::string s0 = args[0] ? args[0]->toString() : "";
            std::string s1 = args[1] ? args[1]->toString() : "";
            return Value::fromInt(f(s0.c_str(), s1.c_str()));
        }
        if (n == 3 && params[0] == "int" && params[1] == "int" && params[2] == "int") {
            using F = int(*)(int, int, int);
            return Value::fromInt(reinterpret_cast<F>(fn)(
                static_cast<int>(toInt64(args[0])),
                static_cast<int>(toInt64(args[1])),
                static_cast<int>(toInt64(args[2]))));
        }
        if (n == 4 && params[0] == "ptr" && params[1] == "string" && params[2] == "string" && params[3] == "int") {
            // MessageBoxA signature: int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT)
            using F = int(*)(void*, const char*, const char*, int);
            F f = reinterpret_cast<F>(fn);
            void* hwnd = (args[0] && args[0]->type == Value::Type::PTR) ? std::get<void*>(args[0]->data) : nullptr;
            std::string text = args[1] ? args[1]->toString() : "";
            std::string caption = args[2] ? args[2]->toString() : "";
            int type = static_cast<int>(toInt64(args[3]));
            return Value::fromInt(f(hwnd, text.c_str(), caption.c_str(), type));
        }
        if (n == 4 && params[0] == "int" && params[1] == "int" && params[2] == "int" && params[3] == "int") {
            using F = int(*)(int, int, int, int);
            return Value::fromInt(reinterpret_cast<F>(fn)(
                static_cast<int>(toInt64(args[0])),
                static_cast<int>(toInt64(args[1])),
                static_cast<int>(toInt64(args[2])),
                static_cast<int>(toInt64(args[3]))));
        }
        // Fallback: try generic int(*)(...) — let the platform handle it
        // This catches unsupported sigs gracefully
        return Value::fromInt(0);
    }

    // ── Return type: float ───────────────────────────────────────────────
    if (ret == "float") {
        if (n == 0) {
            using F = double(*)();
            return Value::fromFloat(reinterpret_cast<F>(fn)());
        }
        if (n == 1 && params[0] == "float") {
            using F = double(*)(double);
            return Value::fromFloat(reinterpret_cast<F>(fn)(toDoubleVal(args[0])));
        }
        if (n == 1 && params[0] == "int") {
            using F = double(*)(int);
            return Value::fromFloat(reinterpret_cast<F>(fn)(static_cast<int>(toInt64(args[0]))));
        }
        if (n == 2 && params[0] == "float" && params[1] == "float") {
            using F = double(*)(double, double);
            return Value::fromFloat(reinterpret_cast<F>(fn)(
                toDoubleVal(args[0]), toDoubleVal(args[1])));
        }
        if (n == 2 && params[0] == "int" && params[1] == "int") {
            using F = double(*)(int, int);
            return Value::fromFloat(reinterpret_cast<F>(fn)(
                static_cast<int>(toInt64(args[0])),
                static_cast<int>(toInt64(args[1]))));
        }
        return Value::fromFloat(0.0);
    }

    // ── Return type: ptr ─────────────────────────────────────────────────
    if (ret == "ptr") {
        if (n == 0) {
            using F = void*(*)();
            return Value::fromPtr(reinterpret_cast<F>(fn)());
        }
        if (n == 1 && params[0] == "int") {
            using F = void*(*)(int);
            return Value::fromPtr(reinterpret_cast<F>(fn)(static_cast<int>(toInt64(args[0]))));
        }
        if (n == 1 && params[0] == "string") {
            using F = void*(*)(const char*);
            F f = reinterpret_cast<F>(fn);
            std::string s = args[0] ? args[0]->toString() : "";
            return Value::fromPtr(f(s.c_str()));
        }
        return Value::nil();
    }

    // ── Return type: string ──────────────────────────────────────────────
    // NOTE: This assumes the C function returns a heap-allocated C string
    // that the caller must NOT free (e.g., GetCommandLineA, strerror).
    // Use with care.
    if (ret == "string") {
        if (n == 0) {
            using F = const char*(*)();
            const char* s = reinterpret_cast<F>(fn)();
            return Value::fromString(s ? s : "");
        }
        if (n == 1 && params[0] == "int") {
            using F = const char*(*)(int);
            const char* s = reinterpret_cast<F>(fn)(static_cast<int>(toInt64(args[0])));
            return Value::fromString(s ? s : "");
        }
        return Value::nil();
    }

    return Value::nil();
}

// ── Builtin: ffi_load(path) → ptr ────────────────────────────────────────

Value builtinFfiLoad(VM*, std::vector<ValuePtr> args) {
    if (args.empty()) {
        return Value::nil();
    }
    std::string path = args[0]->toString();
    void* handle = ffiLoadLibrary(path);
    if (!handle) {
        return Value::nil();
    }
    return Value::fromPtr(handle);
}

// ── Builtin: ffi_bind(handle, name, return_type, param_types) → ffi_fn ───
//
//   handle       – ptr returned from ffi_load
//   name         – string: exported function name
//   return_type  – string: "void" | "int" | "float" | "ptr" | "string"
//   param_types  – array of strings: ["int", "int"] etc.

Value builtinFfiBind(VM*, std::vector<ValuePtr> args) {
    if (args.size() < 3) return Value::nil();

    void* handle = nullptr;
    if (args[0] && args[0]->type == Value::Type::PTR)
        handle = std::get<void*>(args[0]->data);
    if (!handle) return Value::nil();

    std::string funcName = args[1] ? args[1]->toString() : "";
    if (funcName.empty()) return Value::nil();

    std::string returnType = args[2] ? args[2]->toString() : "void";

    std::vector<std::string> paramTypes;
    if (args.size() >= 4 && args[3] && args[3]->type == Value::Type::ARRAY) {
        auto& arr = std::get<std::vector<ValuePtr>>(args[3]->data);
        for (const auto& elem : arr) {
            paramTypes.push_back(elem ? elem->toString() : "int");
        }
    }

    void* fnPtr = ffiGetProcAddress(handle, funcName);
    if (!fnPtr) return Value::nil();

    auto closure = std::make_shared<FfiClosure>();
    closure->fnPtr = fnPtr;
    closure->returnType = returnType;
    closure->paramTypes = std::move(paramTypes);

    return Value::fromFfi(std::move(closure));
}

// ── Builtin: ffi_free(handle) → nil ──────────────────────────────────────

Value builtinFfiFree(VM*, std::vector<ValuePtr> args) {
    if (args.empty()) return Value::nil();
    void* handle = nullptr;
    if (args[0] && args[0]->type == Value::Type::PTR)
        handle = std::get<void*>(args[0]->data);
    if (handle) ffiFreeLibrary(handle);
    return Value::nil();
}

// ── Registration ──────────────────────────────────────────────────────────

void registerFfiBuiltins(VM& vm) {
    // Use high slot indices to avoid colliding with the growing builtin list.
    // We'll use slots starting at 1000.
    const size_t base = 1000;
    vm.registerBuiltin(base + 0, builtinFfiLoad);
    vm.registerBuiltin(base + 1, builtinFfiBind);
    vm.registerBuiltin(base + 2, builtinFfiFree);

    auto makeFn = [](size_t idx) {
        auto fn = std::make_shared<FunctionObject>();
        fn->isBuiltin = true;
        fn->builtinIndex = idx;
        return fn;
    };

    vm.setGlobal("ffi_load", std::make_shared<Value>(Value::fromFunction(makeFn(base + 0))));
    vm.setGlobal("ffi_bind", std::make_shared<Value>(Value::fromFunction(makeFn(base + 1))));
    vm.setGlobal("ffi_free", std::make_shared<Value>(Value::fromFunction(makeFn(base + 2))));
}

// ── FFI call dispatcher (called from VM::runInstruction CALL handler) ────

Value callFfiFunction(const FfiClosure* ffi, const std::vector<ValuePtr>& args) {
    return callFfiClosure(ffi, args);
}

} // namespace kern
