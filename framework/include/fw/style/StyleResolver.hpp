#pragma once

#include "fw/document/Document.hpp"
#include "fw/style/StyleSheet.hpp"

#include <unordered_map>

namespace fw::style {

using StyleMap = std::unordered_map<std::string, std::string>;

class StyleResolver {
public:
    using NodeStyleTable = std::unordered_map<const document::Node*, StyleMap>;

    NodeStyleTable resolve(const document::Document& doc, const StyleSheet& sheet) const;

private:
    static int specificity(const StyleRule& rule);
    static bool matches(const document::Node& node, const StyleRule& rule);
    void resolveNode(
        const document::Node::Ptr& node,
        const StyleSheet& sheet,
        const StyleMap& inherited,
        NodeStyleTable& out) const;
};

} // namespace fw::style

