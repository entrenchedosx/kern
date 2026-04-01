/* *
 * kern Value implementation
 */

#include "value.hpp"
#include <sstream>
#include <cmath>

namespace kern {

bool Value::isTruthy() const {
    switch (type) {
        case Type::NIL: return false;
        case Type::BOOL: return std::get<bool>(data);
        case Type::INT: return std::get<int64_t>(data) != 0;
        case Type::FLOAT: return std::get<double>(data) != 0.0;
        case Type::STRING: return !std::get<std::string>(data).empty();
        case Type::ARRAY: return !std::get<std::vector<ValuePtr>>(data).empty();
        case Type::MAP: return !std::get<std::unordered_map<std::string, ValuePtr>>(data).empty();
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
                s += arr[i] ? arr[i]->toString() : "null";
            }
            return s + "]";
        }
        case Type::MAP: return "[object]";
        case Type::FUNCTION: return "<function>";
        case Type::CLASS: return "<class>";
        case Type::INSTANCE: return "<instance>";
        case Type::PTR: return "<ptr>";
        default: return "?";
    }
}

bool Value::equals(const Value& other) const {
    if (type != other.type) return false;
    switch (type) {
        case Type::NIL: return true;
        case Type::BOOL: return std::get<bool>(data) == std::get<bool>(other.data);
        case Type::INT: return std::get<int64_t>(data) == std::get<int64_t>(other.data);
        case Type::FLOAT: return std::get<double>(data) == std::get<double>(other.data);
        case Type::STRING: return std::get<std::string>(data) == std::get<std::string>(other.data);
        default: return false;
    }
}

} // namespace kern
