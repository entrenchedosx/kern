#pragma once

#include "fw/style/StyleRule.hpp"

#include <string>
#include <vector>

namespace fw::style {

class StyleSheet {
public:
    static StyleSheet parse(const std::string& source);

    const std::vector<StyleRule>& rules() const noexcept { return rules_; }
    void addRule(StyleRule rule) { rules_.push_back(std::move(rule)); }

private:
    std::vector<StyleRule> rules_;
};

} // namespace fw::style

