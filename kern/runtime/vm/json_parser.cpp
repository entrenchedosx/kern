/* *
 * kern/runtime/vm/json_parser.cpp — Lightweight recursive-descent JSON parser.
 *
 * Zero external dependencies. Returns Kern Value objects directly.
 * On malformed input, returns Value::nil() safely.
 */

#include "json_parser.hpp"
#include <cstdlib>
#include <cerrno>
#include <cmath>

namespace kern {

JsonParser::JsonParser(const std::string& input)
    : input_(input), pos_(0) {}

// ─── Character helpers ──────────────────────────────────────────────────

void JsonParser::skipWhitespace() {
    while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\t' ||
                                    input_[pos_] == '\n' || input_[pos_] == '\r')) {
        pos_++;
    }
}

char JsonParser::peek() const {
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_];
}

char JsonParser::advance() {
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_++];
}

bool JsonParser::match(char c) {
    if (peek() == c) {
        advance();
        return true;
    }
    return false;
}

bool JsonParser::eof() const {
    return pos_ >= input_.size();
}

// ─── Top-level dispatch ─────────────────────────────────────────────────

Value JsonParser::parse() {
    skipWhitespace();
    if (eof()) return Value::nil();
    Value result = parseValue();
    // After parsing a value, there may be trailing whitespace (ignore)
    return result;
}

Value JsonParser::parseValue() {
    skipWhitespace();
    char c = peek();
    switch (c) {
        case '{': return parseObject();
        case '[': return parseArray();
        case '"': return parseString();
        case 't': case 'f': case 'n': return parseKeyword();
        case '-': case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': return parseNumber();
        default:
            // Unexpected character — return nil
            return Value::nil();
    }
}

// ─── Object parsing ─────────────────────────────────────────────────────

Value JsonParser::parseObject() {
    advance(); // consume '{'
    skipWhitespace();
    if (match('}')) {
        // Empty object
        return Value::fromMap(std::unordered_map<std::string, ValuePtr>());
    }

    std::unordered_map<std::string, ValuePtr> map;
    for (;;) {
        skipWhitespace();
        if (peek() != '"') {
            // Expected string key — abort
            return Value::nil();
        }
        Value keyVal = parseString();
        if (keyVal.type != Value::Type::STRING) return Value::nil();
        std::string key = keyVal.asString();

        skipWhitespace();
        if (!match(':')) return Value::nil();

        Value val = parseValue();
        map[key] = std::make_shared<Value>(std::move(val));

        skipWhitespace();
        if (match('}')) break;
        if (!match(',')) return Value::nil();
        // Allow trailing comma before close
        skipWhitespace();
        if (peek() == '}') { advance(); break; }
    }

    return Value::fromMap(std::move(map));
}

// ─── Array parsing ──────────────────────────────────────────────────────

Value JsonParser::parseArray() {
    advance(); // consume '['
    skipWhitespace();
    if (match(']')) {
        return Value::fromArray(std::vector<ValuePtr>());
    }

    std::vector<ValuePtr> elements;
    for (;;) {
        Value elem = parseValue();
        elements.push_back(std::make_shared<Value>(std::move(elem)));

        skipWhitespace();
        if (match(']')) break;
        if (!match(',')) return Value::nil();
        skipWhitespace();
        if (peek() == ']') { advance(); break; }
    }

    return Value::fromArray(std::move(elements));
}

// ─── String parsing ─────────────────────────────────────────────────────

Value JsonParser::parseString() {
    if (!match('"')) return Value::nil();

    std::string result;
    while (!eof()) {
        char c = advance();
        if (c == '"') {
            return Value::fromString(std::move(result));
        }
        if (c == '\\') {
            if (eof()) return Value::nil();
            char esc = advance();
            switch (esc) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    // Parse 4-hex-digit Unicode escape
                    if (pos_ + 4 > input_.size()) return Value::nil();
                    std::string hex = input_.substr(pos_, 4);
                    pos_ += 4;
                    char* end = nullptr;
                    unsigned long cp = std::strtoul(hex.c_str(), &end, 16);
                    if (end != hex.c_str() + 4) return Value::nil();
                    // Basic UTF-8 encoding for code points U+0000..U+FFFF
                    if (cp < 0x80) {
                        result += static_cast<char>(cp);
                    } else if (cp < 0x800) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default:
                    // Invalid escape — return nil
                    return Value::nil();
            }
        } else {
            result += c;
        }
    }
    // Unterminated string
    return Value::nil();
}

// ─── Number parsing ─────────────────────────────────────────────────────

Value JsonParser::parseNumber() {
    size_t start = pos_;

    // Optional minus
    if (peek() == '-') advance();

    // Integer part
    if (peek() == '0') {
        advance();
    } else if (peek() >= '1' && peek() <= '9') {
        advance();
        while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
            advance();
    } else {
        return Value::nil();
    }

    bool isFloat = false;

    // Fractional part
    if (peek() == '.') {
        isFloat = true;
        advance();
        if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9')
            return Value::nil();
        while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
            advance();
    }

    // Exponent part
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9')
            return Value::nil();
        while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9')
            advance();
    }

    std::string numStr = input_.substr(start, pos_ - start);

    if (isFloat) {
        char* end = nullptr;
        double val = std::strtod(numStr.c_str(), &end);
        if (end != numStr.c_str() + numStr.size()) return Value::nil();
        return Value::fromFloat(val);
    } else {
        char* end = nullptr;
        errno = 0;
        int64_t val = std::strtoll(numStr.c_str(), &end, 10);
        if (end != numStr.c_str() + numStr.size()) return Value::nil();
        if (errno == ERANGE) {
            // Overflow — fall back to float
            char* end2 = nullptr;
            double dval = std::strtod(numStr.c_str(), &end2);
            if (end2 != numStr.c_str() + numStr.size()) return Value::nil();
            return Value::fromFloat(dval);
        }
        return Value::fromInt(val);
    }
}

// ─── Keyword parsing (true / false / null) ──────────────────────────────

Value JsonParser::parseKeyword() {
    if (match('t')) {
        if (pos_ + 3 <= input_.size() && input_.substr(pos_, 3) == "rue") {
            pos_ += 3;
            return Value::fromBool(true);
        }
        return Value::nil();
    }
    if (match('f')) {
        if (pos_ + 4 <= input_.size() && input_.substr(pos_, 4) == "alse") {
            pos_ += 4;
            return Value::fromBool(false);
        }
        return Value::nil();
    }
    if (match('n')) {
        if (pos_ + 3 <= input_.size() && input_.substr(pos_, 3) == "ull") {
            pos_ += 3;
            return Value::nil();
        }
        return Value::nil();
    }
    return Value::nil();
}

} // namespace kern
