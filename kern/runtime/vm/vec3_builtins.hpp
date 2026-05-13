#ifndef KERN_VEC3_BUILTINS_HPP
#define KERN_VEC3_BUILTINS_HPP

#include "bytecode/value.hpp"
#include "vm.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <cstdint>

namespace kern {

struct Vec3StructFieldLayout {
    std::string name;
    size_t offset = 0;
    size_t size = 0;
    size_t align = 1;
};

struct Vec3StructLayoutMeta {
    size_t size = 0;
    size_t align = 1;
    std::vector<Vec3StructFieldLayout> fields;
};

// Global registry of struct layout metadata (name -> layout)
// Populated by the struct_define builtin.
inline std::unordered_map<std::string, Vec3StructLayoutMeta>& getVec3StructLayouts() {
    static std::unordered_map<std::string, Vec3StructLayoutMeta> layouts;
    return layouts;
}

inline void registerVec3Builtins(VM& vm) {
    size_t i = 0;

    auto setGlobalFn = [&vm](const std::string& name, size_t idx) {
        auto fn = std::make_shared<FunctionObject>();
        fn->isBuiltin = true;
        fn->builtinIndex = idx;
        vm.setGlobal(name, std::make_shared<Value>(Value::fromFunction(fn)));
    };

    auto toDouble = [](ValuePtr v) -> double {
        if (!v) return 0.0;
        if (v->type == Value::Type::FLOAT) return std::get<double>(v->data);
        if (v->type == Value::Type::INT) return static_cast<double>(std::get<int64_t>(v->data));
        return 0.0;
    };

    // --- vec3_new(x, y, z) (index 0) ---
    vm.registerBuiltin(i, [&toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        double x = toDouble(args[0]);
        double y = toDouble(args[1]);
        double z = toDouble(args[2]);
        return Value::fromVec3(x, y, z);
    });
    setGlobalFn("vec3_new", i);
    i++;

    // --- vec3_add(a, b) (index 1) ---
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        if (args[0]->type != Value::Type::VEC3 || args[1]->type != Value::Type::VEC3) return Value::nil();
        auto a = std::get<Vec3Ptr>(args[0]->data);
        auto b = std::get<Vec3Ptr>(args[1]->data);
        return Value::fromVec3(a->x + b->x, a->y + b->y, a->z + b->z);
    });
    setGlobalFn("vec3_add", i);
    i++;

    // --- vec3_sub(a, b) (index 2) ---
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        if (args[0]->type != Value::Type::VEC3 || args[1]->type != Value::Type::VEC3) return Value::nil();
        auto a = std::get<Vec3Ptr>(args[0]->data);
        auto b = std::get<Vec3Ptr>(args[1]->data);
        return Value::fromVec3(a->x - b->x, a->y - b->y, a->z - b->z);
    });
    setGlobalFn("vec3_sub", i);
    i++;

    // --- vec3_dot(a, b) (index 3) ---
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        if (args[0]->type != Value::Type::VEC3 || args[1]->type != Value::Type::VEC3) return Value::nil();
        auto a = std::get<Vec3Ptr>(args[0]->data);
        auto b = std::get<Vec3Ptr>(args[1]->data);
        return Value::fromFloat(a->x * b->x + a->y * b->y + a->z * b->z);
    });
    setGlobalFn("vec3_dot", i);
    i++;

    // --- vec3_normalize(v) (index 4) ---
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::nil();
        if (args[0]->type != Value::Type::VEC3) return Value::nil();
        auto v = std::get<Vec3Ptr>(args[0]->data);
        double len = std::sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
        if (len == 0) return Value::fromVec3(0, 0, 0);
        return Value::fromVec3(v->x / len, v->y / len, v->z / len);
    });
    setGlobalFn("vec3_normalize", i);
    i++;

    // --- struct_define(name, fieldsArray) (index 5) ---
    // Registers struct layout metadata so offsetof_struct/sizeof_struct work.
    // Called by the codegen when a struct declaration is encountered.
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        std::string name = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : "";
        if (name.empty()) return Value::nil();
        if (args[1]->type != Value::Type::ARRAY) return Value::nil();
        Vec3StructLayoutMeta meta;
        const auto& arr = std::get<ValueArray>(args[1]->data);
        size_t runningOffset = 0;
        size_t maxAlign = 1;
        for (const auto& el : arr) {
            if (!el || el->type != Value::Type::ARRAY) continue;
            const auto& pair = std::get<ValueArray>(el->data);
            std::string fname = (pair.size() >= 1 && pair[0] && pair[0]->type == Value::Type::STRING)
                ? std::get<std::string>(pair[0]->data) : "";
            int64_t rawSize = (pair.size() >= 2 && pair[1]) ? (pair[1]->type == Value::Type::INT ? std::get<int64_t>(pair[1]->data) : 0) : 0;
            size_t fsize = static_cast<size_t>(std::max(int64_t(0), rawSize));
            int64_t rawAlign = (pair.size() >= 3 && pair[2]) ? (pair[2]->type == Value::Type::INT ? std::get<int64_t>(pair[2]->data) : 0) : 0;
            size_t falign = static_cast<size_t>(std::max(int64_t(1), rawAlign));
            if (falign == 0) falign = 1;
            if (falign > maxAlign) maxAlign = falign;
            if (!fname.empty()) {
                size_t pad = runningOffset % falign;
                if (pad != 0) runningOffset += (falign - pad);
                Vec3StructFieldLayout fld;
                fld.name = fname;
                fld.size = fsize;
                fld.align = falign;
                fld.offset = runningOffset;
                runningOffset += fsize;
                meta.fields.push_back(std::move(fld));
            }
        }
        if (maxAlign == 0) maxAlign = 1;
        size_t endPad = runningOffset % maxAlign;
        if (endPad != 0) runningOffset += (maxAlign - endPad);
        meta.align = maxAlign;
        meta.size = runningOffset;
        getVec3StructLayouts()[name] = std::move(meta);
        return Value::fromInt(1);
    });
    setGlobalFn("struct_define", i);
    i++;

    // --- ok(value) (index 6) ---
    // Returns a success Result: { is_ok: true, value: <value> }
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> map;
        map["is_ok"] = std::make_shared<Value>(Value::fromBool(true));
        map["value"] = args.empty() ? std::make_shared<Value>(Value::nil()) : (args[0] ? args[0] : std::make_shared<Value>(Value::nil()));
        return Value::fromMap(std::move(map));
    });
    setGlobalFn("ok", i);
    i++;

    // --- err(message) (index 7) ---
    // Returns an error Result: { is_ok: false, error: <message> }
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> map;
        map["is_ok"] = std::make_shared<Value>(Value::fromBool(false));
        map["error"] = (args.size() >= 1 && args[0]) ? args[0] : std::make_shared<Value>(Value::nil());
        return Value::fromMap(std::move(map));
    });
    setGlobalFn("err", i);
    i++;

    // --- print(value) (index 8) ---
    // Outputs a value to stdout (for testing purposes)
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) {
        if (!args.empty() && args[0]) {
            std::cout << args[0]->toString() << std::endl;
        } else {
            std::cout << "nil" << std::endl;
        }
        return Value::nil();
    });
    setGlobalFn("print", i);
    // i = 9, done
}

} // namespace kern

#endif // KERN_VEC3_BUILTINS_HPP
