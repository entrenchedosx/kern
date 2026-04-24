/* *
 * kern/core/value.cpp - Value System Implementation
 * 
 * Implements the new inline Value class with:
 * - Small String Optimization (SSO)
 * - Inline arrays and maps for small sizes
 * - Move semantics
 * - No shared_ptr overhead
 */
#include "value.hpp"
#include <cstring>
#include <algorithm>

namespace kern {

// ============================================================================
// SmallString Implementation
// ============================================================================

SmallString::SmallString(const char* str) {
    size_t len = std::strlen(str);
    if (len <= SSO_CAPACITY) {
        std::memcpy(sso.data, str, len + 1);
        sso.size = static_cast<uint8_t>(len);
    } else {
        heap.size = len;
        heap.capacity = len + 1;
        heap.ptr = new char[heap.capacity];
        std::memcpy(heap.ptr, str, len + 1);
        sso.size = 0x80;  // Mark as heap
    }
}

SmallString::SmallString(const std::string& str) : SmallString(str.c_str()) {}

SmallString::SmallString(const SmallString& other) {
    if (other.isHeap()) {
        heap.size = other.heap.size;
        heap.capacity = other.heap.capacity;
        heap.ptr = new char[heap.capacity];
        std::memcpy(heap.ptr, other.heap.ptr, heap.size + 1);
        sso.size = 0x80;
    } else {
        sso = other.sso;
    }
}

SmallString::SmallString(SmallString&& other) noexcept {
    if (other.isHeap()) {
        heap = other.heap;
        sso.size = 0x80;
        other.sso.size = 0;  // Empty other
    } else {
        sso = other.sso;
    }
}

SmallString::~SmallString() {
    if (isHeap()) {
        delete[] heap.ptr;
    }
}

SmallString& SmallString::operator=(const SmallString& other) {
    if (this != &other) {
        this->~SmallString();
        new (this) SmallString(other);
    }
    return *this;
}

SmallString& SmallString::operator=(SmallString&& other) noexcept {
    if (this != &other) {
        this->~SmallString();
        new (this) SmallString(std::move(other));
    }
    return *this;
}

const char* SmallString::c_str() const {
    return isHeap() ? heap.ptr : sso.data;
}

size_t SmallString::size() const {
    return isHeap() ? heap.size : sso.size;
}

std::string SmallString::toString() const {
    return std::string(c_str(), size());
}

bool SmallString::operator==(const SmallString& other) const {
    if (size() != other.size()) return false;
    return std::memcmp(c_str(), other.c_str(), size()) == 0;
}

bool SmallString::operator<(const SmallString& other) const {
    return std::strcmp(c_str(), other.c_str()) < 0;
}

// ============================================================================
// ValueArray Implementation
// ============================================================================

ValueArray::ValueArray() : isHeap(false) {
    std::memset(inline_data, 0, sizeof(inline_data));
}

ValueArray::~ValueArray() {
    if (isHeap) {
        delete[] heap.ptr;
    }
}

ValueArray::ValueArray(ValueArray&& other) noexcept : isHeap(other.isHeap) {
    if (isHeap) {
        heap = other.heap;
        other.heap.ptr = nullptr;
        other.heap.size = 0;
    } else {
        std::memcpy(inline_data, other.inline_data, sizeof(inline_data));
    }
}

ValueArray& ValueArray::operator=(ValueArray&& other) noexcept {
    if (this != &other) {
        this->~ValueArray();
        new (this) ValueArray(std::move(other));
    }
    return *this;
}

void ValueArray::push(Value* val) {
    if (!isHeap) {
        // Count current size
        size_t count = 0;
        while (count < INLINE_CAPACITY && inline_data[count] != nullptr) {
            count++;
        }
        if (count < INLINE_CAPACITY) {
            inline_data[count] = val;
            return;
        }
        // Convert to heap
        Value** newPtr = new Value*[INLINE_CAPACITY * 2];
        for (size_t i = 0; i < INLINE_CAPACITY; i++) {
            newPtr[i] = inline_data[i];
        }
        newPtr[INLINE_CAPACITY] = val;
        heap.ptr = newPtr;
        heap.size = INLINE_CAPACITY + 1;
        heap.capacity = INLINE_CAPACITY * 2;
        isHeap = true;
    } else {
        if (heap.size >= heap.capacity) {
            size_t newCap = heap.capacity * 2;
            Value** newPtr = new Value*[newCap];
            std::memcpy(newPtr, heap.ptr, heap.size * sizeof(Value*));
            delete[] heap.ptr;
            heap.ptr = newPtr;
            heap.capacity = newCap;
        }
        heap.ptr[heap.size++] = val;
    }
}

Value* ValueArray::get(size_t idx) const {
    if (!isHeap) {
        return idx < INLINE_CAPACITY ? inline_data[idx] : nullptr;
    }
    return idx < heap.size ? heap.ptr[idx] : nullptr;
}

void ValueArray::set(size_t idx, Value* val) {
    if (!isHeap) {
        if (idx < INLINE_CAPACITY) {
            inline_data[idx] = val;
        }
    } else {
        if (idx < heap.size) {
            heap.ptr[idx] = val;
        }
    }
}

size_t ValueArray::size() const {
    if (!isHeap) {
        size_t count = 0;
        while (count < INLINE_CAPACITY && inline_data[count] != nullptr) {
            count++;
        }
        return count;
    }
    return heap.size;
}

bool ValueArray::empty() const {
    return size() == 0;
}

// ============================================================================
// ValueMap Implementation
// ============================================================================

ValueMap::ValueMap() : isHeap(false), count(0) {
    for (auto& entry : inline_data) {
        entry.value = nullptr;
    }
}

ValueMap::~ValueMap() {
    if (isHeap) {
        delete heap;
    }
}

ValueMap::ValueMap(ValueMap&& other) noexcept 
    : isHeap(other.isHeap), count(other.count) {
    if (isHeap) {
        heap = other.heap;
        other.heap = nullptr;
    } else {
        for (size_t i = 0; i < INLINE_CAPACITY; i++) {
            inline_data[i] = other.inline_data[i];
        }
    }
    other.count = 0;
}

ValueMap& ValueMap::operator=(ValueMap&& other) noexcept {
    if (this != &other) {
        this->~ValueMap();
        new (this) ValueMap(std::move(other));
    }
    return *this;
}

void ValueMap::set(const std::string& key, Value* value) {
    if (!isHeap) {
        // Check if key exists in inline storage
        for (size_t i = 0; i < INLINE_CAPACITY; i++) {
            if (inline_data[i].value != nullptr && 
                inline_data[i].key.toString() == key) {
                inline_data[i].value = value;
                return;
            }
        }
        // Find empty slot
        for (size_t i = 0; i < INLINE_CAPACITY; i++) {
            if (inline_data[i].value == nullptr) {
                inline_data[i].key = SmallString(key);
                inline_data[i].value = value;
                count++;
                return;
            }
        }
        // Convert to heap
        auto* newHeap = new std::unordered_map<std::string, Value*>();
        for (size_t i = 0; i < INLINE_CAPACITY; i++) {
            if (inline_data[i].value != nullptr) {
                (*newHeap)[inline_data[i].key.toString()] = inline_data[i].value;
            }
        }
        (*newHeap)[key] = value;
        heap = newHeap;
        isHeap = true;
        count++;
    } else {
        (*heap)[key] = value;
    }
}

Value* ValueMap::get(const std::string& key) const {
    if (!isHeap) {
        for (size_t i = 0; i < INLINE_CAPACITY; i++) {
            if (inline_data[i].value != nullptr && 
                inline_data[i].key.toString() == key) {
                return inline_data[i].value;
            }
        }
        return nullptr;
    }
    auto it = heap->find(key);
    return it != heap->end() ? it->second : nullptr;
}

bool ValueMap::contains(const std::string& key) const {
    return get(key) != nullptr;
}

size_t ValueMap::size() const {
    if (!isHeap) return count;
    return heap->size();
}

// ============================================================================
// Value Implementation
// ============================================================================

bool Value::isTruthy() const {
    switch (type) {
        case ValueType::NIL: return false;
        case ValueType::BOOL: return std::get<bool>(data);
        case ValueType::INT: return std::get<int64_t>(data) != 0;
        case ValueType::FLOAT: return std::get<double>(data) != 0.0;
        case ValueType::STRING: return !std::get<SmallString>(data).empty();
        case ValueType::ARRAY: return std::get<ValueArray>(data).size() > 0;
        case ValueType::MAP: return std::get<ValueMap>(data).size() > 0;
        default: return true;
    }
}

bool Value::asBool() const {
    if (type == ValueType::BOOL) return std::get<bool>(data);
    if (type == ValueType::INT) return std::get<int64_t>(data) != 0;
    return isTruthy();
}

int64_t Value::asInt() const {
    switch (type) {
        case ValueType::INT: return std::get<int64_t>(data);
        case ValueType::FLOAT: return static_cast<int64_t>(std::get<double>(data));
        case ValueType::BOOL: return std::get<bool>(data) ? 1 : 0;
        default: return 0;
    }
}

double Value::asFloat() const {
    switch (type) {
        case ValueType::FLOAT: return std::get<double>(data);
        case ValueType::INT: return static_cast<double>(std::get<int64_t>(data));
        default: return 0.0;
    }
}

std::string Value::asString() const {
    if (type == ValueType::STRING) {
        return std::get<SmallString>(data).toString();
    }
    return toString();
}

std::string Value::toString() const {
    switch (type) {
        case ValueType::NIL: return "nil";
        case ValueType::BOOL: return std::get<bool>(data) ? "true" : "false";
        case ValueType::INT: return std::to_string(std::get<int64_t>(data));
        case ValueType::FLOAT: return std::to_string(std::get<double>(data));
        case ValueType::STRING: return std::get<SmallString>(data).toString();
        case ValueType::ARRAY: return "[array]";
        case ValueType::MAP: return "{map}";
        case ValueType::ERROR: return "Error: " + std::get<ErrorValue>(data).message.toString();
        default: return "[object]";
    }
}

bool Value::operator==(const Value& other) const {
    if (type != other.type) return false;
    
    switch (type) {
        case ValueType::NIL: return true;
        case ValueType::BOOL: return std::get<bool>(data) == std::get<bool>(other.data);
        case ValueType::INT: return std::get<int64_t>(data) == std::get<int64_t>(other.data);
        case ValueType::FLOAT: return std::get<double>(data) == std::get<double>(other.data);
        case ValueType::STRING: return std::get<SmallString>(data) == std::get<SmallString>(other.data);
        default: return false;  // Reference equality for complex types
    }
}

bool Value::operator<(const Value& other) const {
    if (type != other.type) return type < other.type;
    
    switch (type) {
        case ValueType::INT: return std::get<int64_t>(data) < std::get<int64_t>(other.data);
        case ValueType::FLOAT: return std::get<double>(data) < std::get<double>(other.data);
        case ValueType::STRING: return std::get<SmallString>(data) < std::get<SmallString>(other.data);
        default: return false;
    }
}

// Factory methods
Value Value::makeArray() {
    Value v;
    v.data = ValueArray();
    v.type = ValueType::ARRAY;
    return v;
}

Value Value::makeMap() {
    Value v;
    v.data = ValueMap();
    v.type = ValueType::MAP;
    return v;
}

Value Value::makeError(uint32_t code, const std::string& msg) {
    Value v;
    ErrorValue err;
    err.code = code;
    err.message = SmallString(msg);
    v.data = std::move(err);
    v.type = ValueType::ERROR;
    return v;
}

// Array operations
void Value::arrayPush(const Value& val) {
    if (type != ValueType::ARRAY) return;
    // Note: In real implementation, this needs proper memory management
    // This is simplified for demonstration
}

Value Value::arrayGet(size_t idx) const {
    if (type != ValueType::ARRAY) return Value();
    // Simplified
    return Value();
}

size_t Value::arraySize() const {
    if (type != ValueType::ARRAY) return 0;
    return std::get<ValueArray>(data).size();
}

// Map operations
void Value::mapSet(const std::string& key, const Value& val) {
    if (type != ValueType::MAP) return;
    // Simplified
}

Value Value::mapGet(const std::string& key) const {
    if (type != ValueType::MAP) return Value();
    // Simplified
    return Value();
}

bool Value::mapContains(const std::string& key) const {
    if (type != ValueType::MAP) return false;
    return std::get<ValueMap>(data).contains(key);
}

size_t Value::mapSize() const {
    if (type != ValueType::MAP) return 0;
    return std::get<ValueMap>(data).size();
}

} // namespace kern
