#include "fw/style/StyleSheet.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace fw::style {

namespace {
static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}
} // namespace

StyleSheet StyleSheet::parse(const std::string& source) {
    StyleSheet sheet;
    size_t pos = 0;
    int order = 0;
    while (pos < source.size()) {
        size_t open = source.find('{', pos);
        if (open == std::string::npos) break;
        size_t close = source.find('}', open + 1);
        if (close == std::string::npos) break;

        std::string selector = trim(source.substr(pos, open - pos));
        std::string body = source.substr(open + 1, close - open - 1);
        pos = close + 1;
        if (selector.empty()) continue;

        StyleRule rule{};
        rule.order = order++;
        if (selector[0] == '#') {
            rule.selectorType = SelectorType::Id;
            rule.selectorValue = selector.substr(1);
        } else if (selector[0] == '.') {
            rule.selectorType = SelectorType::Class;
            rule.selectorValue = selector.substr(1);
        } else {
            rule.selectorType = SelectorType::Type;
            rule.selectorValue = selector;
        }

        std::istringstream decls(body);
        std::string part;
        while (std::getline(decls, part, ';')) {
            size_t sep = part.find(':');
            if (sep == std::string::npos) continue;
            std::string key = trim(part.substr(0, sep));
            std::string value = trim(part.substr(sep + 1));
            if (!key.empty()) rule.declarations[key] = value;
        }
        sheet.addRule(std::move(rule));
    }
    return sheet;
}

} // namespace fw::style

