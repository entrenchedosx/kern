/* *
 * kern/core/value.hpp - Refactored Value System
 * 
 * Eliminates shared_ptr overhead by using a variant-based value type
 * with small-string optimization and move semantics.
 */
#pragma once

#include <variant>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cstring>

namespace kern {

// Forward declarations
class Value;
class FunctionObject;
class GeneratorObject;
class ClassObject;
class InstanceObject;

using ValuePtr = std::shared_ptr<Value>;  // Legacy compat - minimize usage

// Type enumeration for fast type checking
enum class ValueType : uint8_t {
    NIL = 0,
    BOOL,
    INT,
    FLOAT,
    STRING,      // Small string optimization (< 23 chars inline)
    ARRAY,
    MAP,
    FUNCTION,
    GENERATOR,
    CLASS,
    INSTANCE,
    NATIVE_PTR,  // For FFI
    ERROR        // Result type for error handling
};

// Small String Optimization (SSO) - 23 chars inline on 64-bit
class SmallString {
    static constexpr size_t SSO_CAPACITY = 23;
    
    union {
        struct {
            char data[SSO_CAPACITY];
            uint8_t size;  // MSB indicates heap allocation
        } sso;
        struct {
            char* ptr;
            size_t size;
            size_t capacity;
        } heap;
    };
    
    bool isHeap() const { return sso.size & 0x80; }
    
public:
    SmallString() { sso.data[0] = '\0'; sso.size = 0; }
    explicit SmallString(const char* str);
    explicit SmallString(const std::string& str);
    SmallString(const SmallString& other);
    SmallString(SmallString&& other) noexcept;
    ~SmallString();
    
    SmallString& operator=(const SmallString& other);
    SmallString& operator=(SmallString&& other) noexcept;
    
    const char* c_str() const;
    size_t size() const;
    std::string toString() const;
    
    bool operator==(const SmallString& other) const;
    bool operator<(const SmallString& other) const;
};

// Efficient array storage using inline buffer for small arrays
class ValueArray {
    static constexpr size_t INLINE_CAPACITY = 4;
    
    union {
        Value* inline_data[INLINE_CAPACITY];
        struct {
            Value** ptr;
            size_t size;
            size_t capacity;
        } heap;
    };
    bool isHeap;
    
public:
    ValueArray();
    ~ValueArray();
    ValueArray(const ValueArray&) = delete;
    ValueArray& operator=(const ValueArray&) = delete;
    ValueArray(ValueArray&& other) noexcept;
    ValueArray& operator=(ValueArray&& other) noexcept;
    
    void push(Value* val);
    Value* get(size_t idx) const;
    void set(size_t idx, Value* val);
    size_t size() const;
    bool empty() const;
};

// Map using inline storage for small maps
class ValueMap {
    static constexpr size_t INLINE_CAPACITY = 4;
    
    struct Entry {
        SmallString key;
        Value* value;
    };
    
    union {
        Entry inline_data[INLINE_CAPACITY];
        std::unordered_map<std::string, Value*>* heap;
    };
    bool isHeap;
    size_t count;
    
public:
    ValueMap();
    ~ValueMap();
    ValueMap(const ValueMap&) = delete;
    ValueMap& operator=(const ValueMap&) = delete;
    ValueMap(ValueMap&& other) noexcept;
    ValueMap& operator=(ValueMap&& other) noexcept;
    
    void set(const std::string& key, Value* value);
    Value* get(const std::string& key) const;
    bool contains(const std::string& key) const;
    size_t size() const;
};

// Result type for error handling
struct ErrorValue {
    uint32_t code;
    SmallString message;
    std::vector<std::string> traceback;
};

// Main Value class - 32 bytes total
class alignas(8) Value {
public:
    using VariantType = std::variant<
        std::monostate,                    // NIL
        bool,                              // BOOL
        int64_t,                           // INT
        double,                            // FLOAT
        SmallString,                       // STRING
        ValueArray,                        // ARRAY
        ValueMap,                          // MAP
        std::shared_ptr<FunctionObject>,   // FUNCTION
        std::shared_ptr<GeneratorObject>,  // GENERATOR
        std::shared_ptr<ClassObject>,      // CLASS
        std::shared_ptr<InstanceObject>,   // INSTANCE
        void*,                             // NATIVE_PTR
        ErrorValue                         // ERROR
    >;
    
private:
    VariantType data;
    ValueType type;
    
public:
    // Constructors
    Value() : data(std::monostate{}), type(ValueType::NIL) {}
    explicit Value(bool b) : data(b), type(ValueType::BOOL) {}
    explicit Value(int64_t i) : data(i), type(ValueType::INT) {}
    explicit Value(double f) : data(f), type(ValueType::FLOAT) {}
    explicit Value(const std::string& s) : data(SmallString(s)), type(ValueType::STRING) {}
    explicit Value(const char* s) : data(SmallString(s)), type(ValueType::STRING) {}
    
    // Factory methods
    static Value nil() { return Value(); }
    static Value fromInt(int64_t i) { return Value(i); }
    static Value fromFloat(double f) { return Value(f); }
    static Value fromString(const std::string& s) { return Value(s); }
    static Value fromBool(bool b) { return Value(b); }
    static Value makeArray();
    static Value makeMap();
    static Value makeError(uint32_t code, const std::string& msg);
    
    // Type checking (inline for performance)
    ValueType getType() const { return type; }
    bool isNil() const { return type == ValueType::NIL; }
    bool isBool() const { return type == ValueType::BOOL; }
    bool isInt() const { return type == ValueType::INT; }
    bool isFloat() const { return type == ValueType::FLOAT; }
    bool isString() const { return type == ValueType::STRING; }
    bool isArray() const { return type == ValueType::ARRAY; }
    bool isMap() const { return type == ValueType::MAP; }
    bool isFunction() const { return type == ValueType::FUNCTION; }
    bool isError() const { return type == ValueType::ERROR; }
    bool isTruthy() const;
    
    // Extractors with bounds checking
    bool asBool() const;
    int64_t asInt() const;
    double asFloat() const;
    std::string asString() const;
    
    // Array operations
    void arrayPush(const Value& val);
    Value arrayGet(size_t idx) const;
    size_t arraySize() const;
    
    // Map operations
    void mapSet(const std::string& key, const Value& val);
    Value mapGet(const std::string& key) const;
    bool mapContains(const std::string& key) const;
    size_t mapSize() const;
    
    // Comparison
    bool operator==(const Value& other) const;
    bool operator<(const Value& other) const;
    
    // String representation
    std::string toString() const;
    
    // Move semantics optimization
    Value(Value&& other) noexcept = default;
    Value& operator=(Value&& other) noexcept = default;
    
    // Copy (explicit to avoid accidental copies)
    Value(const Value& other) = default;
    Value& operator=(const Value& other) = default;
};

// Result type for error handling
template<typename T, typename E = ErrorValue>
class Result {
    std::variant<T, E> data;
    bool isOk;
    
public:
    explicit Result(T&& value) : data(std::move(value)), isOk(true) {}
    explicit Result(const E& error) : data(error), isOk(false) {}
    
    bool ok() const { return isOk; }
    bool isError() const { return !isOk; }
    
    T& value() { return std::get<T>(data); }
    const T& value() const { return std::get<T>(data); }
    E& error() { return std::get<E>(data); }
    const E& error() const { return std::get<E>(data); }
    
    T unwrap() {
        if (!isOk) throw std::runtime_error("unwrap on error");
        return std::move(std::get<T>(data));
    }
};

} // namespace kern
