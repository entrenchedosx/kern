#include "fw/document/Node.hpp"
#include "fw/event/Event.hpp"

#include <cctype>
#include <optional>
#include <string_view>

namespace fw::document {

namespace {

static std::optional<event::EventType> parseEventType(std::string_view name) {
    if (name == "keydown") return event::EventType::KeyDown;
    if (name == "keyup") return event::EventType::KeyUp;
    if (name == "mousemove") return event::EventType::MouseMove;
    if (name == "mousedown") return event::EventType::MouseDown;
    if (name == "mouseup") return event::EventType::MouseUp;
    if (name == "quit") return event::EventType::Quit;
    return std::nullopt;
}

} // namespace

Node::Node(NodeType type, std::string name)
    : type_(type), name_(std::move(name)) {}

Node::Ptr Node::createElement(std::string name) {
    return std::make_shared<Node>(NodeType::Element, std::move(name));
}

Node::Ptr Node::createText(std::string text) {
    auto node = std::make_shared<Node>(NodeType::Text, "#text");
    node->text_ = std::move(text);
    return node;
}

void Node::setAttribute(std::string key, std::string value) {
    attributes_[std::move(key)] = std::move(value);
}

std::string Node::attribute(const std::string& key) const {
    auto it = attributes_.find(key);
    return it == attributes_.end() ? std::string{} : it->second;
}

bool Node::hasClass(const std::string& className) const {
    auto it = attributes_.find("class");
    if (it == attributes_.end()) return false;
    const std::string& classes = it->second;
    size_t i = 0;
    while (i < classes.size()) {
        while (i < classes.size() && std::isspace(static_cast<unsigned char>(classes[i]))) ++i;
        if (i >= classes.size()) break;
        size_t j = i;
        while (j < classes.size() && !std::isspace(static_cast<unsigned char>(classes[j]))) ++j;
        if ((j - i) == className.size() && classes.compare(i, className.size(), className) == 0) return true;
        i = j;
    }
    return false;
}

void Node::appendChild(const Ptr& child) {
    if (!child) return;
    child->parent_ = shared_from_this();
    children_.push_back(child);
}

void Node::addEventListener(const std::string& type, Listener listener) {
    auto parsed = parseEventType(type);
    if (parsed.has_value()) typedListeners_[*parsed].push_back(std::move(listener));
    else customListeners_[type].push_back(std::move(listener));
}

void Node::dispatchEvent(event::InputEvent& ev) {
    auto typed = typedListeners_.find(ev.type);
    if (typed != typedListeners_.end()) {
        for (auto& cb : typed->second) {
            if (cb) cb(ev, *this);
            if (ev.handled) return;
        }
    }
    if (!customListeners_.empty()) {
        auto custom = customListeners_.find(std::string(ev.typeNameView()));
        if (custom != customListeners_.end()) {
            for (auto& cb : custom->second) {
                if (cb) cb(ev, *this);
                if (ev.handled) return;
            }
        }
    }
}

} // namespace fw::document

