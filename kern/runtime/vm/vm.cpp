/* *
 * kern VM Implementation
 */

#include "vm.hpp"
#include "bytecode_verifier.hpp"
#include "errors/vm_error_registry.hpp"
#include "platform/env_compat.hpp"
#include <sstream>
#include <string>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace kern {

/* * map subscript: string key, or int/float coerced to decimal string (for handles-as-keys).*/
static bool mapIndexToKey(const ValuePtr& index, std::string& out) {
    if (!index) return false;
    switch (index->type) {
        case Value::Type::STRING:
            out = std::get<std::string>(index->data);
            return true;
        case Value::Type::INT:
            out = std::to_string(std::get<int64_t>(index->data));
            return true;
        case Value::Type::FLOAT:
            out = std::to_string(std::get<double>(index->data));
            return true;
        default:
            return false;
    }
}

/* * canonicalize null shared_ptr to a NIL Value (stack and stored locals must never hold nullptr).*/
static ValuePtr ensureNonNull(ValuePtr v) {
    return v ? std::move(v) : std::make_shared<Value>(Value::nil());
}

static void normalizeValuePtrVector(std::vector<ValuePtr>& v) {
    for (auto& x : v) {
        if (!x) x = std::make_shared<Value>(Value::nil());
    }
}

struct ThrownErrorInfo {
    int category = 1;
    int code = static_cast<int>(VMErrorCode::NONE);
    std::string message = "exception";
    int line = 0;
    int column = 0;
    int lineEnd = 0;
    int columnEnd = 0;
};

static int mapIntField(const std::unordered_map<std::string, ValuePtr>& m, const char* key, int fallback = 0) {
    auto it = m.find(key);
    if (it == m.end() || !it->second) return fallback;
    if (it->second->type == Value::Type::INT) return static_cast<int>(std::get<int64_t>(it->second->data));
    if (it->second->type == Value::Type::FLOAT) return static_cast<int>(std::get<double>(it->second->data));
    return fallback;
}

static ThrownErrorInfo classifyThrownError(const ValuePtr& v) {
    ThrownErrorInfo out;
    if (!v) return out;
    out.message = v->toString();
    if (v->type != Value::Type::MAP) return out;
    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(v->data);
    out.line = mapIntField(m, "line", 0);
    out.column = mapIntField(m, "column", 0);
    out.lineEnd = mapIntField(m, "lineEnd", out.line);
    out.columnEnd = mapIntField(m, "columnEnd", 0);
    auto mit = m.find("message");
    if (mit != m.end() && mit->second && mit->second->type == Value::Type::STRING)
        out.message = std::get<std::string>(mit->second->data);
    auto cit = m.find("code");
    if (cit == m.end() || !cit->second || cit->second->type != Value::Type::STRING)
        return out;
    VMErrorCode code = vmErrorCodeFromToken(std::get<std::string>(cit->second->data));
    out.code = static_cast<int>(code);
    out.category = vmCategoryFromCode(code, out.category);
    return out;
}

VM::VM() : ip_(0), vmTraceEnabled_(false) {
    // Pre-reserve vectors to avoid allocations in hot path
    stack_.reserve(512);
    callStack_.reserve(128);
    tryStack_.reserve(64);
    exceptionStack_.reserve(64);
    callFrames_.reserve(256);
    frameLocals_.reserve(64);
    deferStack_.reserve(32);
    
    initBuiltins();
    
// Debug-only: check environment for VM tracing
#if defined(KERN_DEBUG) && defined(KERN_DEBUG_VM_TRACE)
    #ifdef _MSC_VER
        char* buf = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&buf, &sz, "KERN_VM_TRACE") == 0 && buf) {
            vmTraceEnabled_ = buf[0] != '\0' && buf[0] != '0';
            std::free(buf);
        }
    #else
        const char* tr = kernGetEnv("KERN_VM_TRACE");
        vmTraceEnabled_ = tr && tr[0] != '\0' && tr[0] != '0';
    #endif
#endif
}

void VM::setBytecode(Bytecode code) {
    code_ = std::move(code);
    ip_ = 0;
    unsafeDepth_ = 0;
    exceptionStack_.clear();  // Clear exception frames
    tryStack_.clear();        // Clear try stack
    entryScriptCache_.reset();
    activeSourcePath_.clear();
    breakpoints_.clear();
}

void VM::verifyBytecodeOrThrow(const Bytecode& bc, size_t strPool, size_t valPool) {
    BytecodeVerifyResult vr;
    if (!verifyBytecode(bc, strPool, valPool, vr)) {
        const Instruction& at = !bc.empty() && vr.failPc < bc.size() ? bc[vr.failPc] : Instruction(Opcode::NOP);
        throw VMError(vr.message.empty() ? "Bytecode verification failed" : vr.message, at.line, at.column, 1,
                      static_cast<int>(vr.code));
    }
}

void VM::setInstructionPointer(size_t ip) {
    if (ip <= code_.size()) ip_ = ip;
}

void VM::addBreakpoint(size_t pc) { breakpoints_.insert(pc); }

void VM::removeBreakpoint(size_t pc) { breakpoints_.erase(pc); }

void VM::clearBreakpoints() { breakpoints_.clear(); }

bool VM::runNextInstruction() {
    if (code_.empty()) return false;
    verifyBytecodeOrThrow(code_, stringConstants_.size(), valueConstants_.size());
    if (ip_ >= code_.size()) return false;
    runInstruction(code_[ip_]);
    ip_++;
    return ip_ < code_.size();
}

void VM::runUntilBreakpoint() {
    if (code_.empty()) return;
    verifyBytecodeOrThrow(code_, stringConstants_.size(), valueConstants_.size());
    resetCycleCount();
    
    if (stack_.capacity() < 512) stack_.reserve(512);
    
    while (ip_ < code_.size()) {
        if (breakpoints_.count(ip_)) return;
        const Instruction& inst = code_[ip_];
        runInstruction(inst);
        
#if defined(KERN_DEBUG) && defined(KERN_DEBUG_VM_TRACE)
        if (vmTraceEnabled_ && cycleCount_ <= 500000u) {
            std::cerr << "[vm] op=" << static_cast<int>(inst.op) 
                      << " line=" << inst.line << " col=" << inst.column
                      << " sp=" << stack_.size() << "\n";
        }
#endif
        
        ip_++;
        if (scriptExitCode_ >= 0) break;
    }
}

void VM::setStringConstants(std::vector<std::string> constants) { stringConstants_ = std::move(constants); }

void VM::setValueConstants(std::vector<Value> constants) { valueConstants_ = std::move(constants); }

void VM::registerBuiltin(size_t index, BuiltinFn fn) {
    builtins_[index] = std::move(fn);
    if (index < 256u) {
        if (builtinsVec_.size() <= index) builtinsVec_.resize(index + 1);
        builtinsVec_[index] = builtins_[index];
    }
}

bool VM::builtinSlotFilled(size_t index) const {
    if (index < builtinsVec_.size() && builtinsVec_[index])
        return true;
    auto it = builtins_.find(index);
    return it != builtins_.end() && static_cast<bool>(it->second);
}

void VM::setGlobal(const std::string& name, ValuePtr value) {
    globals_[name] = ensureNonNull(std::move(value));
}

ValuePtr VM::getGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    return it != globals_.end() ? it->second : nullptr;
}

std::unordered_map<std::string, ValuePtr> VM::getGlobalsSnapshot() const {
    auto m = globals_;
    for (auto& kv : m) {
        if (!kv.second) kv.second = std::make_shared<Value>(Value::nil());
    }
    return m;
}

ValuePtr VM::popStack() {
    if (stack_.empty())
        throw VMError("Stack underflow", 0, 0, 1, static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
    ValuePtr v = stack_.back();
    stack_.pop_back();
    return ensureNonNull(std::move(v));
}

ValuePtr VM::getResult() {
    if (stack_.empty()) return std::make_shared<Value>(Value::nil());
    ValuePtr& top = stack_.back();
    if (!top) top = std::make_shared<Value>(Value::nil());
    return top;
}

std::string VM::getOperandStr(const Instruction& inst) {
    if (!std::holds_alternative<size_t>(inst.operand)) {
        throw VMError("Invalid bytecode operand: expected string constant index", inst.line, inst.column, 1,
                      static_cast<int>(VMErrorCode::INVALID_BYTECODE));
    }
    size_t idx = std::get<size_t>(inst.operand);
    if (idx >= stringConstants_.size()) {
        throw VMError("Invalid bytecode operand: string constant index out of range", inst.line, inst.column, 1,
                      static_cast<int>(VMErrorCode::INVALID_BYTECODE));
    }
    return stringConstants_[idx];
}

size_t VM::getOperandU64(const Instruction& inst) {
    if (!std::holds_alternative<size_t>(inst.operand)) {
        throw VMError("Invalid bytecode operand: expected unsigned operand", inst.line, inst.column, 1,
                      static_cast<int>(VMErrorCode::INVALID_BYTECODE));
    }
    return std::get<size_t>(inst.operand);
}

void VM::push(ValuePtr v) {
    if (stack_.size() >= kMaxStackSize) {
        throw VMError("Stack overflow: exceeded maximum stack size", 0, 0, 1,
                      static_cast<int>(VMErrorCode::STACK_OVERFLOW));
    }
    stack_.push_back(ensureNonNull(std::move(v)));
}

ValuePtr VM::peek() {
    if (stack_.empty())
        throw VMError("Stack underflow", 0, 0, 1, static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
    ValuePtr& top = stack_.back();
    if (!top) top = std::make_shared<Value>(Value::nil());
    return top;
}

static double toDouble(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return static_cast<double>(std::get<int64_t>(v->data));
    if (v->type == Value::Type::FLOAT) return std::get<double>(v->data);
    return 0;
}

static int64_t toInt(ValuePtr v) {
    if (!v) return 0;
    if (v->type == Value::Type::INT) return std::get<int64_t>(v->data);
    if (v->type == Value::Type::FLOAT) {
        double d = std::get<double>(v->data);
        if (!std::isfinite(d)) return 0;
        constexpr double kMax = static_cast<double>(std::numeric_limits<int64_t>::max());
        constexpr double kMin = static_cast<double>(std::numeric_limits<int64_t>::min());
        if (d >= kMax) return std::numeric_limits<int64_t>::max();
        if (d <= kMin) return std::numeric_limits<int64_t>::min();
        return static_cast<int64_t>(d);
    }
    return 0;
}

static void* toPtr(ValuePtr v) {
    if (!v || v->type != Value::Type::PTR) return nullptr;
    return std::get<void*>(v->data);
}

void VM::runInstruction(const Instruction& inst) {
    ++cycleCount_;
    if (stepLimit_ != 0 && cycleCount_ > stepLimit_)
        throw VMError("Step limit exceeded", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::STEP_LIMIT_EXCEEDED));
    auto requireNumeric = [&](const ValuePtr& v, const char* opName) {
        if (!v || (v->type != Value::Type::INT && v->type != Value::Type::FLOAT)) {
            throw VMError(std::string("Invalid operation: ") + opName + " expects numeric operands",
                          inst.line, inst.column, 2, static_cast<int>(VMErrorCode::INVALID_OPERATION));
        }
    };
    auto requireInteger = [&](const ValuePtr& v, const char* opName) {
        if (!v || v->type != Value::Type::INT) {
            throw VMError(std::string("Invalid operation: ") + opName + " expects integer operands",
                          inst.line, inst.column, 2, static_cast<int>(VMErrorCode::INVALID_OPERATION));
        }
    };
    switch (inst.op) {
        case Opcode::CONST_I64:
            if (!std::holds_alternative<int64_t>(inst.operand))
                throw VMError("Invalid bytecode operand: CONST_I64 expects int64", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            push(std::make_shared<Value>(Value::fromInt(std::get<int64_t>(inst.operand))));
            break;
        case Opcode::CONST_F64:
            if (!std::holds_alternative<double>(inst.operand))
                throw VMError("Invalid bytecode operand: CONST_F64 expects float", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            push(std::make_shared<Value>(Value::fromFloat(std::get<double>(inst.operand))));
            break;
        case Opcode::CONST_STR: {
            if (!std::holds_alternative<size_t>(inst.operand))
                throw VMError("Invalid bytecode operand: CONST_STR expects string constant index", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            size_t idx = std::get<size_t>(inst.operand);
            if (idx >= stringConstants_.size())
                throw VMError("Invalid bytecode operand: CONST_STR index out of range", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            push(std::make_shared<Value>(Value::fromString(stringConstants_[idx])));
            break;
        }
        case Opcode::CONST_TRUE:
            push(std::make_shared<Value>(Value::fromBool(true)));
            break;
        case Opcode::CONST_FALSE:
            push(std::make_shared<Value>(Value::fromBool(false)));
            break;
        case Opcode::CONST_NULL:
            push(std::make_shared<Value>(Value::nil()));
            break;
        case Opcode::LOAD: {
            if (!std::holds_alternative<int64_t>(inst.operand))
                throw VMError("Invalid bytecode operand: LOAD expects local slot index", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            int64_t rawSlot = std::get<int64_t>(inst.operand);
            if (rawSlot < 0)
                throw VMError("Invalid bytecode operand: negative local slot in LOAD", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            size_t slot = static_cast<size_t>(rawSlot);
            if (slot < locals_.size()) {
                push(locals_[slot]);
            } else {
                throw VMError("Invalid bytecode operand: local slot out of range in LOAD", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            }
            break;
        }
        case Opcode::STORE: {
            if (!std::holds_alternative<int64_t>(inst.operand))
                throw VMError("Invalid bytecode operand: STORE expects local slot index", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            int64_t rawSlot = std::get<int64_t>(inst.operand);
            if (rawSlot < 0)
                throw VMError("Invalid bytecode operand: negative local slot in STORE", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            size_t slot = static_cast<size_t>(rawSlot);
            while (locals_.size() <= slot) locals_.push_back(std::make_shared<Value>(Value::nil()));
            locals_[slot] = popStack();
            break;
        }
        case Opcode::LOAD_GLOBAL: {
            std::string name = getOperandStr(inst);
            auto it = globals_.find(name);
            ValuePtr v = it != globals_.end() ? ensureNonNull(it->second) : std::make_shared<Value>(Value::nil());
            push(std::move(v));
            break;
        }
        case Opcode::STORE_GLOBAL: {
            std::string name = getOperandStr(inst);
            globals_[name] = popStack();
            break;
        }
        case Opcode::POP:
            popStack();
            break;
        case Opcode::DUP: {
            ValuePtr v = peek();
            push(v);  // duplicate reference so mutations (e.g. SET_FIELD) are visible
            break;
        }
        case Opcode::ADD: {
            ValuePtr b = popStack(), a = popStack();
            if (a->type == Value::Type::STRING || b->type == Value::Type::STRING)
                push(std::make_shared<Value>(Value::fromString(a->toString() + b->toString())));
            else if (a->type == Value::Type::PTR && b->type == Value::Type::INT) {
                void* p = toPtr(a);
                if (!p) throw VMError("Null pointer arithmetic", inst.line, inst.column, 2);
                int64_t off = toInt(b);
                push(std::make_shared<Value>(Value::fromPtr(static_cast<char*>(p) + off)));
            } else if (a->type == Value::Type::INT && b->type == Value::Type::PTR) {
                int64_t off = toInt(a);
                void* p = toPtr(b);
                if (!p) throw VMError("Null pointer arithmetic", inst.line, inst.column, 2);
                push(std::make_shared<Value>(Value::fromPtr(static_cast<char*>(p) + off)));
            } else if (a->type == Value::Type::FLOAT || b->type == Value::Type::FLOAT)
                push(std::make_shared<Value>(Value::fromFloat(toDouble(a) + toDouble(b))));
            else if (a->type == Value::Type::INT && b->type == Value::Type::INT)
                push(std::make_shared<Value>(Value::fromInt(toInt(a) + toInt(b))));
            else
                throw VMError("Invalid operation: ADD expects numeric, string, or ptr+int operands",
                              inst.line, inst.column, 2, static_cast<int>(VMErrorCode::INVALID_OPERATION));
            break;
        }
        case Opcode::SUB: {
            ValuePtr b = popStack(), a = popStack();
            if (a->type == Value::Type::PTR && b->type == Value::Type::INT) {
                void* p = toPtr(a);
                if (!p) throw VMError("Null pointer arithmetic", inst.line, inst.column, 2);
                int64_t off = toInt(b);
                push(std::make_shared<Value>(Value::fromPtr(static_cast<char*>(p) - off)));
            } else if (a->type == Value::Type::PTR && b->type == Value::Type::PTR) {
                char* pa = static_cast<char*>(toPtr(a));
                char* pb = static_cast<char*>(toPtr(b));
                if (!pa || !pb) throw VMError("Null pointer arithmetic", inst.line, inst.column, 2);
                push(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(pa - pb))));
            } else if ((a->type == Value::Type::INT || a->type == Value::Type::FLOAT) &&
                       (b->type == Value::Type::INT || b->type == Value::Type::FLOAT))
                push(std::make_shared<Value>(Value::fromFloat(toDouble(a) - toDouble(b))));
            else
                throw VMError("Invalid operation: SUB expects numeric, ptr-int, or ptr-ptr operands",
                              inst.line, inst.column, 2, static_cast<int>(VMErrorCode::INVALID_OPERATION));
            break;
        }
        case Opcode::MUL: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "MUL");
            requireNumeric(b, "MUL");
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) * toDouble(b))));
            break;
        }
        case Opcode::DIV: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "DIV");
            requireNumeric(b, "DIV");
            double den = toDouble(b);
            if (den == 0)
                throw VMError("Division by zero", inst.line, inst.column, 4, static_cast<int>(VMErrorCode::DIVISION_BY_ZERO));
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) / den)));
            break;
        }
        case Opcode::MOD: {
            ValuePtr b = popStack(), a = popStack();
            requireInteger(a, "MOD");
            requireInteger(b, "MOD");
            int64_t den = toInt(b);
            if (den == 0)
                throw VMError("Division by zero", inst.line, inst.column, 4, static_cast<int>(VMErrorCode::DIVISION_BY_ZERO));
            push(std::make_shared<Value>(Value::fromInt(toInt(a) % den)));
            break;
        }
        case Opcode::POW: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "POW");
            requireNumeric(b, "POW");
            push(std::make_shared<Value>(Value::fromFloat(std::pow(toDouble(a), toDouble(b)))));
            break;
        }
        case Opcode::NEG: {
            ValuePtr v = popStack();
            requireNumeric(v, "NEG");
            if (v->type == Value::Type::FLOAT) push(std::make_shared<Value>(Value::fromFloat(-toDouble(v))));
            else push(std::make_shared<Value>(Value::fromInt(-toInt(v))));
            break;
        }
        case Opcode::EQ: {
            ValuePtr b = popStack(), a = popStack();
            bool eq = (a && b) ? a->equals(*b) : (a.get() == b.get());
            push(std::make_shared<Value>(Value::fromBool(eq)));
            break;
        }
        case Opcode::NE: {
            ValuePtr b = popStack(), a = popStack();
            bool eq = (a && b) ? a->equals(*b) : (a.get() == b.get());
            push(std::make_shared<Value>(Value::fromBool(!eq)));
            break;
        }
        case Opcode::LT: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "LT");
            requireNumeric(b, "LT");
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) < toDouble(b))));
            break;
        }
        case Opcode::LE: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "LE");
            requireNumeric(b, "LE");
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) <= toDouble(b))));
            break;
        }
        case Opcode::GT: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "GT");
            requireNumeric(b, "GT");
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) > toDouble(b))));
            break;
        }
        case Opcode::GE: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "GE");
            requireNumeric(b, "GE");
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) >= toDouble(b))));
            break;
        }
        case Opcode::AND: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool((a && a->isTruthy()) && (b && b->isTruthy()))));
            break;
        }
        case Opcode::OR: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool((a && a->isTruthy()) || (b && b->isTruthy()))));
            break;
        }
        case Opcode::NOT: {
            ValuePtr v = popStack();
            push(std::make_shared<Value>(Value::fromBool(!(v && v->isTruthy()))));
            break;
        }
        case Opcode::BIT_AND: {
            ValuePtr b = popStack(), a = popStack();
            requireInteger(a, "BIT_AND");
            requireInteger(b, "BIT_AND");
            push(std::make_shared<Value>(Value::fromInt(toInt(a) & toInt(b))));
            break;
        }
        case Opcode::BIT_OR: {
            ValuePtr b = popStack(), a = popStack();
            requireInteger(a, "BIT_OR");
            requireInteger(b, "BIT_OR");
            push(std::make_shared<Value>(Value::fromInt(toInt(a) | toInt(b))));
            break;
        }
        case Opcode::BIT_XOR: {
            ValuePtr b = popStack(), a = popStack();
            requireInteger(a, "BIT_XOR");
            requireInteger(b, "BIT_XOR");
            push(std::make_shared<Value>(Value::fromInt(toInt(a) ^ toInt(b))));
            break;
        }
        case Opcode::SHL: {
            ValuePtr b = popStack(), a = popStack();
            requireInteger(a, "SHL");
            requireInteger(b, "SHL");
            int64_t sh = toInt(b) & 63;
            push(std::make_shared<Value>(Value::fromInt(toInt(a) << sh)));
            break;
        }
        case Opcode::SHR: {
            ValuePtr b = popStack(), a = popStack();
            requireInteger(a, "SHR");
            requireInteger(b, "SHR");
            int64_t sh = toInt(b) & 63;
            push(std::make_shared<Value>(Value::fromInt(toInt(a) >> sh)));
            break;
        }
        case Opcode::JMP: {
            size_t target = getOperandU64(inst);
            if (target == 0 || target > code_.size())
                throw VMError("Invalid jump target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            ip_ = target - 1;
            break;
        }
        case Opcode::JMP_IF_FALSE: {
            ValuePtr v = popStack();
            if (!v->isTruthy()) {
                size_t target = getOperandU64(inst);
                if (target == 0 || target > code_.size())
                    throw VMError("Invalid jump target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
                ip_ = target - 1;
            }
            break;
        }
        case Opcode::JMP_IF_TRUE: {
            ValuePtr v = popStack();
            if (v->isTruthy()) {
                size_t target = getOperandU64(inst);
                if (target == 0 || target > code_.size())
                    throw VMError("Invalid jump target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
                ip_ = target - 1;
            }
            break;
        }
        case Opcode::CALL: {
            size_t argc = getOperandU64(inst);
            if (stack_.size() < argc + 1)
                throw VMError("Stack underflow in call (not enough arguments)", inst.line, inst.column, 5);
            std::vector<ValuePtr> args;
            for (size_t i = 0; i < argc; ++i) args.push_back(popStack());
            std::reverse(args.begin(), args.end());
            ValuePtr callee = popStack();
            // callee is never null (popStack canonicalizes); non-FUNCTION falls through below.
            if (callee->type == Value::Type::FUNCTION) {
                auto& fn = std::get<FunctionPtr>(callee->data);
                if (fn->isBuiltin) {
                    BuiltinFn* fast = (fn->builtinIndex < builtinsVec_.size()) ? &builtinsVec_[fn->builtinIndex] : nullptr;
                    if (fast && *fast)
                        push(std::make_shared<Value>((*fast)(this, args)));
                    else {
                        auto it = builtins_.find(fn->builtinIndex);
                        if (it != builtins_.end()) {
                            push(std::make_shared<Value>(it->second(this, args)));
                        } else {
                            throw VMError("Invalid builtin index", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_BYTECODE));
                        }
                    }
                } else if (fn->isGenerator) {
                    auto go = std::make_shared<GeneratorObject>();
                    go->fn = fn;
                    go->ip = fn->entryPoint;
                    go->locals = std::move(args);
                    while (go->locals.size() < fn->arity)
                        go->locals.push_back(std::make_shared<Value>(Value::nil()));
                    for (const auto& c : fn->captures)
                        go->locals.push_back(ensureNonNull(c ? c : std::make_shared<Value>(Value::nil())));
                    go->exhausted = false;
                    push(std::make_shared<Value>(Value::fromGenerator(std::move(go))));
                } else {
                    if (maxCallDepth_ > 0 && callFrames_.size() >= maxCallDepth_) {
                        throw VMError("Maximum call depth exceeded (" + std::to_string(maxCallDepth_) + ")", inst.line, inst.column, 1);
                    }
                    // tail-call check must use the caller's bytecode/ip_. Switching code_ for fn->script first
                    // would read the wrong instruction (cross-script calls: e.g. callback from module -> main).
                    bool tailCall = (ip_ + 1 < code_.size() && code_[ip_ + 1].op == Opcode::RETURN)
                        && !deferStack_.empty() && deferStack_.back().empty();
                    // When maxCallDepth_ is enforced, do not reuse frames: tail recursion would bypass the limit.
                    if (maxCallDepth_ > 0) tailCall = false;
                    const std::string callerPath = activeSourcePath_;
                    // if function was defined in an imported script, switch to its bytecode for the call
                    if (fn->script) {
                        codeFrameStack_.push_back(
                            std::make_tuple(std::move(code_), std::move(stringConstants_), std::move(valueConstants_), callerPath));
                        code_ = fn->script->code;
                        stringConstants_ = fn->script->stringConstants;
                        valueConstants_ = fn->script->valueConstants;
                        activeSourcePath_ = fn->script->sourcePath;
                    }
                    // tail-call reuse is invalid without an outer frame (would callStack_.back() UB).
                    auto appendCaptures = [&] {
                        for (const auto& c : fn->captures)
                            locals_.push_back(ensureNonNull(c ? c : std::make_shared<Value>(Value::nil())));
                    };
                    if (tailCall && !callStack_.empty()) {
                        callStack_.back() = {fn->name.empty() ? "<anonymous>" : fn->name, callerPath, inst.line, inst.column};
                        locals_.clear();
                        for (size_t i = 0; i < args.size(); ++i)
                            locals_.push_back(ensureNonNull(ValuePtr(args[i])));
                        while (locals_.size() < fn->arity) locals_.push_back(std::make_shared<Value>(Value::nil()));
                        appendCaptures();
                        ip_ = fn->entryPoint - 1;
                    } else {
                        deferStack_.push_back({});
                        callStack_.push_back({fn->name.empty() ? "<anonymous>" : fn->name, callerPath, inst.line, inst.column});
                        callFrames_.push_back(ip_);
                        frameLocals_.push_back(std::move(locals_));
                        locals_.clear();
                        for (size_t i = 0; i < args.size(); ++i)
                            locals_.push_back(ensureNonNull(ValuePtr(args[i])));
                        while (locals_.size() < fn->arity)
                            locals_.push_back(std::make_shared<Value>(Value::nil()));
                        appendCaptures();
                        ip_ = fn->entryPoint - 1;
                    }
                }
            } else {
                throw VMError("Invalid call target: value is not callable", inst.line, inst.column, 2,
                              static_cast<int>(VMErrorCode::INVALID_CALL_TARGET));
            }
            break;
        }
        case Opcode::DEFER: {
            size_t n = getOperandU64(inst);
            if (stack_.size() < n)
                throw VMError("Stack underflow in defer", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
            if (deferStack_.empty())
                throw VMError("Invalid defer stack state", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            std::vector<ValuePtr> args;
            for (size_t i = 0; i < n - 1; ++i) args.push_back(popStack());
            std::reverse(args.begin(), args.end());
            ValuePtr callee = popStack();
            deferStack_.back().emplace_back(std::move(callee), std::move(args));
            break;
        }
        case Opcode::RETURN: {
            if (inGeneratorExecution_ && callFrames_.empty()) {
                doneGenerator_ = true;
                break;
            }
            ValuePtr result = stack_.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            if (callFrames_.empty())
                throw VMError("Return outside function", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::RETURN_OUTSIDE_FUNCTION));
            if (!deferStack_.empty()) runDeferredCalls();
            if (!callStack_.empty()) callStack_.pop_back();
            ip_ = callFrames_.back();
            callFrames_.pop_back();
            locals_ = std::move(frameLocals_.back());
            frameLocals_.pop_back();
            deferStack_.pop_back();
            if (!codeFrameStack_.empty()) {
                auto t = std::move(codeFrameStack_.back());
                codeFrameStack_.pop_back();
                code_ = std::move(std::get<0>(t));
                stringConstants_ = std::move(std::get<1>(t));
                valueConstants_ = std::move(std::get<2>(t));
                activeSourcePath_ = std::move(std::get<3>(t));
            }
            push(result);
            break;
        }
        case Opcode::BUILD_FUNC: {
            size_t entry = getOperandU64(inst);
            if (entry >= code_.size()) throw VMError("Invalid function entry point", inst.line, inst.column);
            auto fn = std::make_shared<FunctionObject>();
            fn->entryPoint = entry;
            fn->arity = 0;
            if (currentScript_) {
                fn->script = currentScript_;  // so we can run after import returns
            } else {
                if (!entryScriptCache_) {
                    entryScriptCache_ = std::make_shared<ScriptCode>();
                    entryScriptCache_->code = code_;
                    entryScriptCache_->stringConstants = stringConstants_;
                    entryScriptCache_->valueConstants = valueConstants_;
                    entryScriptCache_->sourcePath = activeSourcePath_;
                }
                fn->script = entryScriptCache_;
            }
            push(std::make_shared<Value>(Value::fromFunction(fn)));
            break;
        }
        case Opcode::BUILD_CLOSURE: {
            if (!std::holds_alternative<std::pair<size_t, size_t>>(inst.operand))
                throw VMError("Invalid bytecode operand: BUILD_CLOSURE expects pair(entry, captureCount)", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            auto p = std::get<std::pair<size_t, size_t>>(inst.operand);
            size_t entry = p.first;
            size_t captureCount = p.second;
            if (entry >= code_.size()) throw VMError("Invalid function entry point", inst.line, inst.column);
            if (stack_.size() < captureCount)
                throw VMError("Stack underflow in BUILD_CLOSURE", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
            std::vector<ValuePtr> caps;
            caps.reserve(captureCount);
            for (size_t i = 0; i < captureCount; ++i) caps.push_back(popStack());
            std::reverse(caps.begin(), caps.end());
            auto fn = std::make_shared<FunctionObject>();
            fn->entryPoint = entry;
            fn->arity = 0;
            fn->captures = std::move(caps);
            if (currentScript_) {
                fn->script = currentScript_;
            } else {
                if (!entryScriptCache_) {
                    entryScriptCache_ = std::make_shared<ScriptCode>();
                    entryScriptCache_->code = code_;
                    entryScriptCache_->stringConstants = stringConstants_;
                    entryScriptCache_->valueConstants = valueConstants_;
                    entryScriptCache_->sourcePath = activeSourcePath_;
                }
                fn->script = entryScriptCache_;
            }
            push(std::make_shared<Value>(Value::fromFunction(fn)));
            break;
        }
        case Opcode::SET_FUNC_ARITY: {
            size_t arity = getOperandU64(inst);
            ValuePtr v = popStack();
            if (v->type == Value::Type::FUNCTION)
                std::get<FunctionPtr>(v->data)->arity = arity;
            push(std::move(v));
            break;
        }
        case Opcode::SET_FUNC_PARAM_NAMES: {
            std::string joined = getOperandStr(inst);
            ValuePtr v = popStack();
            if (v->type == Value::Type::FUNCTION) {
                std::vector<std::string> names;
                for (size_t i = 0; i < joined.size(); ) {
                    size_t c = joined.find(',', i);
                    if (c == std::string::npos) { names.push_back(joined.substr(i)); break; }
                    names.push_back(joined.substr(i, c - i));
                    i = c + 1;
                }
                if (names.empty() && !joined.empty()) names.push_back(joined);
                std::get<FunctionPtr>(v->data)->paramNames = std::move(names);
            }
            push(std::move(v));
            break;
        }
        case Opcode::SET_FUNC_NAME: {
            std::string name = getOperandStr(inst);
            ValuePtr v = popStack();
            if (v->type == Value::Type::FUNCTION)
                std::get<FunctionPtr>(v->data)->name = name;
            push(std::move(v));
            break;
        }
        case Opcode::SET_FUNC_GENERATOR: {
            ValuePtr v = popStack();
            if (v->type == Value::Type::FUNCTION)
                std::get<FunctionPtr>(v->data)->isGenerator = true;
            push(std::move(v));
            break;
        }
        case Opcode::YIELD: {
            ValuePtr val = stack_.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            if (!inGeneratorExecution_)
                throw VMError("yield outside generator", inst.line, inst.column, 1);
            pendingYield_ = true;
            pendingYieldValue_ = std::move(val);
            break;
        }
        case Opcode::NEW_OBJECT: {
            push(std::make_shared<Value>(Value::fromMap({})));
            break;
        }
        case Opcode::BUILD_ARRAY: {
            size_t n = getOperandU64(inst);
            const size_t kMaxArraySize = 64 * 1024 * 1024;
            if (n > kMaxArraySize)
                throw VMError("Array size too large", inst.line, inst.column, 1);
            if (stack_.size() < n)
                throw VMError("Stack underflow building array (need " + std::to_string(n) + " values)", inst.line, inst.column, 1);
            std::vector<ValuePtr> arr;
            arr.reserve(n);
            for (size_t i = 0; i < n; ++i) arr.push_back(popStack());
            std::reverse(arr.begin(), arr.end());
            push(std::make_shared<Value>(Value::fromArray(std::move(arr))));
            break;
        }
        case Opcode::SPREAD: {
            ValuePtr spreadVal = popStack();
            ValuePtr accVal = popStack();
            if (accVal->type != Value::Type::ARRAY) { push(std::move(accVal)); push(std::move(spreadVal)); break; }
            if (spreadVal->type != Value::Type::ARRAY) { push(std::move(accVal)); push(std::move(spreadVal)); break; }
            auto& acc = std::get<std::vector<ValuePtr>>(accVal->data);
            auto& sp = std::get<std::vector<ValuePtr>>(spreadVal->data);
            for (auto& x : acc) x = ensureNonNull(ValuePtr(x));
            for (auto& x : sp) x = ensureNonNull(ValuePtr(x));
            acc.insert(acc.end(), sp.begin(), sp.end());
            push(accVal);
            break;
        }
        case Opcode::GET_FIELD: {
            ValuePtr obj = popStack();
            std::string field = getOperandStr(inst);
            if (obj && obj->type == Value::Type::MAP) {
                auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(obj->data);
                auto it = m.find(field);
                if (it != m.end()) {
                    it->second = ensureNonNull(ValuePtr(it->second));
                    push(it->second);
                } else {
                    auto proto = m.find("__class");
                    if (proto != m.end() && proto->second && proto->second->type == Value::Type::MAP) {
                        auto& cm = std::get<std::unordered_map<std::string, ValuePtr>>(proto->second->data);
                        auto cit = cm.find(field);
                        if (cit != cm.end()) {
                            cit->second = ensureNonNull(ValuePtr(cit->second));
                            push(cit->second);
                        } else push(std::make_shared<Value>(Value::nil()));
                    } else push(std::make_shared<Value>(Value::nil()));
                }
            } else push(std::make_shared<Value>(Value::nil()));
            break;
        }
        case Opcode::SET_FIELD: {
            ValuePtr val = popStack();
            ValuePtr obj = popStack();
            std::string field = getOperandStr(inst);
            ValuePtr stored = ensureNonNull(std::move(val));
            if (obj && obj->type == Value::Type::MAP)
                std::get<std::unordered_map<std::string, ValuePtr>>(obj->data)[field] = stored;
            push(stored);
            break;
        }
        case Opcode::GET_INDEX: {
            ValuePtr index = popStack(), obj = popStack();
            if (obj && obj->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
                int64_t raw = toInt(index);
                size_t i = static_cast<size_t>(raw >= 0 ? raw : std::max(int64_t(0), raw + static_cast<int64_t>(arr.size())));
                if (i < arr.size()) {
                    arr[i] = ensureNonNull(ValuePtr(arr[i]));
                    push(arr[i]);
                } else push(std::make_shared<Value>(Value::nil()));
            } else if (obj && obj->type == Value::Type::STRING) {
                const std::string& s = std::get<std::string>(obj->data);
                int64_t raw = toInt(index);
                int64_t len = static_cast<int64_t>(s.size());
                int64_t i = (raw >= 0) ? raw : raw + len;
                if (i >= 0 && i < len)
                    push(std::make_shared<Value>(Value::fromString(s.substr(static_cast<size_t>(i), 1))));
                else
                    push(std::make_shared<Value>(Value::nil()));
            } else if (obj && obj->type == Value::Type::MAP) {
                std::string key;
                if (mapIndexToKey(index, key)) {
                    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(obj->data);
                    auto it = m.find(key);
                    if (it != m.end()) {
                        it->second = ensureNonNull(ValuePtr(it->second));
                        push(it->second);
                    } else push(std::make_shared<Value>(Value::nil()));
                } else
                    push(std::make_shared<Value>(Value::nil()));
            } else push(std::make_shared<Value>(Value::nil()));
            break;
        }
        case Opcode::ARRAY_LEN: {
            ValuePtr obj = popStack();
            if (obj && obj->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
                push(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(arr.size()))));
            } else
                push(std::make_shared<Value>(Value::fromInt(0)));
            break;
        }
        case Opcode::SET_INDEX: {
            ValuePtr val = popStack(), index = popStack(), obj = popStack();
            ValuePtr stored = ensureNonNull(std::move(val));
            if (obj && obj->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
                int64_t raw = toInt(index);
                if (raw < 0) raw += static_cast<int64_t>(arr.size());
                if (raw < 0) raw = 0;
                const size_t kMaxArraySize = 64 * 1024 * 1024;
                size_t i = static_cast<size_t>(raw);
                if (i > kMaxArraySize)
                    throw VMError("Array index out of range", inst.line, inst.column, 6);
                while (arr.size() <= i) arr.push_back(std::make_shared<Value>(Value::nil()));
                arr[i] = stored;
            } else if (obj && obj->type == Value::Type::MAP) {
                std::string key;
                if (mapIndexToKey(index, key))
                    std::get<std::unordered_map<std::string, ValuePtr>>(obj->data)[key] = stored;
            }
            push(stored);
            break;
        }
        case Opcode::PRINT: {
            ValuePtr v = peek();
            std::cout << (v ? v->toString() : "null") << std::endl;
            break;
        }
        case Opcode::BUILTIN: {
            size_t idx = getOperandU64(inst);
            auto it = builtins_.find(idx);
            if (it != builtins_.end()) push(std::make_shared<Value>(it->second(this, {})));
            else
                throw VMError("Invalid builtin index in bytecode", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            break;
        }
        case Opcode::TRY_BEGIN: {
            size_t catchAddr = getOperandU64(inst);
            if (catchAddr == 0 || catchAddr > code_.size())
                throw VMError("Invalid catch target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            // Create exception frame with current stack and call frame marks
            exceptionStack_.emplace_back(catchAddr, stack_.size(), callStack_.size());
            tryStack_.push_back(catchAddr);
            break;
        }
        case Opcode::TRY_END: {
            if (!tryStack_.empty()) tryStack_.pop_back();
            if (!exceptionStack_.empty()) {
                // Pop the exception frame (exception scope ends)
                exceptionStack_.pop_back();
            }
            break;
        }
        case Opcode::THROW: {
            ValuePtr val = stack_.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            attachTracebackToError(val);
            if (tryStack_.empty() || exceptionStack_.empty()) {
                ThrownErrorInfo info = classifyThrownError(val);
                SourceSpan candidate = normalizeSourceSpan(info.line, info.column, info.lineEnd, info.columnEnd);
                if (!hasFullSourceSpan(candidate))
                    candidate = normalizeSourceSpan(inst.line, 1, inst.line, 1);
                throw VMError(info.message, candidate.line, candidate.column, info.category, info.code,
                    candidate.lineEnd, candidate.columnEnd);
            }
            // Use scoped exception frame instead of global lastThrown_
            auto& frame = exceptionStack_.back();
            frame.thrown = val;
            size_t catchAddr = frame.catchIp;
            tryStack_.pop_back();
            // Rollback stack to saved mark (transactional unwind)
            if (stack_.size() > frame.stackMark) {
                stack_.resize(frame.stackMark);
            }
            if (catchAddr == 0 || catchAddr > code_.size())
                throw VMError("Invalid catch target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            push(val);  // Push exception onto stack for catch handler
            ip_ = catchAddr - 1;
            break;
        }
        case Opcode::RETHROW: {
            // RETHROW requires an active exception frame with a thrown value
            if (exceptionStack_.empty() || !exceptionStack_.back().thrown) {
                throw VMError("No active exception to rethrow", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_OPERATION));
            }
            ValuePtr val = exceptionStack_.back().thrown;
            if (tryStack_.empty()) {
                ThrownErrorInfo info = classifyThrownError(val);
                SourceSpan candidate = normalizeSourceSpan(info.line, info.column, info.lineEnd, info.columnEnd);
                if (!hasFullSourceSpan(candidate))
                    candidate = normalizeSourceSpan(inst.line, 1, inst.line, 1);
                throw VMError(info.message, candidate.line, candidate.column, info.category, info.code,
                    candidate.lineEnd, candidate.columnEnd);
            }
            auto& frame = exceptionStack_.back();
            size_t catchAddr = frame.catchIp;
            tryStack_.pop_back();
            // Rollback stack to saved mark
            if (stack_.size() > frame.stackMark) {
                stack_.resize(frame.stackMark);
            }
            if (catchAddr == 0 || catchAddr > code_.size())
                throw VMError("Invalid catch target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            push(val);
            ip_ = catchAddr - 1;
            break;
        }
        case Opcode::SLICE: {
            if (stack_.size() < 4) throw VMError("Stack underflow in slice", inst.line, inst.column, 6);
            ValuePtr stepVal = popStack(), endVal = popStack(), startVal = popStack(), obj = popStack();
            if (!obj || obj->type != Value::Type::ARRAY) { push(std::make_shared<Value>(Value::fromArray({}))); break; }
            auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
            // heal null slots in the source array (same contract as SPREAD / GET_INDEX).
            for (auto& x : arr) x = ensureNonNull(ValuePtr(x));
            int64_t len = static_cast<int64_t>(arr.size());
            int64_t start = (startVal && startVal->type != Value::Type::NIL) ? toInt(startVal) : 0;
            int64_t end = (endVal && endVal->type != Value::Type::NIL) ? toInt(endVal) : len;
            int64_t step = (stepVal && stepVal->type != Value::Type::NIL) ? toInt(stepVal) : 1;
            if (step == 0) step = 1;
            if (start < 0) start = std::max(int64_t(0), start + len);
            if (end < 0) end = std::max(int64_t(0), end + len);
            if (end > len) end = len;
            if (start > len) start = len;
            std::vector<ValuePtr> out;
            // bounds-check every index (start can exceed len before clamp; step can still skip safely).
            if (step > 0) {
                for (int64_t i = start; i < end; i += step) {
                    if (i >= 0 && i < len)
                        out.push_back(ensureNonNull(ValuePtr(arr[static_cast<size_t>(i)])));
                }
            } else if (step < 0) {
                for (int64_t i = start; i > end; i += step) {
                    if (i >= 0 && i < len)
                        out.push_back(ensureNonNull(ValuePtr(arr[static_cast<size_t>(i)])));
                }
            }
            push(std::make_shared<Value>(Value::fromArray(std::move(out))));
            break;
        }
        case Opcode::FOR_IN_ITER: {
            ValuePtr iterable = popStack();
            iterStack_.push_back({iterable, 0});
            break;
        }
        case Opcode::FOR_IN_NEXT: {
            size_t slot1, slot2 = static_cast<size_t>(-1);
            if (std::holds_alternative<std::pair<size_t, size_t>>(inst.operand)) {
                auto p = std::get<std::pair<size_t, size_t>>(inst.operand);
                slot1 = p.first;
                slot2 = p.second;
            } else {
                slot1 = getOperandU64(inst);
            }
            if (iterStack_.empty()) { push(std::make_shared<Value>(Value::fromBool(false))); break; }
            auto& [v, i] = iterStack_.back();
            if (v->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(v->data);
                if (i < arr.size()) {
                    while (locals_.size() <= slot1) locals_.push_back(std::make_shared<Value>(Value::nil()));
                    locals_[slot1] = ensureNonNull(ValuePtr(arr[i]));
                    i++;
                    push(std::make_shared<Value>(Value::fromBool(true)));
                } else {
                    iterStack_.pop_back();
                    push(std::make_shared<Value>(Value::fromBool(false)));
                }
            } else if (v->type == Value::Type::MAP) {
                auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(v->data);
                std::vector<std::string> keys;
                for (const auto& kv : m) keys.push_back(kv.first);
                std::sort(keys.begin(), keys.end());
                if (i < keys.size()) {
                    while (locals_.size() <= slot1) locals_.push_back(std::make_shared<Value>(Value::nil()));
                    locals_[slot1] = std::make_shared<Value>(Value::fromString(keys[i]));
                    if (slot2 != static_cast<size_t>(-1)) {
                        while (locals_.size() <= slot2) locals_.push_back(std::make_shared<Value>(Value::nil()));
                        locals_[slot2] = ensureNonNull(ValuePtr(m[keys[i]]));
                    }
                    i++;
                    push(std::make_shared<Value>(Value::fromBool(true)));
                } else {
                    iterStack_.pop_back();
                    push(std::make_shared<Value>(Value::fromBool(false)));
                }
            } else if (v->type == Value::Type::GENERATOR) {
                auto gen = std::get<GeneratorPtr>(v->data);
                ValuePtr yielded;
                if (!resumeGenerator(gen, yielded)) {
                    iterStack_.pop_back();
                    push(std::make_shared<Value>(Value::fromBool(false)));
                } else {
                    while (locals_.size() <= slot1) locals_.push_back(std::make_shared<Value>(Value::nil()));
                    locals_[slot1] = ensureNonNull(std::move(yielded));
                    if (slot2 != static_cast<size_t>(-1)) {
                        while (locals_.size() <= slot2) locals_.push_back(std::make_shared<Value>(Value::nil()));
                        locals_[slot2] = std::make_shared<Value>(Value::nil());
                    }
                    push(std::make_shared<Value>(Value::fromBool(true)));
                }
            } else {
                iterStack_.pop_back();
                push(std::make_shared<Value>(Value::fromBool(false)));
            }
            break;
        }
        case Opcode::NOP:
            // explicit no-op: used as a stable patch target in codegen.
            break;
        case Opcode::LOOP: {
            // legacy form: treat like an unconditional jump to operand target.
            size_t target = getOperandU64(inst);
            if (target == 0 || target > code_.size())
                throw VMError("Invalid jump target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            ip_ = target - 1;
            break;
        }
        case Opcode::NEW_INSTANCE:
        case Opcode::INVOKE_METHOD:
        case Opcode::LOAD_THIS:
            throw VMError(std::string("Opcode not implemented: ") + opcodeName(inst.op),
                          inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_BYTECODE));
        case Opcode::ALLOC: {
            int64_t n = toInt(popStack());
            if (n <= 0) { push(std::make_shared<Value>(Value::fromPtr(nullptr))); break; }
            const size_t kMaxAlloc = 256 * 1024 * 1024;  // 256 miB
            size_t sz = static_cast<size_t>(n);
            if (sz > kMaxAlloc)
                throw VMError("Allocation size too large", inst.line, inst.column, 1);
            void* p = std::malloc(sz);
            if (!p) throw VMError("Allocation failed", inst.line, inst.column, 1);
            push(std::make_shared<Value>(Value::fromPtr(p)));
            break;
        }
        case Opcode::FREE: {
            ValuePtr v = popStack();
            if (v->type == Value::Type::PTR) {
                void* p = std::get<void*>(v->data);
                if (p) std::free(p);
            }
            break;
        }
        case Opcode::MEM_COPY: {
            ValuePtr vn = popStack(), vsrc = popStack(), vdst = popStack();
            if (vdst->type != Value::Type::PTR || vsrc->type != Value::Type::PTR) break;
            void* dest = std::get<void*>(vdst->data);
            void* src = std::get<void*>(vsrc->data);
            const size_t kMaxCopy = 256 * 1024 * 1024;  // keep memcpy bounded
            size_t n = static_cast<size_t>(std::max(int64_t(0), toInt(vn)));
            if (n > kMaxCopy) throw VMError("mem_copy size too large", inst.line, inst.column, 3);
            if (dest && src) std::memcpy(dest, src, n);
            break;
        }
        case Opcode::UNSAFE_BEGIN:
            ++unsafeDepth_;
            break;
        case Opcode::UNSAFE_END:
            if (unsafeDepth_ > 0) --unsafeDepth_;
            break;
        case Opcode::HALT:
            ip_ = code_.size();
            break;
        default:
            throw VMError(std::string("Invalid opcode: ") + opcodeName(inst.op),
                          inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_BYTECODE));
    }
}

void VM::restoreExecutionState(
    Bytecode code,
    std::vector<std::string> stringConstants,
    std::vector<Value> valueConstants,
    size_t ip,
    std::vector<ValuePtr> locals,
    std::vector<size_t> callFrames,
    std::vector<std::vector<ValuePtr>> frameLocals,
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> deferStack,
    std::vector<VMStackFrame> callStack,
    std::vector<std::pair<ValuePtr, size_t>> iterStack,
    std::vector<size_t> tryStack,
    std::vector<ExceptionFrame> exceptionStack,
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> codeFrameStack,
    std::shared_ptr<ScriptCode> currentScript,
    std::string activeSourcePath) {
    code_ = std::move(code);
    stringConstants_ = std::move(stringConstants);
    valueConstants_ = std::move(valueConstants);
    ip_ = ip;
    locals_ = std::move(locals);
    callFrames_ = std::move(callFrames);
    frameLocals_ = std::move(frameLocals);
    deferStack_ = std::move(deferStack);
    callStack_ = std::move(callStack);
    iterStack_ = std::move(iterStack);
    tryStack_ = std::move(tryStack);
    exceptionStack_ = std::move(exceptionStack);
    codeFrameStack_ = std::move(codeFrameStack);
    currentScript_ = std::move(currentScript);
    activeSourcePath_ = std::move(activeSourcePath);
    normalizeValuePtrVector(locals_);
    for (auto& fr : frameLocals_) normalizeValuePtrVector(fr);
    for (auto& deflist : deferStack_) {
        for (auto& p : deflist) {
            if (!p.first) p.first = std::make_shared<Value>(Value::nil());
            normalizeValuePtrVector(p.second);
        }
    }
    for (auto& it : iterStack_) {
        if (!it.first) it.first = std::make_shared<Value>(Value::nil());
    }
}

bool VM::resumeGenerator(std::shared_ptr<GeneratorObject> gen, ValuePtr& out) {
    if (!gen || !gen->fn) return false;
    if (gen->exhausted) return false;
    Bytecode savedCode = code_;
    std::vector<std::string> savedStr = stringConstants_;
    std::vector<Value> savedVal = valueConstants_;
    size_t savedIp = ip_;
    std::vector<ValuePtr> savedLocals = locals_;
    std::vector<size_t> savedCF = callFrames_;
    std::vector<std::vector<ValuePtr>> savedFL = frameLocals_;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> savedDef = deferStack_;
    std::vector<VMStackFrame> savedCS = callStack_;
    std::vector<std::pair<ValuePtr, size_t>> savedIter = iterStack_;
    std::vector<size_t> savedTry = tryStack_;
    std::vector<ExceptionFrame> savedException = exceptionStack_;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> savedCFS = codeFrameStack_;
    std::shared_ptr<ScriptCode> savedCurScript = currentScript_;
    std::string savedActivePath = activeSourcePath_;

    if (gen->fn->script) {
        code_ = gen->fn->script->code;
        stringConstants_ = gen->fn->script->stringConstants;
        valueConstants_ = gen->fn->script->valueConstants;
        currentScript_ = gen->fn->script;
        activeSourcePath_ = gen->fn->script->sourcePath;
    }
    ip_ = gen->ip;
    locals_ = gen->locals;
    callFrames_.clear();
    frameLocals_.clear();
    deferStack_.clear();
    callStack_.clear();
    tryStack_.clear();
    codeFrameStack_.clear();
    stack_.clear();

    inGeneratorExecution_ = true;
    activeGenerator_ = gen;
    pendingYield_ = false;
    doneGenerator_ = false;

    try {
        while (ip_ < code_.size() && !gen->exhausted) {
            const Instruction& inst = code_[ip_];
            runInstruction(inst);
            if (pendingYield_) {
                out = pendingYieldValue_;
                gen->ip = ip_ + 1;
                gen->locals = locals_;
                pendingYield_ = false;
                restoreExecutionState(std::move(savedCode), std::move(savedStr), std::move(savedVal), savedIp,
                    std::move(savedLocals), std::move(savedCF), std::move(savedFL), std::move(savedDef),
                    std::move(savedCS), std::move(savedIter), std::move(savedTry), std::move(savedException),
                    std::move(savedCFS), std::move(savedCurScript), std::move(savedActivePath));
                inGeneratorExecution_ = false;
                activeGenerator_.reset();
                return true;
            }
            if (doneGenerator_) {
                gen->exhausted = true;
                restoreExecutionState(std::move(savedCode), std::move(savedStr), std::move(savedVal), savedIp,
                    std::move(savedLocals), std::move(savedCF), std::move(savedFL), std::move(savedDef),
                    std::move(savedCS), std::move(savedIter), std::move(savedTry), std::move(savedException),
                    std::move(savedCFS), std::move(savedCurScript), std::move(savedActivePath));
                inGeneratorExecution_ = false;
                activeGenerator_.reset();
                return false;
            }
            ip_++;
        }
        gen->exhausted = true;
        restoreExecutionState(std::move(savedCode), std::move(savedStr), std::move(savedVal), savedIp,
            std::move(savedLocals), std::move(savedCF), std::move(savedFL), std::move(savedDef),
            std::move(savedCS), std::move(savedIter), std::move(savedTry), std::move(savedException),
            std::move(savedCFS), std::move(savedCurScript), std::move(savedActivePath));
        inGeneratorExecution_ = false;
        activeGenerator_.reset();
        return false;
    } catch (...) {
        restoreExecutionState(std::move(savedCode), std::move(savedStr), std::move(savedVal), savedIp,
            std::move(savedLocals), std::move(savedCF), std::move(savedFL), std::move(savedDef),
            std::move(savedCS), std::move(savedIter), std::move(savedTry), std::move(savedException),
            std::move(savedCFS), std::move(savedCurScript), std::move(savedActivePath));
        inGeneratorExecution_ = false;
        activeGenerator_.reset();
        throw;
    }
}

void VM::attachTracebackToError(ValuePtr val) {
    if (!val || val->type != Value::Type::MAP) return;
    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(val->data);
    if (m.find("traceback") != m.end()) return;
    const size_t depth = callStack_.size();
    const auto slice = getCallStackSlice(kMaxCallStackSnapshotFrames);
    std::vector<ValuePtr> arr;
    arr.reserve(slice.size() + 1);
    if (depth > slice.size()) {
        std::unordered_map<std::string, ValuePtr> marker;
        marker["name"] = std::make_shared<Value>(Value::fromString(
            "(" + std::to_string(depth - slice.size()) + " outer frame(s) omitted)"));
        marker["file"] = std::make_shared<Value>(Value::fromString(""));
        marker["line"] = std::make_shared<Value>(Value::fromInt(0));
        marker["column"] = std::make_shared<Value>(Value::fromInt(0));
        arr.push_back(std::make_shared<Value>(Value::fromMap(std::move(marker))));
    }
    for (const auto& f : slice) {
        std::unordered_map<std::string, ValuePtr> fm;
        fm["name"] = std::make_shared<Value>(Value::fromString(f.functionName));
        // Raw path (e.g. "<repl>") for programmatic use; use humanizePathForDisplay in format_exception / stack_trace.
        fm["file"] = std::make_shared<Value>(Value::fromString(f.filePath));
        fm["line"] = std::make_shared<Value>(Value::fromInt(f.line));
        fm["column"] = std::make_shared<Value>(Value::fromInt(f.column));
        arr.push_back(std::make_shared<Value>(Value::fromMap(std::move(fm))));
    }
    m["traceback"] = std::make_shared<Value>(Value::fromArray(std::move(arr)));
}

void VM::initBuiltins() {
    registerBuiltin(0, [](VM*, std::vector<ValuePtr> args) {
        for (const auto& a : args) std::cout << (a ? a->toString() : "null");
        std::cout << std::endl;
        return Value::nil();
    });
}

ValuePtr VM::callValue(ValuePtr callee, std::vector<ValuePtr> args) {
    if (!callee) return std::make_shared<Value>(Value::nil());
    if (maxCallDepth_ > 0 && callFrames_.size() >= maxCallDepth_)
        throw VMError("Maximum call depth exceeded (" + std::to_string(maxCallDepth_) + ")", 0, 0, 1);
    // exception-safe snapshot so failed callbacks can't corrupt VM control-flow stacks.
    Bytecode savedCodeState = code_;
    std::vector<std::string> savedStrState = stringConstants_;
    std::vector<Value> savedValState = valueConstants_;
    std::vector<ValuePtr> savedStackState = stack_;
    std::vector<ValuePtr> savedLocalsState = locals_;
    std::vector<size_t> savedCallFramesState = callFrames_;
    std::vector<std::vector<ValuePtr>> savedFrameLocalsState = frameLocals_;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> savedDeferState = deferStack_;
    std::vector<VMStackFrame> savedCallStackState = callStack_;
    std::vector<std::pair<ValuePtr, size_t>> savedIterState = iterStack_;
    std::vector<size_t> savedTryState = tryStack_;
    std::vector<ExceptionFrame> savedExceptionState = exceptionStack_;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> savedCodeFramesState =
        codeFrameStack_;
    std::shared_ptr<ScriptCode> savedCurScriptState = currentScript_;
    std::string savedActivePathState = activeSourcePath_;
    int savedUnsafeDepthState = unsafeDepth_;
    size_t savedFrames = callFrames_.size();
    size_t savedIp = ip_;
    try {
        push(callee);
        for (const auto& a : args) push(a);
        Instruction callInst(Opcode::CALL, args.size());
        runInstruction(callInst);
        // match main run(): after each instruction (including CALL), ip_ advances. For entryPoint==0, ip_ was SIZE_MAX; ++ wraps to 0.
        if (callFrames_.size() > savedFrames) ip_++;
        size_t guard = 0;
        const size_t maxGuard = callbackStepGuard_ == 0
            ? std::max<size_t>(
                  500000,
                  code_.empty() ? 4096 : (code_.size() * 64 + 4096))
            : static_cast<size_t>(callbackStepGuard_);
        while (callFrames_.size() > savedFrames && ip_ < code_.size()) {
            const Instruction& inst = code_[ip_];
            runInstruction(inst);
            if (++guard > maxGuard) break;
            ip_++;
        }

        // safety: if callback did not unwind naturally, restore the caller frame.
        while (callFrames_.size() > savedFrames) {
            ip_ = callFrames_.back();
            callFrames_.pop_back();
            if (!frameLocals_.empty()) {
                locals_ = std::move(frameLocals_.back());
                frameLocals_.pop_back();
            }
            if (!deferStack_.empty()) deferStack_.pop_back();
            if (!callStack_.empty()) callStack_.pop_back();
            if (!codeFrameStack_.empty()) {
                auto t = std::move(codeFrameStack_.back());
                codeFrameStack_.pop_back();
                code_ = std::move(std::get<0>(t));
                stringConstants_ = std::move(std::get<1>(t));
                valueConstants_ = std::move(std::get<2>(t));
                activeSourcePath_ = std::move(std::get<3>(t));
            }
            push(std::make_shared<Value>(Value::nil()));
        }
        ip_ = savedIp;
        if (stack_.empty()) return std::make_shared<Value>(Value::nil());
        ValuePtr result = popStack();
        return result;
    } catch (...) {
        code_ = std::move(savedCodeState);
        stringConstants_ = std::move(savedStrState);
        valueConstants_ = std::move(savedValState);
        stack_ = std::move(savedStackState);
        locals_ = std::move(savedLocalsState);
        callFrames_ = std::move(savedCallFramesState);
        frameLocals_ = std::move(savedFrameLocalsState);
        deferStack_ = std::move(savedDeferState);
        callStack_ = std::move(savedCallStackState);
        iterStack_ = std::move(savedIterState);
        tryStack_ = std::move(savedTryState);
        exceptionStack_ = std::move(savedExceptionState);
        codeFrameStack_ = std::move(savedCodeFramesState);
        currentScript_ = std::move(savedCurScriptState);
        activeSourcePath_ = std::move(savedActivePathState);
        unsafeDepth_ = savedUnsafeDepthState;
        ip_ = savedIp;
        throw;
    }
}

void VM::runDeferredCalls() {
    if (deferStack_.empty()) return;
    auto& list = deferStack_.back();
    while (!list.empty()) {
        auto [callee, args] = std::move(list.back());
        list.pop_back();
        if (!callee) continue;
        if (callee->type == Value::Type::FUNCTION) {
            auto& fn = std::get<FunctionPtr>(callee->data);
            if (fn->isBuiltin) {
                BuiltinFn* fast = (fn->builtinIndex < builtinsVec_.size()) ? &builtinsVec_[fn->builtinIndex] : nullptr;
                if (fast && *fast)
                    push(std::make_shared<Value>((*fast)(this, args)));
                else {
                    auto it = builtins_.find(fn->builtinIndex);
                    if (it != builtins_.end()) {
                        push(std::make_shared<Value>(it->second(this, args)));
                    } else {
                        throw VMError("Invalid builtin index", 0, 0, 1, static_cast<int>(VMErrorCode::INVALID_BYTECODE));
                    }
                }
                popStack();
            } else {
                deferStack_.push_back({});
                size_t savedFrames = callFrames_.size();
                callFrames_.push_back(ip_);
                frameLocals_.push_back(std::move(locals_));
                locals_.clear();
                for (size_t i = 0; i < args.size(); ++i)
                    locals_.push_back(ensureNonNull(ValuePtr(args[i])));
                while (locals_.size() < fn->arity)
                    locals_.push_back(std::make_shared<Value>(Value::nil()));
                // match main run(): after CALL, ip_ is entryPoint-1 then incremented to entryPoint before executing.
                ip_ = fn->entryPoint - 1;
                while (callFrames_.size() > savedFrames) {
                    ip_++;
                    if (ip_ >= code_.size()) break;
                    runInstruction(code_[ip_]);
                }
                if (!stack_.empty()) popStack();  // discard deferred user function return value
            }
        }
    }
}

// Production-optimized VM execution loop
// 
// Performance notes:
// - Pre-reserved vectors avoid allocations
// - No heap allocations in hot path
// - Switch-based dispatch is branch-prediction friendly
// - vmTraceEnabled_ only checked in debug builds (constexpr optimization possible)
void VM::run() {
    if (code_.empty()) return;
    verifyBytecodeOrThrow(code_, stringConstants_.size(), valueConstants_.size());
    resetCycleCount();
    
    // Pre-reserve to prevent reallocations in hot path
    if (stack_.capacity() < 512) stack_.reserve(512);
    
    while (ip_ < code_.size()) {
        const Instruction& inst = code_[ip_];
        runInstruction(inst);
        
#if defined(KERN_DEBUG) && defined(KERN_DEBUG_VM_TRACE)
        if (vmTraceEnabled_ && cycleCount_ <= 500000u) {
            std::cerr << "[vm] op=" << static_cast<int>(inst.op) 
                      << " line=" << inst.line << " col=" << inst.column
                      << " sp=" << stack_.size() << "\n";
        }
#endif
        
        ip_++;
        if (scriptExitCode_ >= 0) break;
    }
}

void VM::runSubScript(Bytecode code, std::vector<std::string> stringConstants, std::vector<Value> valueConstants,
                      const std::string& sourcePath) {
    auto savedScript = currentScript_;
    std::string savedActive = activeSourcePath_;
    int savedExitCode = scriptExitCode_;
    scriptExitCode_ = -1;  // imported scripts must not terminate parent script
    currentScript_ = std::make_shared<ScriptCode>();
    currentScript_->code = std::move(code);
    currentScript_->stringConstants = std::move(stringConstants);
    currentScript_->valueConstants = std::move(valueConstants);
    currentScript_->sourcePath = sourcePath;
    verifyBytecodeOrThrow(currentScript_->code, currentScript_->stringConstants.size(), currentScript_->valueConstants.size());
    Bytecode savedCode = std::move(code_);
    std::vector<std::string> savedStr = std::move(stringConstants_);
    std::vector<Value> savedVal = std::move(valueConstants_);
    size_t savedIp = ip_;
    code_ = currentScript_->code;
    stringConstants_ = currentScript_->stringConstants;
    valueConstants_ = currentScript_->valueConstants;
    activeSourcePath_ = sourcePath;
    ip_ = 0;
    while (ip_ < code_.size()) {
        const Instruction& inst = code_[ip_];
        runInstruction(inst);
        ip_++;
        if (scriptExitCode_ >= 0) break;
    }
    code_ = std::move(savedCode);
    stringConstants_ = std::move(savedStr);
    valueConstants_ = std::move(savedVal);
    ip_ = savedIp;
    activeSourcePath_ = std::move(savedActive);
    currentScript_ = savedScript;
    scriptExitCode_ = savedExitCode;
}

} // namespace kern
