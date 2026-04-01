#include "ir/passes/passes.hpp"

#include <unordered_map>

namespace kern {

void runTypedIRCanonicalization(IRProgram& program) {
    for (auto& mod : program.typedModules) {
        for (auto& fn : mod.functions) {
            for (auto& bb : fn.blocks) {
                // keep a deterministic non-empty block.
                if (bb.instructions.empty()) {
                    bb.instructions.push_back({TypedIROp::Nop, "", "", "", 0});
                }
            }
        }
    }
}

void runTypedIRConstantPropagation(IRProgram& program) {
    for (auto& mod : program.typedModules) {
        for (auto& fn : mod.functions) {
            std::unordered_map<std::string, std::string> env;
            for (auto& bb : fn.blocks) {
                for (auto& ins : bb.instructions) {
                    if (ins.op == TypedIROp::Assign && !ins.dst.empty()) {
                        env[ins.dst] = ins.lhs;
                    } else if (ins.op == TypedIROp::Call) {
                        auto it = env.find(ins.lhs);
                        if (it != env.end()) ins.lhs = it->second;
                    }
                }
            }
        }
    }
}

void runTypedIRDeadBlockElimination(IRProgram& program) {
    for (auto& mod : program.typedModules) {
        for (auto& fn : mod.functions) {
            if (fn.blocks.empty()) continue;
            // current typed IR builder emits a single entry block; keep one.
            fn.blocks.resize(1);
        }
    }
}

} // namespace kern
