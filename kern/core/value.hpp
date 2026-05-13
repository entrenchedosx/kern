/* *
 * kern/core/value.hpp - Refactored Value System
 * 
 * Eliminates shared_ptr overhead by using a variant-based value type
 * with small-string optimization and move semantics.
 */
#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <cstdint>
#include <cstring>
#include "bytecode/value.hpp"

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
    PTR,         // Generic pointer type
    STRUCT,      // Struct/record type
    VEC3,        // 3D vector type
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

// Clean type aliases for variant compatibility
using ValueArray = std::vector<ValuePtr>;
using ValueMap = std::unordered_map<std::string, ValuePtr>;

// Forward declarations for structs defined in bytecode/value.hpp
struct Vec3Object;
struct StructObject;
struct FunctionObject;
struct GeneratorObject;
struct ClassObject;
struct InstanceObject;

using Vec3Ptr = std::shared_ptr<Vec3Object>;
using StructPtr = std::shared_ptr<StructObject>;
using FunctionPtr = std::shared_ptr<FunctionObject>;
using ClassPtr = std::shared_ptr<ClassObject>;
using InstancePtr = std::shared_ptr<InstanceObject>;
using GeneratorPtr = std::shared_ptr<GeneratorObject>;

// Result type for error handling
struct ErrorValue {
    uint32_t code;
    SmallString message;
    std::vector<std::string> traceback;
};

// Value class is defined in bytecode/value.hpp - use that definition

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
