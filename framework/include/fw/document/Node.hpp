#pragma once

#include "fw/event/Event.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fw::document {

enum class NodeType {
    Element,
    Text
};

class Node : public std::enable_shared_from_this<Node> {
public:
    using Ptr = std::shared_ptr<Node>;
    using Listener = std::function<void(event::InputEvent&, Node&)>;

    Node(NodeType type, std::string name);

    static Ptr createElement(std::string name);
    static Ptr createText(std::string text);

    NodeType type() const noexcept { return type_; }
    const std::string& name() const noexcept { return name_; }
    const std::string& text() const noexcept { return text_; }
    void setText(std::string value) { text_ = std::move(value); }

    void setAttribute(std::string key, std::string value);
    std::string attribute(const std::string& key) const;
    bool hasClass(const std::string& className) const;

    void appendChild(const Ptr& child);
    const std::vector<Ptr>& children() const noexcept { return children_; }
    Ptr parent() const noexcept { return parent_.lock(); }

    const std::unordered_map<std::string, std::string>& attributes() const noexcept { return attributes_; }

    void addEventListener(const std::string& type, Listener listener);
    void dispatchEvent(event::InputEvent& ev);

private:
    NodeType type_;
    std::string name_;
    std::string text_;
    std::weak_ptr<Node> parent_;
    std::vector<Ptr> children_;
    std::unordered_map<std::string, std::string> attributes_;
    std::unordered_map<event::EventType, std::vector<Listener>> typedListeners_;
    std::unordered_map<std::string, std::vector<Listener>> customListeners_;
};

} // namespace fw::document

