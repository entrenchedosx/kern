#include "fw/layout/LayoutEngine.hpp"

#include <algorithm>
#include <cstdlib>

namespace fw::layout {

namespace {
static float toPx(const style::StyleMap& s, const std::string& key, float fallback = 0.0f) {
    auto it = s.find(key);
    if (it == s.end()) return fallback;
    const std::string& v = it->second;
    char* end = nullptr;
    const float parsed = std::strtof(v.c_str(), &end);
    return (end == v.c_str()) ? fallback : parsed;
}
} // namespace

LayoutTree LayoutEngine::compute(
    const document::Document& document,
    const style::StyleResolver::NodeStyleTable& styles,
    float viewportWidth,
    float viewportHeight) const {
    (void)viewportHeight;
    float y = 0.0f;
    auto root = buildNode(document.root(), styles, 0.0f, y, viewportWidth);
    if (root) {
        root->parent = nullptr;
        root->frame.width = viewportWidth;
        root->frame.height = std::max(root->frame.height, y);
    }
    return LayoutTree(std::move(root));
}

LayoutNode::Ptr LayoutEngine::buildNode(
    const document::Node::Ptr& node,
    const style::StyleResolver::NodeStyleTable& styles,
    float x,
    float& y,
    float availableWidth) const {
    if (!node) return nullptr;
    auto ln = std::make_unique<LayoutNode>();
    ln->source = node.get();
    auto sit = styles.find(node.get());
    if (sit != styles.end()) ln->style = sit->second;

    ln->box.margin = toPx(ln->style, "margin", 4.0f);
    ln->box.padding = toPx(ln->style, "padding", 4.0f);
    ln->box.border = toPx(ln->style, "border", 1.0f);

    const bool isInline = ln->style.find("display") != ln->style.end() && ln->style["display"] == "inline";
    const float width = toPx(ln->style, "width", isInline ? 160.0f : availableWidth - (ln->box.margin * 2.0f));
    float height = toPx(ln->style, "height", node->type() == document::NodeType::Text ? 18.0f : 24.0f);

    ln->frame.x = x + ln->box.margin;
    ln->frame.y = y + ln->box.margin;
    ln->frame.width = std::max(8.0f, width);
    ln->frame.height = std::max(8.0f, height);

    float childY = ln->frame.y + ln->box.padding + ln->box.border;
    float inlineX = ln->frame.x + ln->box.padding + ln->box.border;
    float maxBottom = ln->frame.y + ln->frame.height;

    for (const auto& ch : node->children()) {
        float yRef = childY;
        auto childLayout = buildNode(ch, styles, inlineX, yRef, ln->frame.width - (ln->box.padding * 2.0f));
        if (!childLayout) continue;
        const bool childInline =
            childLayout->style.find("display") != childLayout->style.end() &&
            childLayout->style["display"] == "inline";
        if (childInline) {
            childLayout->frame.x = inlineX;
            childLayout->frame.y = childY;
            inlineX += childLayout->frame.width + childLayout->box.margin;
        } else {
            childLayout->frame.x = ln->frame.x + ln->box.padding + ln->box.border;
            childLayout->frame.y = childY;
            childY = childLayout->frame.y + childLayout->frame.height + childLayout->box.margin;
            inlineX = ln->frame.x + ln->box.padding + ln->box.border;
        }
        maxBottom = std::max(maxBottom, childLayout->frame.y + childLayout->frame.height + childLayout->box.margin);
        childLayout->parent = ln.get();
        ln->children.push_back(std::move(childLayout));
    }

    ln->frame.height = std::max(ln->frame.height, maxBottom - ln->frame.y + ln->box.padding);
    y = isInline ? y : (ln->frame.y + ln->frame.height + ln->box.margin);
    return ln;
}

} // namespace fw::layout

