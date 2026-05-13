#pragma once

#include "fw/document/Node.hpp"
#include "fw/style/StyleResolver.hpp"

#include <memory>
#include <vector>

namespace fw::layout {

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

struct BoxModel {
    float margin{0.0f};
    float padding{0.0f};
    float border{0.0f};
};

class LayoutNode {
public:
    using Ptr = std::unique_ptr<LayoutNode>;

    document::Node* source{nullptr};
    LayoutNode* parent{nullptr};
    style::StyleMap style;
    Rect frame;
    BoxModel box;
    std::vector<Ptr> children;
};

class LayoutTree {
public:
    LayoutTree() = default;
    explicit LayoutTree(LayoutNode::Ptr root) : root_(std::move(root)) {}
    const LayoutNode* root() const noexcept { return root_.get(); }
    LayoutNode* root() noexcept { return root_.get(); }
    void setRoot(LayoutNode::Ptr root) { root_ = std::move(root); }

private:
    LayoutNode::Ptr root_;
};

} // namespace fw::layout

