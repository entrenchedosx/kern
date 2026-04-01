#pragma once

#include "fw/document/Document.hpp"
#include "fw/layout/LayoutTree.hpp"
#include "fw/style/StyleResolver.hpp"

namespace fw::layout {

class LayoutEngine {
public:
    LayoutTree compute(
        const document::Document& document,
        const style::StyleResolver::NodeStyleTable& styles,
        float viewportWidth,
        float viewportHeight) const;

private:
    LayoutNode::Ptr buildNode(
        const document::Node::Ptr& node,
        const style::StyleResolver::NodeStyleTable& styles,
        float x,
        float& y,
        float availableWidth) const;
};

} // namespace fw::layout

