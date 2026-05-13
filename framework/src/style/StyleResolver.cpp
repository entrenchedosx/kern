#include "fw/style/StyleResolver.hpp"

#include <algorithm>
#include <array>
#include <tuple>

namespace fw::style {

int StyleResolver::specificity(const StyleRule& rule) {
    switch (rule.selectorType) {
        case SelectorType::Id: return 100;
        case SelectorType::Class: return 10;
        case SelectorType::Type: return 1;
        default: return 0;
    }
}

bool StyleResolver::matches(const document::Node& node, const StyleRule& rule) {
    if (node.type() != document::NodeType::Element) return false;
    if (rule.selectorType == SelectorType::Type) return node.name() == rule.selectorValue;
    if (rule.selectorType == SelectorType::Class) return node.hasClass(rule.selectorValue);
    if (rule.selectorType == SelectorType::Id) return node.attribute("id") == rule.selectorValue;
    return false;
}

StyleResolver::NodeStyleTable StyleResolver::resolve(const document::Document& doc, const StyleSheet& sheet) const {
    NodeStyleTable table;
    if (!doc.root()) return table;
    resolveNode(doc.root(), sheet, {}, table);
    return table;
}

void StyleResolver::resolveNode(
    const document::Node::Ptr& node,
    const StyleSheet& sheet,
    const StyleMap& inherited,
    NodeStyleTable& out) const {
    if (!node) return;
    StyleMap computed = inherited;

    std::unordered_map<std::string, std::pair<int, int>> weight;
    for (const auto& rule : sheet.rules()) {
        if (!matches(*node, rule)) continue;
        const int spec = specificity(rule);
        for (const auto& [k, v] : rule.declarations) {
            auto it = weight.find(k);
            const bool stronger =
                it == weight.end() ||
                spec > it->second.first ||
                (spec == it->second.first && rule.order >= it->second.second);
            if (stronger) {
                computed[k] = v;
                weight[k] = {spec, rule.order};
            }
        }
    }
    out[node.get()] = computed;

    StyleMap childInherited;
    static const std::array<const char*, 3> inheritable = {"color", "font-size", "font-family"};
    for (const char* key : inheritable) {
        auto it = computed.find(key);
        if (it != computed.end()) childInherited[key] = it->second;
    }
    for (const auto& ch : node->children()) resolveNode(ch, sheet, childInherited, out);
}

} // namespace fw::style

