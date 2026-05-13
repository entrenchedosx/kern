#include "fw/script/ScriptContext.hpp"

namespace fw::script {

void ScriptContext::bind(const std::string& name, BindingFn fn) {
    bindings_[name] = std::move(fn);
}

void ScriptContext::invoke(const std::string& name, document::Document& doc, event::InputEvent& ev) const {
    auto it = bindings_.find(name);
    if (it == bindings_.end()) return;
    if (it->second) it->second(doc, ev);
}

} // namespace fw::script

