#pragma once

#include "fw/document/Document.hpp"
#include "fw/event/Event.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace fw::script {

class ScriptContext {
public:
    using BindingFn = std::function<void(document::Document&, event::InputEvent&)>;

    void bind(const std::string& name, BindingFn fn);
    void invoke(const std::string& name, document::Document& doc, event::InputEvent& ev) const;

private:
    std::unordered_map<std::string, BindingFn> bindings_;
};

} // namespace fw::script

