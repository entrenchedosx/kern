#pragma once

#include "fw/document/Node.hpp"

namespace fw::document {

class Document {
public:
    Document();
    explicit Document(Node::Ptr root);

    Node::Ptr root() const noexcept { return root_; }
    void setRoot(Node::Ptr root) { root_ = std::move(root); }

private:
    Node::Ptr root_;
};

} // namespace fw::document

