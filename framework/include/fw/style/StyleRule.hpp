#pragma once

#include <string>
#include <unordered_map>

namespace fw::style {

enum class SelectorType {
    Type,
    Class,
    Id
};

struct StyleRule {
    SelectorType selectorType{SelectorType::Type};
    std::string selectorValue;
    std::unordered_map<std::string, std::string> declarations;
    int order{0};
};

} // namespace fw::style

