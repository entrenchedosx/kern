#include "value.hpp"
#include <variant>
#include <string>
#include <sstream>

namespace kern {

bool Value::isTruthy() const {
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return false;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return arg != 0;
        } else if constexpr (std::is_same_v<T, double>) {
            return arg != 0.0;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return !arg.empty();
        } else if constexpr (std::is_same_v<T, std::vector<ValuePtr>>) {
            return !arg.empty();
        } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, ValuePtr>>) {
            return !arg.empty();
        } else if constexpr (std::is_same_v<T, void*>) {
            return arg != nullptr;
        } else if constexpr (std::is_same_v<T, FunctionPtr> || 
                           std::is_same_v<T, GeneratorPtr> ||
                           std::is_same_v<T, ClassPtr> ||
                           std::is_same_v<T, InstancePtr> ||
                           std::is_same_v<T, Vec3Ptr> ||
                           std::is_same_v<T, StructPtr>) {
            return arg != nullptr;
        } else {
            return true;
        }
    }, data);
}

bool Value::equals(const Value& other) const {
    if (this->type != other.type) {
        return false;
    }
    
    try {
        return std::visit([&other](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return true; // Both nil
            } else {
                return arg == std::get<T>(other.data);
            }
        }, data);
    } catch (const std::bad_variant_access&) {
        return false;
    }
}

std::string Value::toString() const {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return "nil";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, std::vector<ValuePtr>>) {
            return "[Array]";
        } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, ValuePtr>>) {
            return "[Map]";
        } else if constexpr (std::is_same_v<T, FunctionPtr>) {
            return "<function>";
        } else if constexpr (std::is_same_v<T, GeneratorPtr>) {
            return "<generator>";
        } else if constexpr (std::is_same_v<T, ClassPtr>) {
            return "<class>";
        } else if constexpr (std::is_same_v<T, InstancePtr>) {
            return "<instance>";
        } else if constexpr (std::is_same_v<T, Vec3Ptr>) {
            return "<vec3>";
        } else if constexpr (std::is_same_v<T, StructPtr>) {
            return "<struct>";
        } else if constexpr (std::is_same_v<T, void*>) {
            return "<native pointer>";
        } else {
            return "<object>";
        }
    }, data);
}

// Type name helper for error messages
std::string Value::typeName() const {
    switch (type) {
        case Type::NIL:      return "Nil";
        case Type::BOOL:     return "Bool";
        case Type::INT:      return "Int";
        case Type::FLOAT:    return "Float";
        case Type::STRING:   return "String";
        case Type::ARRAY:    return "Array";
        case Type::MAP:      return "Map";
        case Type::FUNCTION: return "Function";
        case Type::GENERATOR: return "Generator";
        case Type::CLASS:    return "Class";
        case Type::INSTANCE: return "Instance";
        case Type::PTR:      return "Pointer";
        case Type::VEC3:     return "Vec3";
        case Type::STRUCT:   return "Struct";
        default:             return "Unknown";
    }
}

// Getter methods for native bindings — type-safe with explicit checks
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

} // namespace kern
