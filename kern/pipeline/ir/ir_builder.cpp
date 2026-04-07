#include "ir/ir_builder.hpp"
#include "ir/typed_ir_builder.hpp"

namespace kern {

IRProgram buildIRFromResolvedGraph(const ResolveResult& resolved) {
    IRProgram p;
    for (const auto& key : resolved.topologicalOrder) {
        auto it = resolved.modules.find(key);
        if (it == resolved.modules.end()) continue;
        IRModule m;
        m.path = it->second.canonicalPath;
        m.source = it->second.source;
        m.dependencies = it->second.dependencies;
        p.modules.push_back(std::move(m));
    }
    buildTypedIRForProgram(p);
    return p;
}

} // namespace kern
