/* *
 * kern/runtime/bindings/native_bindings.h - Native Binding Layer
 * 
 * Safe type bridging between C++ and the Kern VM.
 * Allows C++ functions to be called from Kern scripts.
 */

#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <type_traits>
#include <tuple>

#include "../../core/value.hpp"
#include "../vm/vm.hpp"

// Function traits template for extracting function information
template<typename T>
struct function_traits;

template<typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<typename R, typename... Args>
struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {};

template<typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> : function_traits<R(Args...)> {};

namespace kern::runtime {

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE BRIDGE - VM ↔ C++ Type Conversion
// ═══════════════════════════════════════════════════════════════════════════════

/// Convert VM Value to C++ type
template<typename T>
struct VMToCpp {
    static T convert(const Value& value);
};

/// Convert C++ type to VM Value
template<typename T>
struct CppToVM {
    static Value convert(const T& value);
};

// Specializations for primitive types
#define KERN_BIND_PRIMITIVE(cppType, valueGetter, valueConstructor) \
    template<> struct VMToCpp<cppType> { \
        static cppType convert(const Value& v) { return v.valueGetter(); } \
    }; \
    template<> struct CppToVM<cppType> { \
        static Value convert(const cppType& v) { return Value::valueConstructor(v); } \
    };

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTION BINDING HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

namespace detail {
    
    // Helper to count template arguments
    template<typename... Args>
    constexpr size_t countArgs = sizeof...(Args);
    
    // Helper to extract argument from VM stack
    template<typename T>
    T extractArg(VM& vm, size_t index) {
        // TODO: Implement proper argument extraction from VM stack
        // For now, return default value
        return T{};
    }
    
    // Helper to push return value to VM
    template<typename T>
    void pushReturn(VM& vm, const T& value) {
        vm.push(std::make_shared<Value>(CppToVM<T>::convert(value)));
    }
    
    // Function wrapper for different arities
    template<typename Func, typename... Args, size_t... Indices>
    auto wrapFunctionImpl(Func func, std::index_sequence<Indices...>) {
        return [func](VM& vm) {
            // Extract all arguments
            std::tuple<Args...> args = std::make_tuple(extractArg<Args>(vm, Indices)...);
            
            // Call function
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                // Void return
                std::apply(func, args);
            } else {
                // Non-void return
                auto result = std::apply(func, args);
                pushReturn(vm, result);
            }
        };
    }
    
} // namespace detail

// ═══════════════════════════════════════════════════════════════════════════════
// NATIVE BINDING LAYER
// ═══════════════════════════════════════════════════════════════════════════════
//
// Central registry for native C++ functions callable from VM.

class NativeBindingLayer {
public:
    using NativeFunction = std::function<void(VM&)>;
    
    NativeBindingLayer();
    ~NativeBindingLayer();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // FUNCTION REGISTRATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Register a native function
    /// @param name Global name in Kern namespace (e.g., "math.sin", "io.print")
    /// @param func Function to register
    void registerFunction(const std::string& name, NativeFunction func);
    
    /// Register function with automatic type bridging
    /// @param name Global name in Kern namespace
    /// @param func C++ function with primitive types
    template<typename Func>
    void registerFunction(const std::string& name, Func func) {
        using Traits = function_traits<Func>;
        using Args = typename Traits::args;
        using Return = typename Traits::return_type;
        
        auto wrapped = [func](VM& vm) {
            // Extract arguments
            // Call function
            // Push result
        };
        
        registerFunction(name, wrapped);
    }
    
    /// Unregister a function
    void unregisterFunction(const std::string& name);
    
    /// Check if function is registered
    bool isRegistered(const std::string& name) const;
    
    /// Get registered function
    /// @return nullptr if not found
    NativeFunction getFunction(const std::string& name) const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // TYPE REGISTRATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Register a C++ struct/class for use in Kern
    /// @param name Type name in Kern
    template<typename T>
    void registerType(const std::string& name);
    
    /// Register a type constructor
    template<typename T, typename... Args>
    void registerConstructor(const std::string& name);
    
    /// Register a type method
    template<typename T, typename Func>
    void registerMethod(const std::string& typeName, const std::string& methodName, Func func);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // INVOCATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Call registered function from C++ side
    /// Used internally by VM when Kern script calls native function
    void invoke(const std::string& name, VM& vm);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ENUMERATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /// Get list of registered function names
    std::vector<std::string> getRegisteredNames() const;
    
    /// Get count of registered functions
    size_t getCount() const { return functions_.size(); }

private:
    std::unordered_map<std::string, NativeFunction> functions_;
};

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTION TRAITS HELPER
// ═══════════════════════════════════════════════════════════════════════════════

template<typename Func>
struct function_traits;

template<typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

template<typename R, typename... Args>
struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {};

template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...)> : function_traits<R(Args...)> {};

template<typename R, typename C, typename... Args>
struct function_traits<R(C::*)(Args...) const> : function_traits<R(Args...)> {};

template<typename Func>
struct function_traits : function_traits<decltype(&Func::operator())> {};

} // namespace kern::runtime
