/* *
 * Peephole pass: removes unreachable-by-edge NOPs and remaps jump / entry operands.
 * Does not remove NOPs that are jump targets, fall-through successors, or function entry PCs.
 */

#include "bytecode_peephole.hpp"

#include <cstddef>
#include <unordered_set>

namespace kern {

namespace {

bool getU64(const Instruction& inst, size_t* out) {
    if (!std::holds_alternative<size_t>(inst.operand)) return false;
    *out = std::get<size_t>(inst.operand);
    return true;
}

void setU64(Instruction& inst, size_t v) { inst.operand = v; }

/** PCs that may start execution (successors of any edge the verifier models). */
static void collectControlFlowTargets(const Bytecode& code, std::unordered_set<size_t>& targets) {
    const size_t n = code.size();
    targets.clear();
    targets.insert(0);
    for (size_t pc = 0; pc < n; ++pc) {
        const Instruction& inst = code[pc];
        auto add = [&](size_t t) {
            if (t < n) targets.insert(t);
        };
        switch (inst.op) {
            case Opcode::JMP:
            case Opcode::LOOP: {
                size_t t = 0;
                if (getU64(inst, &t)) add(t);
                break;
            }
            case Opcode::JMP_IF_FALSE:
            case Opcode::JMP_IF_TRUE: {
                add(pc + 1);
                size_t t = 0;
                if (getU64(inst, &t)) add(t);
                break;
            }
            case Opcode::CALL:
                add(pc + 1);
                break;
            case Opcode::RETURN:
            case Opcode::HALT:
            case Opcode::THROW:
            case Opcode::RETHROW:
                break;
            case Opcode::TRY_BEGIN: {
                add(pc + 1);
                size_t t = 0;
                if (getU64(inst, &t)) add(t);
                break;
            }
            default:
                add(pc + 1);
                break;
        }
    }
    for (size_t pc = 0; pc < n; ++pc) {
        const Instruction& inst = code[pc];
        if (inst.op == Opcode::BUILD_FUNC) {
            size_t e = 0;
            if (getU64(inst, &e) && e < n) targets.insert(e);
        } else if (inst.op == Opcode::BUILD_CLOSURE) {
            if (std::holds_alternative<std::pair<size_t, size_t>>(inst.operand)) {
                size_t e = std::get<std::pair<size_t, size_t>>(inst.operand).first;
                if (e < n) targets.insert(e);
            }
        }
    }
}

/** After deleting the instruction at delIdx (0-based), shift every code-index operand. */
static void remapOperandsAfterDeleteAt(Bytecode& code, size_t delIdx) {
    const size_t n = code.size();
    for (size_t pc = 0; pc < n; ++pc) {
        Instruction& inst = code[pc];
        switch (inst.op) {
            case Opcode::JMP:
            case Opcode::JMP_IF_FALSE:
            case Opcode::JMP_IF_TRUE:
            case Opcode::LOOP:
            case Opcode::TRY_BEGIN: {
                size_t t = 0;
                if (!getU64(inst, &t)) break;
                if (t > delIdx) --t;
                setU64(inst, t);
                break;
            }
            case Opcode::BUILD_FUNC: {
                size_t e = 0;
                if (!getU64(inst, &e)) break;
                if (e > delIdx) --e;
                setU64(inst, e);
                break;
            }
            case Opcode::BUILD_CLOSURE: {
                if (!std::holds_alternative<std::pair<size_t, size_t>>(inst.operand)) break;
                auto p = std::get<std::pair<size_t, size_t>>(inst.operand);
                if (p.first > delIdx) --p.first;
                inst.operand = p;
                break;
            }
            default:
                break;
        }
    }
}

} // namespace

void applyBytecodePeephole(Bytecode& code) {
    for (;;) {
        const size_t n = code.size();
        if (n == 0) return;

        std::unordered_set<size_t> targets;
        collectControlFlowTargets(code, targets);

        bool removed = false;
        for (size_t i = n; i-- > 0;) {
            if (code[i].op != Opcode::NOP) continue;
            if (targets.count(i)) continue;
            code.erase(code.begin() + static_cast<std::ptrdiff_t>(i));
            remapOperandsAfterDeleteAt(code, i);
            removed = true;
            break;
        }
        if (!removed) return;
    }
}

} // namespace kern
