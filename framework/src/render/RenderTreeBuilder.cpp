#include "fw/render/RenderTree.hpp"

#include "fw/document/Node.hpp"

namespace fw::render {

namespace {
static Color parseColor(const std::string& value, Color fallback) {
    if (value.size() == 7 && value[0] == '#') {
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        return Color{
            static_cast<uint8_t>((hex(value[1]) << 4) | hex(value[2])),
            static_cast<uint8_t>((hex(value[3]) << 4) | hex(value[4])),
            static_cast<uint8_t>((hex(value[5]) << 4) | hex(value[6])),
            255
        };
    }
    return fallback;
}
} // namespace

RenderTree RenderTreeBuilder::build(const layout::LayoutTree& layout) const {
    RenderTree out;
    walk(layout.root(), out);
    return out;
}

void RenderTreeBuilder::walk(const layout::LayoutNode* node, RenderTree& out) const {
    if (!node || !node->source) return;
    const auto& style = node->style;

    Color bg = parseColor(style.count("background") ? style.at("background") : "#1f1f24", Color{31, 31, 36, 255});
    out.add(RenderObject{RenderObjectType::Rect, node->frame, bg, {}});

    std::string text;
    if (node->source->type() == document::NodeType::Text) text = node->source->text();
    else {
        auto it = node->source->attributes().find("text");
        if (it != node->source->attributes().end()) text = it->second;
    }
    if (!text.empty()) {
        Color fg = parseColor(style.count("color") ? style.at("color") : "#e6e6e6", Color{230, 230, 230, 255});
        auto trect = node->frame;
        trect.x += 6.0f;
        trect.y += 6.0f;
        out.add(RenderObject{RenderObjectType::Text, trect, fg, text});
    }

    for (const auto& child : node->children) walk(child.get(), out);
}

} // namespace fw::render

