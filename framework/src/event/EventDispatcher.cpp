#include "fw/event/EventDispatcher.hpp"

namespace fw::event {

void EventDispatcher::dispatch(layout::LayoutTree& layoutTree, InputEvent& ev) const {
    const layout::LayoutNode* target = nullptr;
    if (ev.type == EventType::MouseDown || ev.type == EventType::MouseUp || ev.type == EventType::MouseMove) {
        target = hitTest(layoutTree.root(), ev.x, ev.y);
    } else {
        target = layoutTree.root();
    }

    while (target && target->source) {
        target->source->dispatchEvent(ev);
        if (ev.handled) return;
        target = target->parent;
    }
}

const layout::LayoutNode* EventDispatcher::hitTest(const layout::LayoutNode* node, int x, int y) const {
    if (!node) return nullptr;
    const bool inside =
        x >= node->frame.x && y >= node->frame.y &&
        x <= (node->frame.x + node->frame.width) &&
        y <= (node->frame.y + node->frame.height);
    if (!inside) return nullptr;
    for (const auto& ch : node->children) {
        if (const auto* hit = hitTest(ch.get(), x, y)) return hit;
    }
    return node;
}

} // namespace fw::event

