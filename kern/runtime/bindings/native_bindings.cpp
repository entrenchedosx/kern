/* *
 * kern/runtime/bindings/native_bindings.cpp - Native Binding Layer Implementation
 */

#include "native_bindings.h"
#include <iostream>

// Core includes for Value and VM integration
#include "../../core/value.hpp"
#include "../vm/vm.hpp"
#include "../vm/builtins.hpp"
#include "../../core/errors/errors.hpp"

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE BRIDGE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════════════

// Register primitive types - using actual Value API methods
KERN_BIND_PRIMITIVE(int32_t, asInt, fromInt)
KERN_BIND_PRIMITIVE(int64_t, asInt, fromInt)
KERN_BIND_PRIMITIVE(float, asFloat, fromFloat)
KERN_BIND_PRIMITIVE(double, asFloat, fromFloat)
KERN_BIND_PRIMITIVE(bool, asBool, fromBool)
KERN_BIND_PRIMITIVE(std::string, asString, fromString)

// ═══════════════════════════════════════════════════════════════════════════════
// NATIVE BINDING LAYER IMPLEMENTATION
// ═══════════════════════════════════════════════════════════════════════════════

NativeBindingLayer::NativeBindingLayer() {
    // Nothing needed
}

NativeBindingLayer::~NativeBindingLayer() {
    // Clear all registered functions
    functions_.clear();
}

void NativeBindingLayer::registerFunction(const std::string& name, NativeFunction func) {
    functions_[name] = func;
    
    // Register with VM
    // TODO: vm_.registerNativeFunction(name, func);
}

void NativeBindingLayer::unregisterFunction(const std::string& name) {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        functions_.erase(it);
        
        // Unregister from VM
        // TODO: vm_.unregisterNativeFunction(name);
    }
}

bool NativeBindingLayer::isRegistered(const std::string& name) const {
    return functions_.find(name) != functions_.end();
}

NativeBindingLayer::NativeFunction NativeBindingLayer::getFunction(const std::string& name) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        return it->second;
    }
    return nullptr;
}

void NativeBindingLayer::invoke(const std::string& name, VM& vm) {
    auto func = getFunction(name);
    if (func) {
        func(vm);
    } else {
        std::cerr << "Native function not found: " << name << "\n";
        // TODO: Throw exception or set VM error
    }
}

std::vector<std::string> NativeBindingLayer::getRegisteredNames() const {
    std::vector<std::string> names;
    names.reserve(functions_.size());
    
    for (const auto& [name, func] : functions_) {
        names.push_back(name);
    }
    
    return names;
}

} // namespace kern::runtime
