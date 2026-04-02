#include "process_module.hpp"

#include "system/runtime_services.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

namespace kern {

namespace {

#ifdef _WIN32
static int64_t toInt(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return std::get<int64_t>(v->data);
    if (v->type == Value::Type::FLOAT) return static_cast<int64_t>(std::get<double>(v->data));
    return 0;
}
#endif

struct ProcessContext {
#ifdef _WIN32
    std::vector<HANDLE> handles;
#else
    std::vector<void*> handles;
#endif
};

#ifdef _WIN32
static HANDLE getHandle(const ProcessContext& ctx, int64_t handleId) {
    if (handleId < 0 || static_cast<size_t>(handleId) >= ctx.handles.size()) return nullptr;
    HANDLE h = ctx.handles[static_cast<size_t>(handleId)];
    if (h == nullptr || h == INVALID_HANDLE_VALUE) return nullptr;
    return h;
}

static std::string narrowExeName(const wchar_t* wname) {
    std::string out;
    for (int i = 0; wname[i] != 0 && i < MAX_PATH; ++i) {
        wchar_t wc = wname[i];
        out.push_back(static_cast<char>(wc <= 127 ? wc : '?'));
    }
    return out;
}

// Low byte of MemoryBasicInformation::Protect (ignore modifiers in high bits for our checks).
static bool memoryBasicIsReadable(DWORD protect) {
    if (protect & PAGE_GUARD) return false;
    switch (protect & 0xFF) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}
#endif

} // namespace

ValuePtr createProcessModule(VM& vm, const std::shared_ptr<RuntimeServices>& /* services*/) {
    std::unordered_map<std::string, ValuePtr> mod;
    static size_t s_builtinBase = 680;
    const auto context = std::make_shared<ProcessContext>();

    auto add = [&](const std::string& name, VM::BuiltinFn fn) {
        const size_t idx = s_builtinBase++;
        vm.registerBuiltin(idx, std::move(fn));
        auto f = std::make_shared<FunctionObject>();
        f->isBuiltin = true;
        f->builtinIndex = idx;
        mod[name] = std::make_shared<Value>(Value::fromFunction(f));
    };

    add("list", [](VM*, std::vector<ValuePtr>) {
        std::vector<ValuePtr> out;
#ifdef _WIN32
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return Value::fromArray(std::move(out));
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                auto row = std::unordered_map<std::string, ValuePtr>{};
                row["pid"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(pe.th32ProcessID)));
                row["name"] = std::make_shared<Value>(Value::fromString(narrowExeName(pe.szExeFile)));
                row["memory"] = std::make_shared<Value>(Value::fromInt(0));

                HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
                if (ph && ph != INVALID_HANDLE_VALUE) {
                    PROCESS_MEMORY_COUNTERS pmc{};
                    if (GetProcessMemoryInfo(ph, &pmc, sizeof(pmc))) {
                        row["memory"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(pmc.WorkingSetSize)));
                    }
                    CloseHandle(ph);
                }
                out.push_back(std::make_shared<Value>(Value::fromMap(std::move(row))));
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
#endif
        return Value::fromArray(std::move(out));
    });

    add("open", [context](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(-1);
#ifdef _WIN32
        DWORD pid = static_cast<DWORD>(toInt(args[0]));
        if (pid == 0) return Value::fromInt(-1);
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
        if (!h || h == INVALID_HANDLE_VALUE) return Value::fromInt(-1);
        const int64_t handleId = static_cast<int64_t>(context->handles.size());
        context->handles.push_back(h);
        return Value::fromInt(handleId);
#else
        return Value::fromInt(-1);
#endif
    });

    add("close", [context](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
#ifdef _WIN32
        int64_t handleId = toInt(args[0]);
        if (handleId < 0 || static_cast<size_t>(handleId) >= context->handles.size()) return Value::fromBool(false);
        HANDLE h = context->handles[static_cast<size_t>(handleId)];
        context->handles[static_cast<size_t>(handleId)] = nullptr;
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            return Value::fromBool(true);
        }
#endif
        return Value::fromBool(false);
    });

    add("read", [context](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
#ifdef _WIN32
        int64_t handleId = toInt(args[0]);
        uint64_t address = static_cast<uint64_t>(toInt(args[1]));
        int64_t requested = toInt(args[2]);
        if (requested <= 0 || requested > 1024 * 1024) return Value::nil();

        HANDLE h = getHandle(*context, handleId);
        if (!h) return Value::nil();

        std::vector<unsigned char> bytes(static_cast<size_t>(requested));
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(h, reinterpret_cast<LPCVOID>(address), bytes.data(), static_cast<SIZE_T>(bytes.size()), &bytesRead))
            return Value::nil();

        std::vector<ValuePtr> out;
        out.reserve(static_cast<size_t>(bytesRead));
        for (size_t i = 0; i < static_cast<size_t>(bytesRead); ++i) {
            out.push_back(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(bytes[i]))));
        }
        return Value::fromArray(std::move(out));
#else
        return Value::nil();
#endif
    });

    add("read_u32", [context](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
#ifdef _WIN32
        HANDLE h = getHandle(*context, toInt(args[0]));
        if (!h) return Value::nil();
        uint64_t addr = static_cast<uint64_t>(toInt(args[1]));
        uint32_t val = 0;
        SIZE_T read = 0;
        if (!ReadProcessMemory(h, reinterpret_cast<LPCVOID>(addr), &val, sizeof(val), &read) || read != sizeof(val))
            return Value::nil();
        return Value::fromInt(static_cast<int64_t>(val));
#else
        return Value::nil();
#endif
    });

    // First committed readable region (VirtualQueryEx). Use this instead of address 0 — the null page is not mapped.
    add("first_readable_region", [context](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
#ifdef _WIN32
        HANDLE h = getHandle(*context, toInt(args[0]));
        if (!h) return Value::nil();
        uint8_t* address = nullptr;
        for (;;) {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQueryEx(h, address, &mbi, sizeof(mbi)) != sizeof(mbi)) break;
            if (mbi.State == MEM_COMMIT && memoryBasicIsReadable(mbi.Protect)) {
                auto row = std::unordered_map<std::string, ValuePtr>{};
                row["base"] = std::make_shared<Value>(
                    Value::fromInt(static_cast<int64_t>(reinterpret_cast<uintptr_t>(mbi.BaseAddress))));
                row["size"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(mbi.RegionSize)));
                return Value::fromMap(std::move(row));
            }
            uint8_t* next = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
            if (next <= address) break;
            address = next;
        }
        return Value::nil();
#else
        return Value::nil();
#endif
    });

    add("metadata", [context](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        out["pid"] = std::make_shared<Value>(Value::fromInt(-1));
        out["name"] = std::make_shared<Value>(Value::fromString(""));
        out["memory"] = std::make_shared<Value>(Value::fromInt(0));
        if (args.empty()) return Value::fromMap(std::move(out));
#ifdef _WIN32
        int64_t handleId = toInt(args[0]);
        HANDLE h = getHandle(*context, handleId);
        if (!h) return Value::fromMap(std::move(out));

        DWORD pid = GetProcessId(h);
        out["pid"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(pid)));
        char nameBuf[MAX_PATH]{};
        if (GetModuleBaseNameA(h, nullptr, nameBuf, MAX_PATH) > 0) {
            out["name"] = std::make_shared<Value>(Value::fromString(std::string(nameBuf)));
        }
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) {
            out["memory"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(pmc.WorkingSetSize)));
        }
#endif
        return Value::fromMap(std::move(out));
    });

    // Script-visible ABI level (first_readable_region + VirtualQueryEx scan = 2).
    mod["_kern_process_api"] = std::make_shared<Value>(Value::fromInt(2));

    return std::make_shared<Value>(Value::fromMap(std::move(mod)));
}

} // namespace kern

