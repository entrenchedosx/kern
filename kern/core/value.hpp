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
    
    explicit SmallString(const char* str) {
        size_t len = std::strlen(str);
        if (len < SSO_CAPACITY) {
            std::memcpy(sso.data, str, len);
            sso.data[len] = '\0';
            sso.size = static_cast<uint8_t>(len);
        } else {
            heap.ptr = new char[len + 1];
            std::memcpy(heap.ptr, str, len);
            heap.ptr[len] = '\0';
            heap.size = len;
            heap.capacity = len;
            sso.size = 0x80;
        }
    }
    
    explicit SmallString(const std::string& str) {
        size_t len = str.size();
        if (len < SSO_CAPACITY) {
            std::memcpy(sso.data, str.data(), len);
            sso.data[len] = '\0';
            sso.size = static_cast<uint8_t>(len);
        } else {
            heap.ptr = new char[len + 1];
            std::memcpy(heap.ptr, str.data(), len);
            heap.ptr[len] = '\0';
            heap.size = len;
            heap.capacity = len;
            sso.size = 0x80;
        }
    }
    
    SmallString(const SmallString& other) {
        if (!other.isHeap()) {
            std::memcpy(sso.data, other.sso.data, SSO_CAPACITY);
            sso.size = other.sso.size;
        } else {
            heap.ptr = new char[other.heap.size + 1];
            std::memcpy(heap.ptr, other.heap.ptr, other.heap.size);
            heap.ptr[other.heap.size] = '\0';
            heap.size = other.heap.size;
            heap.capacity = other.heap.size;
            sso.size = 0x80;
        }
    }
    
    SmallString(SmallString&& other) noexcept {
        if (!other.isHeap()) {
            std::memcpy(sso.data, other.sso.data, SSO_CAPACITY);
            sso.size = other.sso.size;
        } else {
            heap.ptr = other.heap.ptr;
            heap.size = other.heap.size;
            heap.capacity = other.heap.capacity;
            sso.size = 0x80;
            other.sso.data[0] = '\0';
            other.sso.size = 0;
        }
    }
    
    ~SmallString() {
        if (isHeap()) delete[] heap.ptr;
    }
    
    SmallString& operator=(const SmallString& other) {
        if (this != &other) {
            if (isHeap()) delete[] heap.ptr;
            if (!other.isHeap()) {
                std::memcpy(sso.data, other.sso.data, SSO_CAPACITY);
                sso.size = other.sso.size;
            } else {
                heap.ptr = new char[other.heap.size + 1];
                std::memcpy(heap.ptr, other.heap.ptr, other.heap.size);
                heap.ptr[other.heap.size] = '\0';
                heap.size = other.heap.size;
                heap.capacity = other.heap.size;
                sso.size = 0x80;
            }
        }
        return *this;
    }
    
    SmallString& operator=(SmallString&& other) noexcept {
        if (this != &other) {
            if (isHeap()) delete[] heap.ptr;
            if (!other.isHeap()) {
                std::memcpy(sso.data, other.sso.data, SSO_CAPACITY);
                sso.size = other.sso.size;
            } else {
                heap.ptr = other.heap.ptr;
                heap.size = other.heap.size;
                heap.capacity = other.heap.capacity;
                sso.size = 0x80;
                other.sso.data[0] = '\0';
                other.sso.size = 0;
            }
        }
        return *this;
    }
    
    const char* c_str() const {
        return isHeap() ? heap.ptr : sso.data;
    }
    
    size_t size() const {
        return isHeap() ? heap.size : static_cast<size_t>(sso.size);
    }
    
    std::string toString() const {
        return std::string(c_str(), size());
    }
    
    bool operator==(const SmallString& other) const {
        if (size() != other.size()) return false;
        return std::memcmp(c_str(), other.c_str(), size()) == 0;
    }
    
    bool operator<(const SmallString& other) const {
        return std::strcmp(c_str(), other.c_str()) < 0;
    }
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

// Specialization for Result<void> – std::variant cannot hold void
template<>
class Result<void, ErrorValue> {
    bool isOk_;
    ErrorValue error_;
public:
    Result() : isOk_(true) {}
    explicit Result(const ErrorValue& error) : isOk_(false), error_(error) {}
    bool ok() const { return isOk_; }
    bool isError() const { return !isOk_; }
    ErrorValue& error() { return error_; }
    const ErrorValue& error() const { return error_; }
    void unwrap() {
        if (!isOk_) throw std::runtime_error("unwrap on error");
    }
};

} // namespace kern
