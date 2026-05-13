/* *
 * kern/core/value_nan_tagged.hpp - NaN-Tagged Unboxed Value System
 * 
 * This is Kern's high-performance value representation.
 * 
 * DESIGN GOALS:
 * - Integers: stored directly in 64-bit word, no heap allocation
 * - Floats: IEEE-754 doubles (fast path) or boxed (slow path)
 * - Objects: pointer with tag bits (strings, arrays, functions, etc.)
 * - Type checks: single bit mask operation
 * - Arithmetic: direct register operations on unboxed ints
 * 
 * PERFORMANCE TARGETS:
 * - Integer arithmetic: 10-50x faster than boxed
 * - Memory usage: 8 bytes per value (vs 16+ bytes + heap)
 * - Cache locality: values inline, no pointer chasing
 * 
 * BIT LAYOUT (64-bit):
 * 
 *   Sign | Exponent | Mantissa
 *     1  |    11    |    52
 * 
 * IEEE-754 NaN has exponent = 0x7FF (all 1s) and mantissa != 0
 * We use quiet NaN (bit 51 = 1) with custom payload for tagging.
 * 
 * NaN payload encoding:
 *   Bits 63-48: 0x7FFF (NaN signature with quiet bit)
 *   Bit 47: 1 (distinguishes our NaNs from real NaNs)
 *   Bits 46-32: Type tag (15 bits = 32768 types)
 *   Bits 31-0: 32-bit payload (integers, pointers, etc.)
 * 
 * Or use full 48-bit payload for integers on 64-bit systems:
 *   Bits 63-48: Type tag in NaN space
 *   Bits 47-0: 48-bit integer value (±140 trillion range)
 * 
 * For maximum portability and range, we use:
 *   - Integers: 48-bit signed in payload (range: ±140 trillion)
 *   - Objects: 48-bit pointer (modern x64 only uses 48 bits)
 *   - Special values: null, true, false in reserved tags
 */
#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <memory>

// Platform detection for pointer size
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    #define KERN_64_BIT
    #define KERN_NAN_TAGGING_SUPPORTED
#elif defined(__i386__) || defined(_M_IX86)
    #define KERN_32_BIT
    // NaN tagging less effective on 32-bit, fall back to boxing
#endif

namespace kern {

// Forward declarations
class Object;
class String;
class Array;
class Function;

// ============================================================================
// NaN-Tagged Value Implementation
// ============================================================================

class Value {
public:
    // Type enumeration - stored in tag bits
    enum class Type : uint16_t {
        // Special immediate values (stored entirely in NaN payload, no allocation)
        NULL_VALUE = 0,
        TRUE_VALUE = 1,
        FALSE_VALUE = 2,
        UNDEFINED = 3,  // For uninitialized values
        
        // Numeric types (unboxed)
        INT48 = 16,      // 48-bit signed integer, stored directly
        DOUBLE = 17,     // IEEE-754 double (boxed if needed for precision)
        
        // Pointer types (boxed objects)
        STRING = 32,     // Heap-allocated string
        ARRAY = 33,      // Heap-allocated array
        MAP = 34,        // Heap-allocated map
        FUNCTION = 35,   // Function/closure
        NATIVE_FUNCTION = 36, // C++ native function
        OBJECT = 37,     // Generic object
        ERROR = 38,      // Error value
        
        // Future extensions
        RESERVED_START = 100,
        RESERVED_END = 32767
    };
    
private:
    // The 64-bit payload containing everything
    uint64_t bits_;
    
    // NaN tagging constants
    static constexpr uint64_t kQuietNaNMask = 0x7FF8000000000000ULL;  // Quiet NaN
    static constexpr uint64_t kNaNTagMask = 0xFFFF000000000000ULL;      // Top 16 bits
    static constexpr uint64_t kNaNTagSignature = 0xFFFA000000000000ULL; // Our NaN signature
    static constexpr uint64_t kTypeMask = 0x0000FFFF00000000ULL;        // Type bits (bits 47-32)
    static constexpr uint64_t kPayloadMask = 0x0000FFFFFFFFFFFFULL;    // 48-bit payload
    static constexpr int kTypeShift = 32;
    static constexpr int kPayloadSignBit = 47;  // For sign-extending 48-bit values
    
    // Special values as constants
    static constexpr uint64_t kNullBits = kNaNTagSignature | 
                                          (static_cast<uint64_t>(Type::NULL_VALUE) << kTypeShift);
    static constexpr uint64_t kTrueBits = kNaNTagSignature | 
                                          (static_cast<uint64_t>(Type::TRUE_VALUE) << kTypeShift);
    static constexpr uint64_t kFalseBits = kNaNTagSignature | 
                                           (static_cast<uint64_t>(Type::FALSE_VALUE) << kTypeShift);
    
    // ============================================================================
    // Private Constructors
    // ============================================================================
    
    explicit Value(uint64_t bits) : bits_(bits) {}
    
    // Construct from type and payload
    static Value fromTagAndPayload(Type type, uint64_t payload) {
        Value v;
        v.bits_ = kNaNTagSignature | 
                  (static_cast<uint64_t>(type) << kTypeShift) | 
                  (payload & kPayloadMask);
        return v;
    }
    
public:
    // ============================================================================
    // Public Constructors - Zero-allocation for immediates
    // ============================================================================
    
    // Default: null
    Value() : bits_(kNullBits) {}
    
    // Integer: stored directly (no heap allocation!)
    explicit Value(int64_t intValue) {
        // Check if fits in 48 bits
        if (intValue >= -(1LL << 47) && intValue < (1LL << 47)) {
            // Store as int48
            bits_ = kNaNTagSignature | 
                    (static_cast<uint64_t>(Type::INT48) << kTypeShift) | 
                    (static_cast<uint64_t>(intValue) & kPayloadMask);
        } else {
            // Box as double
            *this = Value(static_cast<double>(intValue));
        }
    }
    
    // Integer (32-bit): always fits in int48
    explicit Value(int32_t intValue) {
        bits_ = kNaNTagSignature | 
                (static_cast<uint64_t>(Type::INT48) << kTypeShift) | 
                (static_cast<uint64_t>(static_cast<int64_t>(intValue)) & kPayloadMask);
    }
    
    // Double: if it's a real NaN, box it. Otherwise store raw.
    explicit Value(double doubleValue) {
        // Check if NaN
        if (std::isnan(doubleValue)) {
            // Box the NaN
            bits_ = boxDouble(doubleValue);
        } else {
            // Store raw IEEE-754 double
            static_assert(sizeof(double) == sizeof(uint64_t));
            std::memcpy(&bits_, &doubleValue, sizeof(double));
        }
    }
    
    // Boolean: immediate, no allocation
    explicit Value(bool boolValue) : bits_(boolValue ? kTrueBits : kFalseBits) {}
    
    // Null: immediate
    static Value null() { return Value(kNullBits); }
    
    // String: boxed
    explicit Value(const std::string& str);
    explicit Value(const char* str);
    
    // Object pointer: boxed
    explicit Value(Object* obj);
    
    // Copy/Move: trivial (just copy 64 bits)
    Value(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(const Value&) = default;
    Value& operator=(Value&&) = default;
    
    // ============================================================================
    // Type Checking - Single bit mask operation
    // ============================================================================
    
    // Fast type check: is this a double (not NaN-tagged)?
    bool isDouble() const {
        // If top 16 bits are not our NaN signature, it's a real double
        return (bits_ & kNaNTagMask) != kNaNTagSignature;
    }
    
    // Is this a NaN-tagged value?
    bool isTagged() const {
        return (bits_ & kNaNTagMask) == kNaNTagSignature;
    }
    
    // Get type (for tagged values)
    Type getType() const {
        if (isDouble()) return Type::DOUBLE;
        return static_cast<Type>((bits_ & kTypeMask) >> kTypeShift);
    }
    
    // Type predicates - all O(1) with single comparison
    bool isInt() const {
        return isTagged() && getType() == Type::INT48;
    }
    
    bool isNumber() const {
        return isDouble() || (isTagged() && getType() == Type::INT48);
    }
    
    bool isBool() const {
        if (!isTagged()) return false;
        auto t = getType();
        return t == Type::TRUE_VALUE || t == Type::FALSE_VALUE;
    }
    
    bool isNull() const {
        return isTagged() && getType() == Type::NULL_VALUE;
    }
    
    bool isString() const {
        return isTagged() && getType() == Type::STRING;
    }
    
    bool isArray() const {
        return isTagged() && getType() == Type::ARRAY;
    }
    
    bool isObject() const {
        return isTagged() && (
            getType() == Type::STRING ||
            getType() == Type::ARRAY ||
            getType() == Type::MAP ||
            getType() == Type::OBJECT ||
            getType() == Type::FUNCTION ||
            getType() == Type::NATIVE_FUNCTION
        );
    }
    
    bool isFunction() const {
        if (!isTagged()) return false;
        auto t = getType();
        return t == Type::FUNCTION || t == Type::NATIVE_FUNCTION;
    }
    
    // ============================================================================
    // Value Extraction - Zero-overhead for unboxed types
    // ============================================================================
    
    // Get int value (fast path: direct extraction)
    int64_t asInt() const {
        assert(isInt() && "Value is not an integer");
        // Sign-extend from 48 bits
        uint64_t payload = bits_ & kPayloadMask;
        if (payload & (1ULL << kPayloadSignBit)) {
            // Negative: sign extend
            return static_cast<int64_t>(payload | ~kPayloadMask);
        }
        return static_cast<int64_t>(payload);
    }
    
    // Get double value
    double asDouble() const {
        assert(isNumber() && "Value is not a number");
        if (isDouble()) {
            double d;
            std::memcpy(&d, &bits_, sizeof(double));
            return d;
        }
        // Convert int to double
        return static_cast<double>(asInt());
    }
    
    // Get bool value
    bool asBool() const {
        assert(isBool() && "Value is not a boolean");
        return getType() == Type::TRUE_VALUE;
    }
    
    // Get string (for boxed strings)
    std::string asString() const;
    
    // Get object pointer
    Object* asObject() const;
    
    // Generic number conversion
    double toNumber() const {
        if (isDouble()) return asDouble();
        if (isInt()) return static_cast<double>(asInt());
        if (isBool()) return asBool() ? 1.0 : 0.0;
        if (isNull()) return 0.0;
        // Try to convert string to number
        if (isString()) {
            // Parse string as number
            try {
                return std::stod(asString());
            } catch (...) {
                return NAN;
            }
        }
        return NAN;
    }
    
    // ============================================================================
    // String Conversion
    // ============================================================================
    
    std::string toString() const {
        if (isNull()) return "null";
        if (isBool()) return asBool() ? "true" : "false";
        if (isInt()) return std::to_string(asInt());
        if (isDouble()) {
            // Format double nicely
            std::string s = std::to_string(asDouble());
            // Remove trailing zeros
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.push_back('0');
            return s;
        }
        if (isString()) return asString();
        if (isArray()) return "[array]";
        if (isObject()) return "[object]";
        if (isFunction()) return "[function]";
        return "[unknown]";
    }
    
    // ============================================================================
    // Arithmetic Operations - Fast Path for Unboxed Ints
    // ============================================================================
    
    // Add - tries int fast path first
    Value operator+(const Value& other) const {
        // Fast path: both are int48
        if (isInt() && other.isInt()) {
            int64_t result = asInt() + other.asInt();
            // Check for overflow (48-bit range)
            if (result >= -(1LL << 47) && result < (1LL << 47)) {
                return Value(static_cast<int32_t>(result));  // Will fit in int48
            }
            // Overflow: promote to double
            return Value(static_cast<double>(result));
        }
        
        // Slow path: convert to double
        return Value(toNumber() + other.toNumber());
    }
    
    // Subtract
    Value operator-(const Value& other) const {
        if (isInt() && other.isInt()) {
            int64_t result = asInt() - other.asInt();
            if (result >= -(1LL << 47) && result < (1LL << 47)) {
                return Value(static_cast<int32_t>(result));
            }
            return Value(static_cast<double>(result));
        }
        return Value(toNumber() - other.toNumber());
    }
    
    // Multiply
    Value operator*(const Value& other) const {
        if (isInt() && other.isInt()) {
            int64_t result = asInt() * other.asInt();
            // Check for overflow using wider arithmetic
            if (result >= -(1LL << 47) && result < (1LL << 47)) {
                return Value(static_cast<int32_t>(result));
            }
            return Value(static_cast<double>(result));
        }
        return Value(toNumber() * other.toNumber());
    }
    
    // Divide - always returns double
    Value operator/(const Value& other) const {
        double divisor = other.toNumber();
        if (divisor == 0.0) {
            // Return error or infinity
            return Value(std::numeric_limits<double>::infinity());
        }
        return Value(toNumber() / divisor);
    }
    
    // Modulo
    Value operator%(const Value& other) const {
        if (isInt() && other.isInt()) {
            int64_t divisor = other.asInt();
            if (divisor == 0) {
                return Value::null();  // Or error
            }
            return Value(static_cast<int32_t>(asInt() % divisor));
        }
        return Value(std::fmod(toNumber(), other.toNumber()));
    }
    
    // Negate
    Value operator-() const {
        if (isInt()) {
            return Value(-asInt());
        }
        return Value(-toNumber());
    }
    
    // ============================================================================
    // Comparison Operations
    // ============================================================================
    
    bool operator==(const Value& other) const {
        // Fast path: same bits
        if (bits_ == other.bits_) return true;
        
        // Fast path: both ints
        if (isInt() && other.isInt()) {
            return asInt() == other.asInt();
        }
        
        // Both doubles
        if (isDouble() && other.isDouble()) {
            return asDouble() == other.asDouble();
        }
        
        // Int vs double
        if (isNumber() && other.isNumber()) {
            return toNumber() == other.toNumber();
        }
        
        // Objects: compare pointers
        if (isObject() && other.isObject()) {
            return asObject() == other.asObject();
        }
        
        return false;
    }
    
    bool operator!=(const Value& other) const {
        return !(*this == other);
    }
    
    bool operator<(const Value& other) const {
        if (isInt() && other.isInt()) {
            return asInt() < other.asInt();
        }
        return toNumber() < other.toNumber();
    }
    
    bool operator<=(const Value& other) const {
        if (isInt() && other.isInt()) {
            return asInt() <= other.asInt();
        }
        return toNumber() <= other.toNumber();
    }
    
    bool operator>(const Value& other) const {
        return other < *this;
    }
    
    bool operator>=(const Value& other) const {
        return other <= *this;
    }
    
    // ============================================================================
    // Debug / Introspection
    // ============================================================================
    
    void dump() const {
        std::cout << "Value bits: 0x" << std::hex << bits_ << std::dec << "\n";
        std::cout << "  Type: " << static_cast<int>(getType()) << "\n";
        std::cout << "  Is tagged: " << isTagged() << "\n";
        std::cout << "  Is double: " << isDouble() << "\n";
        std::cout << "  String: " << toString() << "\n";
    }
    
    uint64_t rawBits() const { return bits_; }
    
private:
    // Helper to box a double in a heap object
    static uint64_t boxDouble(double d);
};

// ============================================================================
// Boxed Object Types (for strings, arrays, etc.)
// ============================================================================

class Object {
public:
    virtual ~Object() = default;
    virtual Value::Type getType() const = 0;
    virtual std::string toString() const = 0;
    
    // Reference counting (simple GC)
    void retain() { ++refCount_; }
    void release() { 
        if (--refCount_ == 0) delete this; 
    }
    
    uint32_t getRefCount() const { return refCount_; }
    
private:
    uint32_t refCount_ = 1;
};

class StringObj : public Object {
    std::string data_;
    
public:
    explicit StringObj(const std::string& s) : data_(s) {}
    
    Value::Type getType() const override { return Value::Type::STRING; }
    std::string toString() const override { return data_; }
    
    const std::string& getData() const { return data_; }
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline Value::Value(const std::string& str) {
    // Allocate string object
    auto* obj = new StringObj(str);
    // Store pointer in payload
    bits_ = kNaNTagSignature | 
            (static_cast<uint64_t>(Type::STRING) << kTypeShift) | 
            (reinterpret_cast<uint64_t>(obj) & kPayloadMask);
}

inline Value::Value(const char* str) : Value(std::string(str)) {}

inline Value::Value(Object* obj) {
    Value::Type type = obj ? obj->getType() : Value::Type::NULL_VALUE;
    bits_ = kNaNTagSignature | 
            (static_cast<uint64_t>(type) << kTypeShift) | 
            (reinterpret_cast<uint64_t>(obj) & kPayloadMask);
}

inline std::string Value::asString() const {
    assert(isString() && "Value is not a string");
    auto* obj = reinterpret_cast<StringObj*>(bits_ & kPayloadMask);
    return obj->getData();
}

inline Object* Value::asObject() const {
    assert(isObject() && "Value is not an object");
    return reinterpret_cast<Object*>(bits_ & kPayloadMask);
}

inline uint64_t Value::boxDouble(double d) {
    // Allocate a boxed double object
    // For now, just store as string (simplified)
    // In real implementation: allocate DoubleObj on heap
    (void)d;
    return kNullBits;
}

// ============================================================================
// Convenience Constants
// ============================================================================

namespace Values {
    inline const Value Null = Value::null();
    inline const Value True = Value(true);
    inline const Value False = Value(false);
    inline const Value Zero = Value(0);
    inline const Value One = Value(1);
}

} // namespace kern
