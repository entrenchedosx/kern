#ifndef KERN_COLLECTION_BUILTINS_HPP
#define KERN_COLLECTION_BUILTINS_HPP

#include "bytecode/value.hpp"
#include "vm.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <stdexcept>

namespace kern {

inline void registerCollectionBuiltins(VM& vm, size_t startIndex = 0) {
    size_t i = startIndex;

    auto setGlobalFn = [&vm](const std::string& name, size_t idx) {
        auto fn = std::make_shared<FunctionObject>();
        fn->isBuiltin = true;
        fn->builtinIndex = idx;
        vm.setGlobal(name, std::make_shared<Value>(Value::fromFunction(fn)));
    };

    // --- len(collection) (index 0) ---
    // Returns the integer size of an Array or Map.
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) -> Value {
        if (args.empty() || !args[0]) {
            throw VMError("len() requires a collection argument");
        }
        if (args[0]->type == Value::Type::ARRAY) {
            const auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
            return Value::fromInt(static_cast<int64_t>(arr.size()));
        }
        if (args[0]->type == Value::Type::MAP) {
            const auto& map = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
            return Value::fromInt(static_cast<int64_t>(map.size()));
        }
        throw VMError("len() expected Array or Map, got " + args[0]->typeName());
    });
    setGlobalFn("len", i);
    i++;

    // --- push(array, item) (index 1) ---
    // Appends an item to an Array in-place. Returns nil.
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) -> Value {
        if (args.size() < 2 || !args[0]) {
            throw VMError("push() requires an array and an item");
        }
        if (args[0]->type != Value::Type::ARRAY) {
            throw VMError("push() expected Array, got " + args[0]->typeName());
        }
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        auto item = args[1] ? args[1] : std::make_shared<Value>(Value::nil());
        arr.push_back(std::move(item));
        return Value::nil();
    });
    setGlobalFn("push", i);
    i++;

    // --- pop(array) (index 2) ---
    // Removes and returns the last item from an Array in-place.
    // Throws if the array is empty.
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) -> Value {
        if (args.empty() || !args[0]) {
            throw VMError("pop() requires an array argument");
        }
        if (args[0]->type != Value::Type::ARRAY) {
            throw VMError("pop() expected Array, got " + args[0]->typeName());
        }
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) {
            throw VMError("pop() called on empty array");
        }
        ValuePtr last = std::move(arr.back());
        arr.pop_back();
        return last ? *last : Value::nil();
    });
    setGlobalFn("pop", i);
    i++;

    // --- remove(array, index) (index 3) ---
    // Removes the item at the given index from an Array in-place.
    // Returns the removed item. Throws if index is out of bounds.
    vm.registerBuiltin(i, [](VM*, std::vector<ValuePtr> args) -> Value {
        if (args.size() < 2 || !args[0]) {
            throw VMError("remove() requires an array and an index");
        }
        if (args[0]->type != Value::Type::ARRAY) {
            throw VMError("remove() expected Array, got " + args[0]->typeName());
        }
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t idx = 0;
        if (args[1] && args[1]->type == Value::Type::INT) {
            idx = std::get<int64_t>(args[1]->data);
        } else {
            throw VMError("remove() expected Int index, got " + (args[1] ? args[1]->typeName() : std::string("nil")));
        }
        if (idx < 0 || static_cast<size_t>(idx) >= arr.size()) {
            throw VMError("remove() index " + std::to_string(idx) + " out of bounds for array of size " + std::to_string(arr.size()));
        }
        ValuePtr removed = std::move(arr[static_cast<size_t>(idx)]);
        arr.erase(arr.begin() + static_cast<ptrdiff_t>(idx));
        return removed ? *removed : Value::nil();
    });
    setGlobalFn("remove", i);
    // i = 4, done
}

} // namespace kern

#endif // KERN_COLLECTION_BUILTINS_HPP
