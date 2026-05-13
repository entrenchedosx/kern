#include "fw/document/Document.hpp"

namespace fw::document {

Document::Document()
    : root_(Node::createElement("root")) {}

Document::Document(Node::Ptr root)
    : root_(std::move(root)) {
    if (!root_) root_ = Node::createElement("root");
}

} // namespace fw::document

