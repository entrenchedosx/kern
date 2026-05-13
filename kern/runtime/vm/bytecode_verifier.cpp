/* *
 * Bytecode verifier — operand shapes, jump targets, abstract stack depth (with try/catch stack).
 */

#include "bytecode_verifier.hpp"

#include <map>
#include <queue>
#include <sstream>
#include <tuple>
#include <utility>
#include <variant>

namespace kern {

namespace {

using TryStack = std::vector<size_t>;

static void fail(BytecodeVerifyResult& out, size_t pc, const Instruction& inst, VMErrorCode code, const std::string& msg) {
    out.ok = false;
    out.failPc = pc;
    out.line = inst.line;
    out.column = inst.column;
    out.code = code;
    out.message = msg;
}

static bool getU64(const Instruction& inst, size_t* out) {
    if (!std::holds_alternative<size_t>(inst.operand)) return false;
    *out = std::get<size_t>(inst.operand);
    return true;
}

static bool getI64(const Instruction& inst, int64_t* out) {
    if (!std::holds_alternative<int64_t>(inst.operand)) return false;
    *out = std::get<int64_t>(inst.operand);
    return true;
}

/** VM convention: 1 <= target <= codeSize; next instruction index = target (0-based) when target < codeSize. */
static bool jumpOk(size_t target, size_t codeSize) { return target >= 1 && target <= codeSize; }

/** Returns false if operand shape invalid; sets minDepth/delta for stack simulation when true. */
static bool stackMinAndDelta(const Instruction& inst, size_t codeSize, size_t stringPoolSize, int* minDepth, int* delta) {
    (void)codeSize;
    (void)stringPoolSize;
    *minDepth = 0;
    *delta = 0;
    switch (inst.op) {
        case Opcode::CONST_I64:
        case Opcode::CONST_F64:
        case Opcode::CONST_STR:
        case Opcode::CONST_TRUE:
        case Opcode::CONST_FALSE:
        case Opcode::CONST_NULL:
            *delta = 1;
            return true;
        case Opcode::LOAD:
        case Opcode::LOAD_GLOBAL:
            *delta = 1;
            return true;
        case Opcode::NEW_OBJECT:
            *delta = 1;
            return true;
        case Opcode::NOP:
        case Opcode::UNSAFE_BEGIN:
        case Opcode::UNSAFE_END:
        case Opcode::TRY_BEGIN:
        case Opcode::TRY_END:
            *delta = 0;
            return true;
        case Opcode::FOR_IN_ITER:
            *minDepth = 1;
            *delta = -1;
            return true;
        case Opcode::STORE:
        case Opcode::STORE_GLOBAL:
            *minDepth = 1;
            *delta = -1;
            return true;
        case Opcode::POP:
            *minDepth = 1;
            *delta = -1;
            return true;
        case Opcode::DUP:
            *minDepth = 1;
            *delta = 1;
            return true;
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::MUL:
        case Opcode::DIV:
        case Opcode::MOD:
        case Opcode::POW:
        case Opcode::EQ:
        case Opcode::NE:
        case Opcode::LT:
        case Opcode::LE:
        case Opcode::GT:
        case Opcode::GE:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::BIT_AND:
        case Opcode::BIT_OR:
        case Opcode::BIT_XOR:
        case Opcode::SHL:
        case Opcode::SHR:
            *minDepth = 2;
            *delta = -1;
            return true;
        case Opcode::NEG:
        case Opcode::NOT:
            *minDepth = 1;
            *delta = 0;
            return true;
        case Opcode::JMP:
        case Opcode::LOOP:
            *delta = 0;
            return true;
        case Opcode::JMP_IF_FALSE:
        case Opcode::JMP_IF_TRUE:
            *minDepth = 1;
            *delta = -1;
            return true;
        case Opcode::CALL: {
            size_t argc = 0;
            if (!getU64(inst, &argc)) return false;
            *minDepth = static_cast<int>(argc) + 1;
            *delta = -static_cast<int>(argc);
            return true;
        }
        case Opcode::DEFER: {
            size_t n = 0;
            if (!getU64(inst, &n)) return false;
            *minDepth = static_cast<int>(n);
            *delta = -static_cast<int>(n);
            return true;
        }
        case Opcode::RETURN:
        case Opcode::HALT:
            *delta = 0;
            return true;
        case Opcode::THROW:
        case Opcode::RETHROW:
            *delta = 0;
            return true;
        case Opcode::BUILD_FUNC:
        case Opcode::SET_FUNC_ARITY:
        case Opcode::SET_FUNC_PARAM_NAMES:
        case Opcode::SET_FUNC_NAME:
        case Opcode::SET_FUNC_GENERATOR:
        case Opcode::SET_FUNC_STRUCT:
            *delta = 0;
            if (inst.op == Opcode::BUILD_FUNC) *delta = 1;
            if (inst.op == Opcode::SET_FUNC_ARITY || inst.op == Opcode::SET_FUNC_PARAM_NAMES || inst.op == Opcode::SET_FUNC_NAME ||
                inst.op == Opcode::SET_FUNC_GENERATOR || inst.op == Opcode::SET_FUNC_STRUCT) {
                *minDepth = 1;
            }
            return true;
        case Opcode::BUILD_CLOSURE: {
            if (!std::holds_alternative<std::pair<size_t, size_t>>(inst.operand)) return false;
            size_t cap = std::get<std::pair<size_t, size_t>>(inst.operand).second;
            *minDepth = static_cast<int>(cap);
            *delta = -static_cast<int>(cap) + 1;
            return true;
        }
        case Opcode::YIELD:
            *minDepth = 0;
            *delta = -1;
            return true;
        case Opcode::BUILD_ARRAY: {
            size_t n = 0;
            if (!getU64(inst, &n)) return false;
            const size_t kMaxArraySize = 64 * 1024 * 1024;
            if (n > kMaxArraySize) return false;
            *minDepth = static_cast<int>(n);
            *delta = -static_cast<int>(n) + 1;
            return true;
        }
        case Opcode::SPREAD:
            *minDepth = 2;
            *delta = -1;
            return true;
        case Opcode::GET_FIELD:
            *minDepth = 1;
            *delta = 0;
            return true;
        case Opcode::SET_FIELD:
            *minDepth = 2;
            *delta = -1;
            return true;
        case Opcode::GET_INDEX:
            *minDepth = 2;
            *delta = -1;
            return true;
        case Opcode::ARRAY_LEN:
            *minDepth = 1;
            *delta = 0;
            return true;
        case Opcode::SET_INDEX:
            *minDepth = 3;
            *delta = -2;
            return true;
        case Opcode::SLICE:
            *minDepth = 4;
            *delta = -3;
            return true;
        case Opcode::PRINT:
            *minDepth = 1;
            *delta = 0;
            return true;
        case Opcode::BUILTIN:
            *delta = 1;
            return true;
        case Opcode::FOR_IN_NEXT:
            *delta = 1;
            return true;
        case Opcode::ALLOC:
            *minDepth = 1;
            *delta = 0;
            return true;
        case Opcode::FREE:
            *minDepth = 1;
            *delta = -1;
            return true;
        case Opcode::MEM_COPY:
            *minDepth = 3;
            *delta = -3;
            return true;
        case Opcode::NEW_INSTANCE:
        case Opcode::INVOKE_METHOD:
        case Opcode::LOAD_THIS:
            return false;
        case Opcode::BUILD_VEC3:
            *minDepth = 3;
            *delta = -2;  // pop 3, push 1
            return true;
        case Opcode::VEC3_GET_X:
        case Opcode::VEC3_GET_Y:
        case Opcode::VEC3_GET_Z:
            *minDepth = 1;
            *delta = 0;  // pop 1 Vec3, push 1 float
            return true;
        default:
            return false;
    }
}

static bool checkOperandOnly(const Instruction& inst, size_t pc, size_t n, size_t stringPoolSize, BytecodeVerifyResult& out) {
    (void)stringPoolSize;
    switch (inst.op) {
        case Opcode::CONST_I64:
            if (!std::holds_alternative<int64_t>(inst.operand)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "CONST_I64 expects int64 operand");
                return false;
            }
            return true;
        case Opcode::CONST_F64:
            if (!std::holds_alternative<double>(inst.operand)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "CONST_F64 expects float operand");
                return false;
            }
            return true;
        case Opcode::CONST_STR: {
            size_t idx = 0;
            if (!getU64(inst, &idx)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "CONST_STR expects string pool index");
                return false;
            }
            if (idx >= stringPoolSize) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "CONST_STR index out of range");
                return false;
            }
            return true;
        }
        case Opcode::LOAD: {
            int64_t slot = 0;
            if (!getI64(inst, &slot) || slot < 0) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "LOAD expects non-negative local slot");
                return false;
            }
            return true;
        }
        case Opcode::STORE: {
            int64_t slot = 0;
            if (!getI64(inst, &slot) || slot < 0) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "STORE expects non-negative local slot");
                return false;
            }
            return true;
        }
        case Opcode::LOAD_GLOBAL:
        case Opcode::STORE_GLOBAL:
        case Opcode::GET_FIELD:
        case Opcode::SET_FIELD:
        case Opcode::SET_FUNC_NAME:
        case Opcode::SET_FUNC_PARAM_NAMES: {
            size_t idx = 0;
            if (!getU64(inst, &idx)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "expected string pool index operand");
                return false;
            }
            if (idx >= stringPoolSize) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "string pool index out of range");
                return false;
            }
            return true;
        }
        case Opcode::JMP:
        case Opcode::JMP_IF_FALSE:
        case Opcode::JMP_IF_TRUE:
        case Opcode::LOOP: {
            size_t t = 0;
            if (!getU64(inst, &t)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "jump expects unsigned target");
                return false;
            }
            if (!jumpOk(t, n)) {
                fail(out, pc, inst, VMErrorCode::INVALID_JUMP_TARGET, "jump target out of bytecode bounds");
                return false;
            }
            return true;
        }
        case Opcode::TRY_BEGIN: {
            size_t t = 0;
            if (!getU64(inst, &t)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "TRY_BEGIN expects catch target");
                return false;
            }
            if (!jumpOk(t, n)) {
                fail(out, pc, inst, VMErrorCode::INVALID_JUMP_TARGET, "TRY_BEGIN catch target out of bounds");
                return false;
            }
            return true;
        }
        case Opcode::CALL:
        case Opcode::DEFER:
        case Opcode::SET_FUNC_ARITY:
        case Opcode::BUILD_ARRAY:
        case Opcode::BUILTIN:
            if (!std::holds_alternative<size_t>(inst.operand)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "expected unsigned operand");
                return false;
            }
            return true;
        case Opcode::BUILD_FUNC: {
            size_t entry = 0;
            if (!getU64(inst, &entry)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "BUILD_FUNC expects entry index");
                return false;
            }
            if (entry >= n) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "BUILD_FUNC entry out of range");
                return false;
            }
            return true;
        }
        case Opcode::BUILD_CLOSURE: {
            if (!std::holds_alternative<std::pair<size_t, size_t>>(inst.operand)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "BUILD_CLOSURE expects pair(entry, captureCount)");
                return false;
            }
            auto p = std::get<std::pair<size_t, size_t>>(inst.operand);
            if (p.first >= n) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "BUILD_CLOSURE entry out of range");
                return false;
            }
            return true;
        }
        case Opcode::FOR_IN_NEXT:
            if (!std::holds_alternative<size_t>(inst.operand) && !std::holds_alternative<std::pair<size_t, size_t>>(inst.operand)) {
                fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE, "FOR_IN_NEXT expects slot operand");
                return false;
            }
            return true;
        default:
            return true;
    }
}

struct Work {
    size_t pc = 0;
    int depth = 0;
    TryStack tryStack;
};

using DepthKey = std::tuple<size_t, TryStack>;

} // namespace

bool verifyBytecode(const Bytecode& code, size_t stringPoolSize, size_t valuePoolSize, BytecodeVerifyResult& out) {
    (void)valuePoolSize;
    out.ok = true;
    out.message.clear();
    out.failPc = 0;
    out.code = VMErrorCode::NONE;
    const size_t n = code.size();
    if (n == 0) return true;

    // Collect function entry PCs for isolated analysis (script calls not modeled; still useful for inner bodies).
    std::vector<size_t> extraEntries;
    extraEntries.reserve(8);
    for (size_t i = 0; i < n; ++i) {
        const Instruction& inst = code[i];
        if (inst.op == Opcode::BUILD_FUNC) {
            size_t e = 0;
            if (getU64(inst, &e)) extraEntries.push_back(e);
        } else if (inst.op == Opcode::BUILD_CLOSURE) {
            if (std::holds_alternative<std::pair<size_t, size_t>>(inst.operand))
                extraEntries.push_back(std::get<std::pair<size_t, size_t>>(inst.operand).first);
        }
    }

    // 1) Operand / jump shape checks for every instruction
    for (size_t pc = 0; pc < n; ++pc) {
        const Instruction& inst = code[pc];
        if (!checkOperandOnly(inst, pc, n, stringPoolSize, out)) return false;
        int minD = 0, delta = 0;
        if (!stackMinAndDelta(inst, n, stringPoolSize, &minD, &delta)) {
            fail(out, pc, inst, VMErrorCode::INVALID_BYTECODE,
                 std::string("Unsupported or invalid opcode for verification: ") + opcodeName(inst.op));
            return false;
        }
    }

    // 2) Abstract stack + try-stack dataflow
    std::map<DepthKey, int> depthAt;
    std::queue<Work> q;
    auto startState = [&](size_t pc0) {
        Work w;
        w.pc = pc0;
        w.depth = 0;
        q.push(w);
    };
    startState(0);
    for (size_t e : extraEntries) startState(e);

    while (!q.empty()) {
        Work w = q.front();
        q.pop();
        if (w.pc >= n) continue;

        const Instruction& inst = code[w.pc];
        int minD = 0, delta = 0;
        if (!stackMinAndDelta(inst, n, stringPoolSize, &minD, &delta)) continue;
        if (w.depth < minD) {
            std::ostringstream os;
            os << "Stack underflow before " << opcodeName(inst.op) << " (need " << minD << ", have " << w.depth << ")";
            fail(out, w.pc, inst, VMErrorCode::STACK_UNDERFLOW, os.str());
            return false;
        }
        int newDepth = w.depth + delta;
        if (newDepth < 0) {
            fail(out, w.pc, inst, VMErrorCode::STACK_UNDERFLOW, "Stack depth would be negative after instruction");
            return false;
        }

        TryStack trySt = w.tryStack;

        auto recordAndEnqueue = [&](size_t nextPc, int d, TryStack ts) -> bool {
            if (nextPc >= n) return true;
            DepthKey key{nextPc, ts};
            auto it = depthAt.find(key);
            if (it != depthAt.end()) {
                if (it->second != d) {
                    const Instruction& at = code[nextPc];
                    fail(out, nextPc, at, VMErrorCode::BYTECODE_VERIFY_STACK,
                         "Conflicting stack depth at same program point");
                    return false;
                }
            } else {
                depthAt[key] = d;
                Work nw;
                nw.pc = nextPc;
                nw.depth = d;
                nw.tryStack = std::move(ts);
                q.push(nw);
            }
            return true;
        };

        // TRY_BEGIN: push catch target onto try stack (operand already validated)
        if (inst.op == Opcode::TRY_BEGIN) {
            size_t t = std::get<size_t>(inst.operand);
            trySt.push_back(t);
        } else if (inst.op == Opcode::TRY_END) {
            if (!trySt.empty()) trySt.pop_back();
        }

        switch (inst.op) {
            case Opcode::JMP:
            case Opcode::LOOP: {
                size_t t = std::get<size_t>(inst.operand);
                if (t < n && !recordAndEnqueue(t, newDepth, trySt)) return false;
                break;
            }
            case Opcode::JMP_IF_FALSE:
            case Opcode::JMP_IF_TRUE: {
                size_t t = std::get<size_t>(inst.operand);
                if (w.pc + 1 < n && !recordAndEnqueue(w.pc + 1, newDepth, trySt)) return false;
                if (t < n && !recordAndEnqueue(t, newDepth, trySt)) return false;
                break;
            }
            case Opcode::CALL:
                if (w.pc + 1 < n && !recordAndEnqueue(w.pc + 1, newDepth, trySt)) return false;
                break;
            case Opcode::RETURN:
            case Opcode::HALT:
                break;
            case Opcode::THROW:
            case Opcode::RETHROW: {
                if (trySt.empty()) break;
                size_t catchTarget = trySt.back();
                TryStack ts2 = trySt;
                ts2.pop_back();
                if (catchTarget < n && !recordAndEnqueue(catchTarget, newDepth, std::move(ts2))) return false;
                break;
            }
            default:
                if (w.pc + 1 < n && !recordAndEnqueue(w.pc + 1, newDepth, trySt)) return false;
                break;
        }
    }

    return true;
}

} // namespace kern
