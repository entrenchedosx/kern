/* *
 * kern Value implementation — both legacy Value (std::variant) and
 * TaggedValue (NaN-boxed, 8-byte) implementations.
 */

#include "bytecode/value.hpp"
#include <sstream>
#include <cmath>
#include <stdexcept>

namespace kern {

// ====================================================================
//  LEGACY Value methods (unchanged)
// ====================================================================

bool Value::isTruthy() const {
    switch (type) {
        case Type::NIL: return false;
        case Type::BOOL: return std::get<bool>(data);
        case Type::INT: return std::get<int64_t>(data) != 0;
        case Type::FLOAT: return std::get<double>(data) != 0.0;
        case Type::STRING: return !std::get<std::string>(data).empty();
        case Type::ARRAY: return !std::get<std::vector<ValuePtr>>(data).empty();
        case Type::MAP: return !std::get<std::unordered_map<std::string, ValuePtr>>(data).empty();
        case Type::GENERATOR: return true;
        case Type::VEC3: return true;
        default: return true;
    }
}

std::string Value::toString() const {
    switch (type) {
        case Type::NIL: return "null";
        case Type::BOOL: return std::get<bool>(data) ? "true" : "false";
        case Type::INT: return std::to_string(std::get<int64_t>(data));
        case Type::FLOAT: {
            std::ostringstream oss;
            double d = std::get<double>(data);
            if (std::isnan(d)) return "nan";
            if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
            if (d == static_cast<int64_t>(d)) oss << static_cast<int64_t>(d);
            else oss << d;
            return oss.str();
        }
        case Type::STRING: return std::get<std::string>(data);
        case Type::ARRAY: {
            auto& arr = std::get<std::vector<ValuePtr>>(data);
            std::string s = "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i) s += ", ";
                s += arr[i].isNil() ? "null" : arr[i].toString();
            }
            return s + "]";
        }
        case Type::MAP: return "[object]";
        case Type::FUNCTION: return "<function>";
        case Type::CLASS: return "<class>";
        case Type::INSTANCE: return "<instance>";
        case Type::GENERATOR: return "<generator>";
        case Type::PTR: return "<ptr>";
        case Type::FFI_FN: return "<ffi>";
        case Type::VEC3: {
            auto& v = std::get<Vec3Ptr>(data);
            std::ostringstream oss;
            oss << "Vec3(" << v->x << ", " << v->y << ", " << v->z << ")";
            return oss.str();
        }
        default: return "?";
    }
}

bool Value::equals(const Value& other) const {
    if (type != other.type) {
        if ((type == Type::INT || type == Type::FLOAT) && (other.type == Type::INT || other.type == Type::FLOAT)) {
            double da = type == Type::INT ? static_cast<double>(std::get<int64_t>(data)) : std::get<double>(data);
            double db = other.type == Type::INT ? static_cast<double>(std::get<int64_t>(other.data)) : std::get<double>(other.data);
            return da == db;
        }
        return false;
    }
    switch (type) {
        case Type::NIL: return true;
        case Type::BOOL: return std::get<bool>(data) == std::get<bool>(other.data);
        case Type::INT: return std::get<int64_t>(data) == std::get<int64_t>(other.data);
        case Type::FLOAT: return std::get<double>(data) == std::get<double>(other.data);
        case Type::STRING: return std::get<std::string>(data) == std::get<std::string>(other.data);
        case Type::VEC3: {
            auto& v1 = std::get<Vec3Ptr>(data);
            auto& v2 = std::get<Vec3Ptr>(other.data);
            return v1->x == v2->x && v1->y == v2->y && v1->z == v2->z;
        }
        default: return false;
    }
}

std::string Value::typeName() const {
    switch (type) {
        case Type::NIL: return "nil";
        case Type::BOOL: return "bool";
        case Type::INT: return "int";
        case Type::FLOAT: return "float";
        case Type::STRING: return "string";
        case Type::ARRAY: return "array";
        case Type::MAP: return "map";
        case Type::FUNCTION: return "function";
        case Type::CLASS: return "class";
        case Type::INSTANCE: return "instance";
        case Type::GENERATOR: return "generator";
        case Type::PTR: return "ptr";
        case Type::FFI_FN: return "ffi";
        case Type::VEC3: return "vec3";
        case Type::STRUCT: return "struct";
        default: return "unknown";
    }
}

int32_t Value::asInt() const {
    if (type != Type::INT) {
        throw std::runtime_error("Type mismatch: expected Int, got " + typeName());
    }
    return static_cast<int32_t>(std::get<int64_t>(data));
}

double Value::asFloat() const {
    if (type != Type::FLOAT) {
        throw std::runtime_error("Type mismatch: expected Float, got " + typeName());
    }
    return std::get<double>(data);
}

bool Value::asBool() const {
    return isTruthy();
}

std::string Value::asString() const {
    if (type != Type::STRING) {
        throw std::runtime_error("Type mismatch: expected String, got " + typeName());
    }
    return std::get<std::string>(data);
}

Vec3Ptr Value::asVec3() const {
    if (type != Type::VEC3) {
        throw std::runtime_error("Type mismatch: expected Vec3, got " + typeName());
    }
    return std::get<Vec3Ptr>(data);
}

// ====================================================================
//  TAGGED VALUE methods (NaN-boxed, 8-byte)
// ====================================================================

bool TaggedValue::asBool() const {
    if (!isBool())
        throw std::runtime_error("Type mismatch: expected Bool, got " + typeName());
    return payload() != 0;
}

int32_t TaggedValue::asInt32() const {
    if (!isInt32())
        throw std::runtime_error("Type mismatch: expected Int32, got " + typeName());
    // The payload is sign-extended to 48 bits during construction.
    // Truncating to 32 bits via two's complement gives the correct signed value.
    return static_cast<int32_t>(bits_ & PAYLOAD_MASK);
}

double TaggedValue::asFloat() const {
    if (isBoxed()) {
        // Allow reading int32 as float (numeric coercion)
        if (isInt32()) return static_cast<double>(asInt32());
        throw std::runtime_error("Type mismatch: expected Float, got " + typeName());
    }
    double d;
    std::memcpy(&d, &bits_, sizeof(d));
    return d;
}

int64_t TaggedValue::asInt64() const {
    if (isInt32()) return static_cast<int64_t>(asInt32());
    if (isFloat()) return static_cast<int64_t>(asFloat());
    throw std::runtime_error("Type mismatch: expected numeric, got " + typeName());
}

ObjString* TaggedValue::asStringPtr() const {
    if (!isString())
        throw std::runtime_error("Type mismatch: expected String, got " + typeName());
    return reinterpret_cast<ObjString*>(payload());
}

ObjArray* TaggedValue::asArrayPtr() const {
    if (!isArray())
        throw std::runtime_error("Type mismatch: expected Array, got " + typeName());
    return reinterpret_cast<ObjArray*>(payload());
}

ObjMap* TaggedValue::asMapPtr() const {
    if (!isMap())
        throw std::runtime_error("Type mismatch: expected Map, got " + typeName());
    return reinterpret_cast<ObjMap*>(payload());
}

ObjClosure* TaggedValue::asClosurePtr() const {
    if (!isClosure())
        throw std::runtime_error("Type mismatch: expected Closure, got " + typeName());
    return reinterpret_cast<ObjClosure*>(payload());
}

FunctionObject* TaggedValue::asFunctionPtr() const {
    if (!isFunction())
        throw std::runtime_error("Type mismatch: expected Function, got " + typeName());
    // During migration, FunctionObject is stored via legacy pointer — read directly
    return reinterpret_cast<FunctionObject*>(payload());
}

ClassObject* TaggedValue::asClassPtr() const {
    if (!isClass())
        throw std::runtime_error("Type mismatch: expected Class, got " + typeName());
    return reinterpret_cast<ClassObject*>(payload());
}

InstanceObject* TaggedValue::asInstancePtr() const {
    if (!isInstance())
        throw std::runtime_error("Type mismatch: expected Instance, got " + typeName());
    return reinterpret_cast<InstanceObject*>(payload());
}

GeneratorObject* TaggedValue::asGeneratorPtr() const {
    if (!isGenerator())
        throw std::runtime_error("Type mismatch: expected Generator, got " + typeName());
    return reinterpret_cast<GeneratorObject*>(payload());
}

Vec3Object* TaggedValue::asVec3Ptr() const {
    if (!isVec3())
        throw std::runtime_error("Type mismatch: expected Vec3, got " + typeName());
    return reinterpret_cast<Vec3Object*>(payload());
}

StructObject* TaggedValue::asStructPtr() const {
    if (!isStruct())
        throw std::runtime_error("Type mismatch: expected Struct, got " + typeName());
    return reinterpret_cast<StructObject*>(payload());
}

FfiClosure* TaggedValue::asFfiPtr() const {
    if (!isFfi())
        throw std::runtime_error("Type mismatch: expected FFI, got " + typeName());
    return reinterpret_cast<FfiClosure*>(payload());
}

void* TaggedValue::asRawPtr() const {
    if (!isPtr())
        throw std::runtime_error("Type mismatch: expected Ptr, got " + typeName());
    return reinterpret_cast<void*>(payload());
}

std::string TaggedValue::toString() const {
    if (!isBoxed()) {
        // Unboxed float
        std::ostringstream oss;
        double d;
        std::memcpy(&d, &bits_, sizeof(d));
        if (std::isnan(d)) return "nan";
        if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
        if (d == static_cast<int64_t>(d)) oss << static_cast<int64_t>(d);
        else oss << d;
        return oss.str();
    }

    switch (tag()) {
        case ValueTag::NIL: return "null";
        case ValueTag::BOOL: return payload() ? "true" : "false";
        case ValueTag::INT32: return std::to_string(asInt32());

        case ValueTag::OBJ: {
            auto* hdr = reinterpret_cast<ObjHeader*>(payload());
            if (!hdr) return "null";
            switch (hdr->type) {
                case ObjType::String: {
                    auto* s = static_cast<ObjString*>(hdr);
                    return std::string(s->chars, s->length);
                }
                case ObjType::Array: {
                    auto* a = static_cast<ObjArray*>(hdr);
                    std::string s = "[";
                    for (uint32_t i = 0; i < a->count; ++i) {
                        if (i) s += ", ";
                        s += a->elements[i].toString();
                    }
                    return s + "]";
                }
                case ObjType::Map: return "[object]";
                case ObjType::Closure: return "<function>";
                case ObjType::Class: return "<class>";
                case ObjType::Instance: return "<instance>";
                case ObjType::Generator: return "<generator>";
                case ObjType::Vec3: {
                    auto* v = reinterpret_cast<Vec3Object*>(hdr);
                    std::ostringstream oss;
                    oss << "Vec3(" << v->x << ", " << v->y << ", " << v->z << ")";
                    return oss.str();
                }
                case ObjType::Struct: return "<struct>";
                case ObjType::Ffi: return "<ffi>";
                case ObjType::RawPtr: return "<ptr>";
            }
            return "?";  // unknown ObjType
        }

        default: return "?";
    }
}

bool TaggedValue::equals(const TaggedValue& other) const {
    // Numeric cross-type comparison (int32 ↔ float)
    if (isNumeric() && other.isNumeric()) {
        double da = isInt32() ? static_cast<double>(asInt32()) : asFloat();
        double db = other.isInt32() ? static_cast<double>(other.asInt32()) : other.asFloat();
        return da == db;
    }

    // If one is boxed and the other is not, they differ
    if (isBoxed() != other.isBoxed()) return false;

    // Both unboxed floats — direct bit comparison (catches +0 vs -0 correctly)
    if (!isBoxed()) return bits_ == other.bits_;

    // Both boxed: compare tags
    if (tag() != other.tag()) return false;

    switch (tag()) {
        case ValueTag::NIL: return true;
        case ValueTag::BOOL: return payload() == other.payload();
        case ValueTag::INT32: return asInt32() == other.asInt32();

        case ValueTag::OBJ: {
            auto* a = reinterpret_cast<ObjHeader*>(payload());
            auto* b = reinterpret_cast<ObjHeader*>(other.payload());
            if (!a || !b) return a == b;  // both null → equal; one null → not equal
            if (a->type != b->type) return false;

            switch (a->type) {
                case ObjType::String: {
                    auto* sa = static_cast<ObjString*>(a);
                    auto* sb = static_cast<ObjString*>(b);
                    return sa->length == sb->length &&
                           std::memcmp(sa->chars, sb->chars, sa->length) == 0;
                }
                case ObjType::Vec3: {
                    auto* va = reinterpret_cast<Vec3Object*>(a);
                    auto* vb = reinterpret_cast<Vec3Object*>(b);
                    return va->x == vb->x && va->y == vb->y && va->z == vb->z;
                }
                default:
                    // For ARRAY, MAP, FUNCTION, etc.: pointer equality
                    return payload() == other.payload();
            }
        }

        default:
            return payload() == other.payload();
    }
}

std::string TaggedValue::typeName() const {
    if (!isBoxed()) return "float";
    switch (tag()) {
        case ValueTag::NIL:   return "nil";
        case ValueTag::BOOL:  return "bool";
        case ValueTag::INT32: return "int";

        case ValueTag::OBJ: {
            auto* hdr = reinterpret_cast<ObjHeader*>(payload());
            if (!hdr) return "null";
            switch (hdr->type) {
                case ObjType::String:   return "string";
                case ObjType::Array:    return "array";
                case ObjType::Map:      return "map";
                case ObjType::Closure:  return "function";
                case ObjType::Class:    return "class";
                case ObjType::Instance: return "instance";
                case ObjType::Generator: return "generator";
                case ObjType::Vec3:     return "vec3";
                case ObjType::Struct:   return "struct";
                case ObjType::Ffi:      return "ffi";
                case ObjType::RawPtr:   return "ptr";
            }
            return "unknown";  // unknown ObjType
        }

        default: return "unknown";
    }
}

} // namespace kern
