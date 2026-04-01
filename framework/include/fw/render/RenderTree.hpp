#pragma once

#include "fw/layout/LayoutTree.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fw::render {

struct Color {
    uint8_t r{32};
    uint8_t g{32};
    uint8_t b{32};
    uint8_t a{255};
};

enum class RenderObjectType {
    Rect,
    Text
};

struct RenderObject {
    RenderObjectType type{RenderObjectType::Rect};
    layout::Rect frame;
    Color color;
    std::string text;
};

class RenderTree {
public:
    void add(RenderObject obj) { objects_.push_back(std::move(obj)); }
    const std::vector<RenderObject>& objects() const noexcept { return objects_; }
    void clear() { objects_.clear(); }

private:
    std::vector<RenderObject> objects_;
};

class RenderTreeBuilder {
public:
    RenderTree build(const layout::LayoutTree& layout) const;

private:
    void walk(const layout::LayoutNode* node, RenderTree& out) const;
};

} // namespace fw::render

