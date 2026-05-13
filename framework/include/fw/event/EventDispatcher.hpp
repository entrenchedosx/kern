#pragma once

#include "fw/event/Event.hpp"
#include "fw/layout/LayoutTree.hpp"

namespace fw::event {

class EventDispatcher {
public:
    void dispatch(layout::LayoutTree& layoutTree, InputEvent& ev) const;

private:
    const layout::LayoutNode* hitTest(const layout::LayoutNode* node, int x, int y) const;
};

} // namespace fw::event

