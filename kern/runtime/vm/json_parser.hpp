/* *
 * kern/runtime/vm/json_parser.hpp — Zero-dependency recursive-descent JSON parser.
 *
 * Returns Kern Value types according to this mapping:
 *   JSON Object  → Kern Map
 *   JSON Array   → Kern Array
 *   JSON String  → Kern String
 *   JSON Number  → Kern Int (if integer) or Kern Float
 *   JSON true    → Kern Bool(true)
 *   JSON false   → Kern Bool(false)
 *   JSON null    → Kern Nil
 *
 * On invalid/malformed JSON, returns Value::nil() — never crashes the VM.
 */

#ifndef KERN_JSON_PARSER_HPP
#define KERN_JSON_PARSER_HPP

#include "bytecode/value.hpp"
#include <string>

namespace kern {

class JsonParser {
public:
    explicit JsonParser(const std::string& input);

    /// Parse the full input as a JSON value.
    /// Returns the parsed Value, or Value::nil() on error.
    Value parse();

private:
    const std::string& input_;
    size_t pos_ = 0;

    void skipWhitespace();
    char peek() const;
    char advance();
    bool match(char c);
    bool eof() const;

    Value parseValue();
    Value parseObject();
    Value parseArray();
    Value parseString();
    Value parseNumber();
    Value parseKeyword();   // true / false / null
};

} // namespace kern

#endif // KERN_JSON_PARSER_HPP
