/* *
 * kern/core/value_refactored.hpp - PRACTICAL Refactored Value System
 * 
 * This is a DROP-IN replacement that integrates with existing codebase.
 * It maintains API compatibility while switching to inline storage.
 */
#pragma once

#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cstring>
#include <functional>

namespace kern {

// Forward declarations
class Value;
using ValuePtr = std::shared_ptr<Value>;  // Legacy compat - but Value is now inline!

// Type enumeration for fast type checking
enum class ValueType : uint8_t {
    NIL = 0,
    BOOL,
    INT,
    FLOAT,
    STRING,
    ARRAY,
    MAP,
    FUNCTION,
    GENERATOR,
    CLASS,
    INSTANCE,
    NATIVE_FUNCTION,  // C++ function
    NATIVE_PTR,
    ERROR
};

// Native function type
using NativeFn = std::function<Value(const std::vector<Value>&)>;

// Error value
struct ErrorValue {
    uint32_t code;
    std::string message;
    std::vector<std::string> traceback;
};

// Function object
struct FunctionObject {
    size_t entryPoint;
    uint8_t arity;
    std::string name;
    std::vector<std::string> captures;
};

// Main Value class - 48 bytes inline (no heap for primitives)
class alignas(8) Value {
public:
    // Variant type - stored inline
    using Storage = std::variant<
        std::monostate,                    // NIL (0 bytes)
        bool,                              // BOOL (1 byte)
        int64_t,                           // INT (8 bytes)
        double,                            // FLOAT (8 bytes)
        std::string,                       // STRING (32 bytes SSO)
        std::vector<Value>,                // ARRAY
        std::unordered_map<std::string, Value>,  // MAP
        std::shared_ptr<FunctionObject>,   // FUNCTION
        NativeFn,                          // NATIVE_FUNCTION
        void*,                             // NATIVE_PTR
        ErrorValue                         // ERROR
    >;
    
private:
    Storage data;
    ValueType type;
    
public:
    // Constructors - all inline
    Value() : data(std::monostate{}), type(ValueType::NIL) {}
    explicit Value(bool b) : data(b), type(ValueType::BOOL) {}
    explicit Value(int64_t i) : data(i), type(ValueType::INT) {}
    explicit Value(int i) : data(static_cast<int64_t>(i)), type(ValueType::INT) {}
    explicit Value(double f) : data(f), type(ValueType::FLOAT) {}
    explicit Value(const std::string& s) : data(s), type(ValueType::STRING) {}
    explicit Value(const char* s) : data(std::string(s)), type(ValueType::STRING) {}
    explicit Value(NativeFn fn) : data(std::move(fn)), type(ValueType::NATIVE_FUNCTION) {}
    
    // Factory methods
    static Value nil() { return Value(); }
    static Value fromInt(int64_t i) { return Value(i); }
    static Value fromFloat(double f) { return Value(f); }
    static Value fromString(const std::string& s) { return Value(s); }
    static Value fromBool(bool b) { return Value(b); }
    static Value fromNative(NativeFn fn) { return Value(std::move(fn)); }
    
    static Value makeArray() {
        Value v;
        v.data = std::vector<Value>();
        v.type = ValueType::ARRAY;
        return v;
    }
    
    static Value makeMap() {
        Value v;
        v.data = std::unordered_map<std::string, Value>();
        v.type = ValueType::MAP;
        return v;
    }
    
    static Value makeError(uint32_t code, const std::string& msg) {
        Value v;
        v.data = ErrorValue{code, msg, {}};
        v.type = ValueType::ERROR;
        return v;
    }
    
    // Type checking
    ValueType getType() const { return type; }
    bool isNil() const { return type == ValueType::NIL; }
    bool isBool() const { return type == ValueType::BOOL; }
    bool isInt() const { return type == ValueType::INT; }
    bool isFloat() const { return type == ValueType::FLOAT; }
    bool isString() const { return type == ValueType::STRING; }
    bool isArray() const { return type == ValueType::ARRAY; }
    bool isMap() const { return type == ValueType::MAP; }
    bool isFunction() const { return type == ValueType::FUNCTION || type == ValueType::NATIVE_FUNCTION; }
    bool isNativeFunction() const { return type == ValueType::NATIVE_FUNCTION; }
    bool isError() const { return type == ValueType::ERROR; }
    
    bool isTruthy() const {
        switch (type) {
            case ValueType::NIL: return false;
            case ValueType::BOOL: return std::get<bool>(data);
            case ValueType::INT: return std::get<int64_t>(data) != 0;
            case ValueType::FLOAT: return std::get<double>(data) != 0.0;
            case ValueType::STRING: return !std::get<std::string>(data).empty();
            case ValueType::ARRAY: return !std::get<std::vector<Value>>(data).empty();
            case ValueType::MAP: return !std::get<std::unordered_map<std::string, Value>>(data).empty();
            default: return true;
        }
    }
    
    // Extractors with bounds checking
    bool asBool() const {
        switch (type) {
            case ValueType::BOOL: return std::get<bool>(data);
            case ValueType::INT: return std::get<int64_t>(data) != 0;
            default: return isTruthy();
        }
    }
    
    int64_t asInt() const {
        switch (type) {
            case ValueType::INT: return std::get<int64_t>(data);
            case ValueType::FLOAT: return static_cast<int64_t>(std::get<double>(data));
            case ValueType::BOOL: return std::get<bool>(data) ? 1 : 0;
            default: return 0;
        }
    }
    
    double asFloat() const {
        switch (type) {
            case ValueType::FLOAT: return std::get<double>(data);
            case ValueType::INT: return static_cast<double>(std::get<int64_t>(data));
            default: return 0.0;
        }
    }
    
    std::string asString() const {
        if (type == ValueType::STRING) {
            return std::get<std::string>(data);
        }
        return toString();
    }
    
    NativeFn asNative() const {
        if (type == ValueType::NATIVE_FUNCTION) {
            return std::get<NativeFn>(data);
        }
        return nullptr;
    }
    
    // Array operations
    void arrayPush(const Value& val) {
        if (type == ValueType::ARRAY) {
            std::get<std::vector<Value>>(data).push_back(val);
        }
    }
    
    Value arrayGet(size_t idx) const {
        if (type == ValueType::ARRAY) {
            const auto& arr = std::get<std::vector<Value>>(data);
            if (idx < arr.size()) return arr[idx];
        }
        return Value::nil();
    }
    
    size_t arraySize() const {
        if (type == ValueType::ARRAY) {
            return std::get<std::vector<Value>>(data).size();
        }
        return 0;
    }
    
    // Map operations
    void mapSet(const std::string& key, const Value& val) {
        if (type == ValueType::MAP) {
            std::get<std::unordered_map<std::string, Value>>(data)[key] = val;
        }
    }
    
    Value mapGet(const std::string& key) const {
        if (type == ValueType::MAP) {
            const auto& m = std::get<std::unordered_map<std::string, Value>>(data);
            auto it = m.find(key);
            if (it != m.end()) return it->second;
        }
        return Value::nil();
    }
    
    bool mapContains(const std::string& key) const {
        if (type == ValueType::MAP) {
            const auto& m = std::get<std::unordered_map<std::string, Value>>(data);
            return m.find(key) != m.end();
        }
        return false;
    }
    
    size_t mapSize() const {
        if (type == ValueType::MAP) {
            return std::get<std::unordered_map<std::string, Value>>(data).size();
        }
        return 0;
    }
    
    // Call function
    Value call(const std::vector<Value>& args) const {
        if (type == ValueType::NATIVE_FUNCTION) {
            return std::get<NativeFn>(data)(args);
        }
        return Value::makeError(1, "Not a function");
    }
    
    // Comparison
    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        
        switch (type) {
            case ValueType::NIL: return true;
            case ValueType::BOOL: return std::get<bool>(data) == std::get<bool>(other.data);
            case ValueType::INT: return std::get<int64_t>(data) == std::get<int64_t>(other.data);
            case ValueType::FLOAT: return std::get<double>(data) == std::get<double>(other.data);
            case ValueType::STRING: return std::get<std::string>(data) == std::get<std::string>(other.data);
            default: return false;
        }
    }
    
    bool operator<(const Value& other) const {
        if (type != other.type) return type < other.type;
        
        switch (type) {
            case ValueType::INT: return std::get<int64_t>(data) < std::get<int64_t>(other.data);
            case ValueType::FLOAT: return std::get<double>(data) < std::get<double>(other.data);
            case ValueType::STRING: return std::get<std::string>(data) < std::get<std::string>(other.data);
            default: return false;
        }
    }
    
    // String representation
    std::string toString() const {
        switch (type) {
            case ValueType::NIL: return "nil";
            case ValueType::BOOL: return std::get<bool>(data) ? "true" : "false";
            case ValueType::INT: return std::to_string(std::get<int64_t>(data));
            case ValueType::FLOAT: return std::to_string(std::get<double>(data));
            case ValueType::STRING: return std::get<std::string>(data);
            case ValueType::ARRAY: return "[array " + std::to_string(arraySize()) + "]";
            case ValueType::MAP: return "{map " + std::to_string(mapSize()) + "}";
            case ValueType::ERROR: return "Error: " + std::get<ErrorValue>(data).message;
            case ValueType::NATIVE_FUNCTION: return "[native function]";
            default: return "[object]";
        }
    }
    
    // Move semantics
    Value(Value&& other) noexcept = default;
    Value& operator=(Value&& other) noexcept = default;
    
    // Copy
    Value(const Value& other) = default;
    Value& operator=(const Value& other) = default;
    
    // Legacy compatibility - convert to shared_ptr if needed
    ValuePtr toPtr() const {
        return std::make_shared<Value>(*this);
    }
};

// Result type for error handling
template<typename T>
class Result {
    std::variant<T, ErrorValue> data;
    bool ok_;
    
public:
    explicit Result(T&& value) : data(std::move(value)), ok_(true) {}
    explicit Result(const ErrorValue& error) : data(error), ok_(false) {}
    
    bool ok() const { return ok_; }
    bool isError() const { return !ok_; }
    
    T& value() { return std::get<T>(data); }
    const T& value() const { return std::get<T>(data); }
    ErrorValue& error() { return std::get<ErrorValue>(data); }
    const ErrorValue& error() const { return std::get<ErrorValue>(data); }
    
    T unwrap() {
        if (!ok_) throw std::runtime_error("unwrap on error");
        return std::move(std::get<T>(data));
    }
};

} // namespace kern
