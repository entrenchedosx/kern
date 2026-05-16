/* *
 * kern VM Implementation
 */

#include "vm.hpp"
#include "builtins.hpp"
#include "ffi_module.hpp"
#include "bytecode_verifier.hpp"
#include "bytecode/bytecode_serializer.hpp"
#include "errors/vm_error_registry.hpp"
#include "errors/errors.hpp"
#include "platform/env_compat.hpp"
#include <array>

namespace kern {
    struct Allocator {};
    struct VM::ModuleContextImpl {};
}
#include <sstream>
#include <string>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <fstream>

namespace kern {

/* * map subscript: string key, or int/float coerced to decimal string (for handles-as-keys).
 * SAFETY: Returns false for null, nil, or invalid types - never crashes.
 */
static bool mapIndexToKey(const ValuePtr& index, std::string& out) {
    if (!index) return false;
    
    // Explicit NIL check to prevent use of nil as dictionary key
    if (index->type == Value::Type::NIL) {
        return false;
    }
    
    switch (index->type) {
        case Value::Type::STRING:
            try {
                out = std::get<std::string>(index->data);
                return true;
            } catch (...) {
                return false;
            }
        case Value::Type::INT:
            try {
                out = std::to_string(std::get<int64_t>(index->data));
                return true;
            } catch (...) {
                return false;
            }
        case Value::Type::FLOAT:
            try {
                out = std::to_string(std::get<double>(index->data));
                return true;
            } catch (...) {
                return false;
            }
        default:
            // Reject all other types (FUNC, MAP, ARRAY, PTR, etc.)
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

VM::VM(const VMConfig& config) : ip_(0), vmTraceEnabled_(false) {
    (void)config; // Suppress unused parameter warning
    // Pre-reserve vectors to avoid allocations in hot path
    stack.reserve(512);
    callStack.reserve(128);
    tryStack.reserve(64);
    exceptionStack.reserve(64);
    callFrames.reserve(256);
    frameLocals_.reserve(64);
    deferStack.reserve(32);
    
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

VM::~VM() {
    // Break shared_ptr cycles to prevent reference leaks
    currentScript.reset();
    entryScriptCache.reset();
    activeGenerator.reset();
    
    // Clear VM state vectors
    stack.clear();
    callStack.clear();
    tryStack.clear();
    exceptionStack.clear();
    callFrames.clear();
    frameLocals_.clear();
    deferStack.clear();
    iterStack.clear();
    codeFrameStack.clear();
    
    // Note: Global state cleanup is now explicit via VM::shutdownGlobalState()
    // This removes hidden coupling between VM instances
}

void VM::shutdownGlobalState() {
    extern void cleanupGlobalMemoryState();
    cleanupGlobalMemoryState();
}

void VM::setBytecode(Bytecode code) {
    code_ = std::move(code);
    ip_ = 0;
    unsafeDepth_ = 0;
    exceptionStack.clear();  // Clear exception frames
    tryStack.clear();        // Clear try stack
    entryScriptCache.reset();
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
    
    if (stack.capacity() < 512) stack.reserve(512);
    
    while (ip_ < code_.size()) {
        if (breakpoints_.count(ip_)) return;
        const Instruction& inst = code_[ip_];
        runInstruction(inst);
        
#if defined(KERN_DEBUG) && defined(KERN_DEBUG_VM_TRACE)
        if (vmTraceEnabled_ && cycleCount_ <= 500000u) {
            std::cerr << "[vm] op=" << static_cast<int>(inst.op) 
                      << " line=" << inst.line << " col=" << inst.column
                      << " sp=" << stack.size() << "\n";
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
    globals[name] = ensureNonNull(std::move(value));
}

ValuePtr VM::getGlobal(const std::string& name) const {
    auto it = globals.find(name);
    return it != globals.end() ? it->second : nullptr;
}

std::unordered_map<std::string, ValuePtr> VM::getGlobalsSnapshot() const {
    auto m = globals;
    for (auto& kv : m) {
        if (!kv.second) kv.second = std::make_shared<Value>(Value::nil());
    }
    return m;
}

ValuePtr VM::popStack() {
    if (stack.empty())
        throw VMError("Stack underflow", 0, 0, 1, static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
    ValuePtr v = stack.back();
    stack.pop_back();
    return ensureNonNull(std::move(v));
}

ValuePtr VM::getResult() {
    if (stack.empty()) return std::make_shared<Value>(Value::nil());
    ValuePtr& top = stack.back();
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
    if (stack.size() >= kMaxStackSize) {
        throw VMError("Stack overflow: exceeded maximum stack size", 0, 0, 1,
                      static_cast<int>(VMErrorCode::STACK_OVERFLOW));
    }
    stack.push_back(ensureNonNull(std::move(v)));
}

ValuePtr VM::peek() {
    if (stack.empty())
        throw VMError("Stack underflow", 0, 0, 1, static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
    ValuePtr& top = stack.back();
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
            auto it = globals.find(name);
            ValuePtr v = it != globals.end() ? ensureNonNull(it->second) : std::make_shared<Value>(Value::nil());
            push(std::move(v));
            break;
        }
        case Opcode::STORE_GLOBAL: {
            std::string name = getOperandStr(inst);
            globals[name] = popStack();
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
        case Opcode::ADD_INT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromInt(toInt(a) + toInt(b))));
            break;
        }
        case Opcode::ADD_FLOAT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) + toDouble(b))));
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
        case Opcode::SUB_INT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromInt(toInt(a) - toInt(b))));
            break;
        }
        case Opcode::SUB_FLOAT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) - toDouble(b))));
            break;
        }
        case Opcode::MUL: {
            ValuePtr b = popStack(), a = popStack();
            requireNumeric(a, "MUL");
            requireNumeric(b, "MUL");
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) * toDouble(b))));
            break;
        }
        case Opcode::MUL_INT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromInt(toInt(a) * toInt(b))));
            break;
        }
        case Opcode::MUL_FLOAT: {
            ValuePtr b = popStack(), a = popStack();
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
        case Opcode::DIV_INT: {
            ValuePtr b = popStack(), a = popStack();
            int64_t den = toInt(b);
            if (den == 0)
                throw VMError("Division by zero", inst.line, inst.column, 4, static_cast<int>(VMErrorCode::DIVISION_BY_ZERO));
            push(std::make_shared<Value>(Value::fromInt(toInt(a) / den)));
            break;
        }
        case Opcode::DIV_FLOAT: {
            ValuePtr b = popStack(), a = popStack();
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
        case Opcode::NEG_INT: {
            ValuePtr v = popStack();
            push(std::make_shared<Value>(Value::fromInt(-toInt(v))));
            break;
        }
        case Opcode::NEG_FLOAT: {
            ValuePtr v = popStack();
            push(std::make_shared<Value>(Value::fromFloat(-toDouble(v))));
            break;
        }
        case Opcode::EQ: {
            ValuePtr b = popStack(), a = popStack();
            bool eq = (a && b) ? a->equals(*b) : (a.get() == b.get());
            push(std::make_shared<Value>(Value::fromBool(eq)));
            break;
        }
        case Opcode::EQ_INT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool(toInt(a) == toInt(b))));
            break;
        }
        case Opcode::EQ_FLOAT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) == toDouble(b))));
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
        case Opcode::LT_INT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool(toInt(a) < toInt(b))));
            break;
        }
        case Opcode::LT_FLOAT: {
            ValuePtr b = popStack(), a = popStack();
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
            if (stack.size() < argc + 1)
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
                    if (maxCallDepth_ > 0 && callFrames.size() >= maxCallDepth_) {
                        throw VMError("Maximum call depth exceeded (" + std::to_string(maxCallDepth_) + ")", inst.line, inst.column, 1);
                    }
                    // tail-call check must use the caller's bytecode/ip_. Switching code_ for fn->script first
                    // would read the wrong instruction (cross-script calls: e.g. callback from module -> main).
                    bool tailCall = (ip_ + 1 < code_.size() && code_[ip_ + 1].op == Opcode::RETURN)
                        && !deferStack.empty() && deferStack.back().empty();
                    // When maxCallDepth_ is enforced, do not reuse frames: tail recursion would bypass the limit.
                    if (maxCallDepth_ > 0) tailCall = false;
                    const std::string callerPath = activeSourcePath_;
                    // if function was defined in an imported script, switch to its bytecode for the call
                    if (fn->script) {
                        codeFrameStack.push_back(
                            std::make_tuple(std::move(code_), std::move(stringConstants_), std::move(valueConstants_), callerPath));
                        code_ = fn->script->code;
                        stringConstants_ = fn->script->stringConstants;
                        valueConstants_ = fn->script->valueConstants;
                        activeSourcePath_ = fn->script->sourcePath;
                    }
                    // tail-call reuse is invalid without an outer frame (would callStack.back() UB).
                    auto appendCaptures = [&] {
                        for (const auto& c : fn->captures)
                            locals_.push_back(ensureNonNull(c ? c : std::make_shared<Value>(Value::nil())));
                    };
                    if (tailCall && !callStack.empty()) {
                        callStack.back() = {fn->name.empty() ? "<anonymous>" : fn->name, callerPath, inst.line, inst.column};
                        locals_.clear();
                        for (size_t i = 0; i < args.size(); ++i)
                            locals_.push_back(ensureNonNull(ValuePtr(args[i])));
                        while (locals_.size() < fn->arity) locals_.push_back(std::make_shared<Value>(Value::nil()));
                        appendCaptures();
                        ip_ = fn->entryPoint - 1;
                    } else {
                        deferStack.push_back({});
                        callStack.push_back({fn->name.empty() ? "<anonymous>" : fn->name, callerPath, inst.line, inst.column});
                        callFrames.push_back(CallFrame{reinterpret_cast<const uint8_t*>(ip_), nullptr, stack.size(), fn->name, callerPath, static_cast<uint32_t>(inst.line), static_cast<uint32_t>(inst.column)});
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
            } else if (callee->type == Value::Type::FFI_FN) {
                auto& ffi = std::get<FfiClosurePtr>(callee->data);
                push(std::make_shared<Value>(callFfiFunction(ffi.get(), args)));
            } else {
                throw VMError("Invalid call target: value is not callable", inst.line, inst.column, 2,
                              static_cast<int>(VMErrorCode::INVALID_CALL_TARGET));
            }
            break;
        }
        case Opcode::DEFER: {
            size_t n = getOperandU64(inst);
            if (stack.size() < n)
                throw VMError("Stack underflow in defer", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::STACK_UNDERFLOW));
            if (deferStack.empty())
                throw VMError("Invalid defer stack state", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_BYTECODE));
            std::vector<ValuePtr> args;
            for (size_t i = 0; i < n - 1; ++i) args.push_back(popStack());
            std::reverse(args.begin(), args.end());
            ValuePtr callee = popStack();
            deferStack.back().emplace_back(std::move(callee), std::move(args));
            break;
        }
        case Opcode::RETURN: {
            if (inGeneratorExecution_ && callFrames.empty()) {
                doneGenerator_ = true;
                break;
            }
            ValuePtr result = stack.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            if (callFrames.empty()) {
                // If we're running inside a coroutine (via resumeAll), the function
                // body was entered directly at fn->entryPoint without a CALL opcode,
                // so there is no call frame.  Treat this as coroutine completion
                // instead of throwing "Return outside function".
                if (!inGeneratorExecution_ && !coroutines_.empty() && activeCoroutineId_ > 0) {
                    // Signal end of execution — the resumeAll() loop will see
                    // ip_ >= code_.size() and mark the coroutine as DEAD.
                    ip_ = code_.size();
                    break;
                }
                throw VMError("Return outside function", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::RETURN_OUTSIDE_FUNCTION));
            }
            if (!deferStack.empty()) runDeferredCalls();
            if (!callStack.empty()) callStack.pop_back();
            ip_ = callFrames.back().returnPc - reinterpret_cast<const uint8_t*>(0);
            callFrames.pop_back();
            locals_ = std::move(frameLocals_.back());
            frameLocals_.pop_back();
            deferStack.pop_back();
            if (!codeFrameStack.empty()) {
                auto t = std::move(codeFrameStack.back());
                codeFrameStack.pop_back();
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
            if (currentScript) {
                fn->script = currentScript;  // so we can run after import returns
            } else {
                if (!entryScriptCache) {
                    entryScriptCache = std::make_shared<ScriptCode>();
                    entryScriptCache->code = code_;
                    entryScriptCache->stringConstants = stringConstants_;
                    entryScriptCache->valueConstants = valueConstants_;
                    entryScriptCache->sourcePath = activeSourcePath_;
                }
                fn->script = entryScriptCache;
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
            if (stack.size() < captureCount)
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
            if (currentScript) {
                fn->script = currentScript;
            } else {
                if (!entryScriptCache) {
                    entryScriptCache = std::make_shared<ScriptCode>();
                    entryScriptCache->code = code_;
                    entryScriptCache->stringConstants = stringConstants_;
                    entryScriptCache->valueConstants = valueConstants_;
                    entryScriptCache->sourcePath = activeSourcePath_;
                }
                fn->script = entryScriptCache;
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
        case Opcode::SET_FUNC_STRUCT: {
            ValuePtr v = popStack();
            if (v->type == Value::Type::FUNCTION)
                std::get<FunctionPtr>(v->data)->isStructConstructor = true;
            push(std::move(v));
            break;
        }
        case Opcode::YIELD: {
            ValuePtr val = stack.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            if (inGeneratorExecution_) {
                // Existing generator path — resumeGenerator() handles the outer loop
                pendingYield_ = true;
                pendingYieldValue_ = std::move(val);
            } else {
                // Coroutine yield — save state and signal run() to break
                if (coroutines_.empty()) {
                    coroutines_.resize(1);
                    coroutines_[0].state = CoroutineState::RUNNING;
                }
                saveCurrentCoroutineState();
                coroutines_[activeCoroutineId_].state = CoroutineState::YIELDED;
                coroutines_[activeCoroutineId_].yieldedValue = std::move(val);
                coroutineYieldRequested_ = true;
            }
            break;
        }
        case Opcode::NEW_OBJECT: {
            push(std::make_shared<Value>(Value::fromMap(ValueMap{})));
            break;
        }
        case Opcode::BUILD_ARRAY: {
            size_t n = getOperandU64(inst);
            const size_t kMaxArraySize = 64 * 1024 * 1024;
            if (n > kMaxArraySize)
                throw VMError("Array size too large", inst.line, inst.column, 1);
            if (stack.size() < n)
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
            } else if (obj && obj->type == Value::Type::VEC3) {
                auto v = std::get<Vec3Ptr>(obj->data);
                if (field == "x") push(std::make_shared<Value>(Value::fromFloat(v->x)));
                else if (field == "y") push(std::make_shared<Value>(Value::fromFloat(v->y)));
                else if (field == "z") push(std::make_shared<Value>(Value::fromFloat(v->z)));
                else push(std::make_shared<Value>(Value::nil()));
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
        case Opcode::BUILD_VEC3: {
            ValuePtr z = popStack(), y = popStack(), x = popStack();
            requireNumeric(x, "BUILD_VEC3 x");
            requireNumeric(y, "BUILD_VEC3 y");
            requireNumeric(z, "BUILD_VEC3 z");
            push(std::make_shared<Value>(Value::fromVec3(toDouble(x), toDouble(y), toDouble(z))));
            break;
        }
        case Opcode::VEC3_GET_X: {
            ValuePtr v = popStack();
            if (v && v->type == Value::Type::VEC3) {
                auto vec = std::get<Vec3Ptr>(v->data);
                push(std::make_shared<Value>(Value::fromFloat(vec->x)));
            } else {
                push(std::make_shared<Value>(Value::nil()));
            }
            break;
        }
        case Opcode::VEC3_GET_Y: {
            ValuePtr v = popStack();
            if (v && v->type == Value::Type::VEC3) {
                auto vec = std::get<Vec3Ptr>(v->data);
                push(std::make_shared<Value>(Value::fromFloat(vec->y)));
            } else {
                push(std::make_shared<Value>(Value::nil()));
            }
            break;
        }
        case Opcode::VEC3_GET_Z: {
            ValuePtr v = popStack();
            if (v && v->type == Value::Type::VEC3) {
                auto vec = std::get<Vec3Ptr>(v->data);
                push(std::make_shared<Value>(Value::fromFloat(vec->z)));
            } else {
                push(std::make_shared<Value>(Value::nil()));
            }
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
            exceptionStack.emplace_back(reinterpret_cast<const uint8_t*>(catchAddr), stack.size(), callStack.size());
            tryStack.push_back(catchAddr);
            break;
        }
        case Opcode::TRY_END: {
            if (!tryStack.empty()) tryStack.pop_back();
            if (!exceptionStack.empty()) {
                // Pop the exception frame (exception scope ends)
                exceptionStack.pop_back();
            }
            break;
        }
        case Opcode::THROW: {
            ValuePtr val = stack.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            attachTracebackToError(val);
            if (tryStack.empty() || exceptionStack.empty()) {
                ThrownErrorInfo info = classifyThrownError(val);
                SourceSpan candidate = normalizeSourceSpan(info.line, info.column, info.lineEnd, info.columnEnd);
                if (!hasFullSourceSpan(candidate))
                    candidate = normalizeSourceSpan(inst.line, 1, inst.line, 1);
                throw VMError(info.message, candidate.line, candidate.column, info.category, info.code,
                    candidate.lineEnd, candidate.columnEnd);
            }
            // Use scoped exception frame instead of global lastThrown_
            auto& frame = exceptionStack.back();
            frame.thrown = val;
            size_t catchAddr = frame.catchIp;
            tryStack.pop_back();
            // Rollback stack to saved mark (transactional unwind)
            if (stack.size() > frame.stackMark) {
                stack.resize(frame.stackMark);
            }
            if (catchAddr == 0 || catchAddr > code_.size())
                throw VMError("Invalid catch target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            push(val);  // Push exception onto stack for catch handler
            ip_ = catchAddr - 1;
            break;
        }
        case Opcode::RETHROW: {
            // RETHROW requires an active exception frame with a thrown value
            if (exceptionStack.empty() || !exceptionStack.back().thrown) {
                throw VMError("No active exception to rethrow", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::INVALID_OPERATION));
            }
            ValuePtr val = exceptionStack.back().thrown;
            if (tryStack.empty()) {
                ThrownErrorInfo info = classifyThrownError(val);
                SourceSpan candidate = normalizeSourceSpan(info.line, info.column, info.lineEnd, info.columnEnd);
                if (!hasFullSourceSpan(candidate))
                    candidate = normalizeSourceSpan(inst.line, 1, inst.line, 1);
                throw VMError(info.message, candidate.line, candidate.column, info.category, info.code,
                    candidate.lineEnd, candidate.columnEnd);
            }
            auto& frame = exceptionStack.back();
            size_t catchAddr = frame.catchIp;
            tryStack.pop_back();
            // Rollback stack to saved mark
            if (stack.size() > frame.stackMark) {
                stack.resize(frame.stackMark);
            }
            if (catchAddr == 0 || catchAddr > code_.size())
                throw VMError("Invalid catch target", inst.line, inst.column, 1, static_cast<int>(VMErrorCode::INVALID_JUMP_TARGET));
            push(val);
            ip_ = catchAddr - 1;
            break;
        }
        case Opcode::SLICE: {
            if (stack.size() < 4) throw VMError("Stack underflow in slice", inst.line, inst.column, 6);
            ValuePtr stepVal = popStack(), endVal = popStack(), startVal = popStack(), obj = popStack();
            if (!obj || obj->type != Value::Type::ARRAY) { push(std::make_shared<Value>(Value::fromArray(ValueArray{}))); break; }
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
            iterStack.push_back({iterable, 0});
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
            if (iterStack.empty()) { push(std::make_shared<Value>(Value::fromBool(false))); break; }
            auto& [v, i] = iterStack.back();
            if (v->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(v->data);
                if (i < arr.size()) {
                    while (locals_.size() <= slot1) locals_.push_back(std::make_shared<Value>(Value::nil()));
                    locals_[slot1] = ensureNonNull(ValuePtr(arr[i]));
                    i++;
                    push(std::make_shared<Value>(Value::fromBool(true)));
                } else {
                    iterStack.pop_back();
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
                    iterStack.pop_back();
                    push(std::make_shared<Value>(Value::fromBool(false)));
                }
            } else if (v->type == Value::Type::GENERATOR) {
                auto gen = std::get<GeneratorPtr>(v->data);
                ValuePtr yielded;
                if (!resumeGenerator(gen, yielded)) {
                    iterStack.pop_back();
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
                iterStack.pop_back();
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

void VM::saveCurrentCoroutineState() {
    if (activeCoroutineId_ >= coroutines_.size()) return;
    Coroutine& cor = coroutines_[activeCoroutineId_];
    cor.code = code_;
    cor.stringConstants = stringConstants_;
    cor.valueConstants = valueConstants_;
    cor.ip = ip_;
    cor.locals = locals_;
    cor.stack = stack;
    cor.callFrames = callFrames;
    cor.callStack = callStack;
    cor.frameLocals = frameLocals_;
    cor.deferStack = deferStack;
    cor.iterStack = iterStack;
    cor.tryStack = tryStack;
    cor.exceptionStack = exceptionStack;
    cor.codeFrameStack = codeFrameStack;
    cor.currentScript = currentScript;
    cor.activeSourcePath = activeSourcePath_;
    cor.unsafeDepth = unsafeDepth_;
}

void VM::restoreCoroutineState(const Coroutine& cor) {
    code_ = cor.code;
    stringConstants_ = cor.stringConstants;
    valueConstants_ = cor.valueConstants;
    ip_ = cor.ip;
    locals_ = cor.locals;
    stack = cor.stack;
    callFrames = cor.callFrames;
    callStack = cor.callStack;
    frameLocals_ = cor.frameLocals;
    deferStack = cor.deferStack;
    iterStack = cor.iterStack;
    tryStack = cor.tryStack;
    exceptionStack = cor.exceptionStack;
    codeFrameStack = cor.codeFrameStack;
    currentScript = cor.currentScript;
    activeSourcePath_ = cor.activeSourcePath;
    unsafeDepth_ = cor.unsafeDepth;
    normalizeValuePtrVector(locals_);
    for (auto& fr : frameLocals_) normalizeValuePtrVector(fr);
    for (auto& deflist : deferStack) {
        for (auto& p : deflist) {
            if (!p.first) p.first = std::make_shared<Value>(Value::nil());
            normalizeValuePtrVector(p.second);
        }
    }
    for (auto& it : iterStack) {
        if (!it.first) it.first = std::make_shared<Value>(Value::nil());
    }
}

void VM::restoreExecutionState(
    Bytecode code,
    std::vector<std::string> stringConstants,
    std::vector<Value> valueConstants,
    size_t ip,
    std::vector<ValuePtr> locals,
    std::vector<CallFrame> callFrames,
    std::vector<std::vector<ValuePtr>> frameLocals,
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> deferStack,
    std::vector<StackFrame> callStack,
    std::vector<std::pair<ValuePtr, size_t>> iterStack,
    std::vector<size_t> tryStack,
    std::vector<ExceptionFrame> exceptionStack,
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> codeFrameStack,
    std::shared_ptr<ScriptCode> currentScript,
    std::string activeSourcePath) {
    // Use direct assignment to avoid self-move issues
    code_ = std::move(code);
    stringConstants_ = std::move(stringConstants);
    valueConstants_ = std::move(valueConstants);
    ip_ = ip;
    locals_ = std::move(locals);
    this->callFrames = std::move(callFrames);
    frameLocals_ = std::move(frameLocals);
    this->deferStack = std::move(deferStack);
    this->callStack = std::move(callStack);
    iterStack = std::move(iterStack);
    tryStack = std::move(tryStack);
    exceptionStack = std::move(exceptionStack);
    codeFrameStack = std::move(codeFrameStack);
    this->currentScript = std::move(currentScript);
    activeSourcePath_ = std::move(activeSourcePath);
    normalizeValuePtrVector(locals_);
    for (auto& fr : frameLocals_) normalizeValuePtrVector(fr);
    for (auto& deflist : this->deferStack) {
        for (auto& p : deflist) {
            if (!p.first) p.first = std::make_shared<Value>(Value::nil());
            normalizeValuePtrVector(p.second);
        }
    }
    for (auto& it : iterStack) {
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
    std::vector<CallFrame> savedCF = callFrames;
    std::vector<std::vector<ValuePtr>> savedFL = frameLocals_;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> savedDef = deferStack;
    std::vector<StackFrame> savedCS = callStack;
    std::vector<std::pair<ValuePtr, size_t>> savedIter = iterStack;
    std::vector<size_t> savedTry = tryStack;
    std::vector<ExceptionFrame> savedException = exceptionStack;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> savedCFS = codeFrameStack;
    std::shared_ptr<ScriptCode> savedCurScript = currentScript;
    std::string savedActivePath = activeSourcePath_;

    if (gen->fn && gen->fn->script) {
        code_ = gen->fn->script->code;
        stringConstants_ = gen->fn->script->stringConstants;
        valueConstants_ = gen->fn->script->valueConstants;
        currentScript = gen->fn->script;
        activeSourcePath_ = gen->fn->script->sourcePath;
    }
    ip_ = gen->ip;
    locals_ = gen->locals;
    callFrames.clear();
    frameLocals_.clear();
    deferStack.clear();
    callStack.clear();
    tryStack.clear();
    codeFrameStack.clear();
    stack.clear();

    inGeneratorExecution_ = true;
    activeGenerator = gen;
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
                activeGenerator.reset();
                return true;
            }
            if (doneGenerator_) {
                gen->exhausted = true;
                restoreExecutionState(std::move(savedCode), std::move(savedStr), std::move(savedVal), savedIp,
                    std::move(savedLocals), std::move(savedCF), std::move(savedFL), std::move(savedDef),
                    std::move(savedCS), std::move(savedIter), std::move(savedTry), std::move(savedException),
                    std::move(savedCFS), std::move(savedCurScript), std::move(savedActivePath));
                inGeneratorExecution_ = false;
                activeGenerator.reset();
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
        activeGenerator.reset();
        return false;
    } catch (...) {
        restoreExecutionState(std::move(savedCode), std::move(savedStr), std::move(savedVal), savedIp,
            std::move(savedLocals), std::move(savedCF), std::move(savedFL), std::move(savedDef),
            std::move(savedCS), std::move(savedIter), std::move(savedTry), std::move(savedException),
            std::move(savedCFS), std::move(savedCurScript), std::move(savedActivePath));
        inGeneratorExecution_ = false;
        activeGenerator.reset();
        throw;
    }
}

void VM::attachTracebackToError(ValuePtr val) {
    if (!val || val->type != Value::Type::MAP) return;
    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(val->data);
    if (m.find("traceback") != m.end()) return;
    const size_t depth = callStack.size();
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
        // Raw path (e.g. "<repl>") for programmatic use; use humanizePathForDisplay in format_exception / stacktrace.
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

ValuePtr VM::callValue(ValuePtr callee, const std::vector<ValuePtr>& args) {
    if (!callee) return std::make_shared<Value>(Value::nil());
    if (maxCallDepth_ > 0 && callFrames.size() >= maxCallDepth_)
        throw VMError("Maximum call depth exceeded (" + std::to_string(maxCallDepth_) + ")", 0, 0, 1);
    // exception-safe snapshot so failed callbacks can't corrupt VM control-flow stacks.
    Bytecode savedCodeState = code_;
    std::vector<std::string> savedStrState = stringConstants_;
    std::vector<Value> savedValState = valueConstants_;
    std::vector<ValuePtr> savedStackState = stack;
    std::vector<ValuePtr> savedLocalsState = locals_;
    std::vector<CallFrame> savedCallFramesState = callFrames;
    std::vector<std::vector<ValuePtr>> savedFrameLocalsState = frameLocals_;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> savedDeferState = deferStack;
    std::vector<StackFrame> savedCallStackState = callStack;
    std::vector<std::pair<ValuePtr, size_t>> savedIterState = iterStack;
    std::vector<size_t> savedTryState = tryStack;
    std::vector<ExceptionFrame> savedExceptionState = exceptionStack;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>, std::string>> savedCodeFramesState =
        codeFrameStack;
    std::shared_ptr<ScriptCode> savedCurScriptState = currentScript;
    std::string savedActivePathState = activeSourcePath_;
    int savedUnsafeDepthState = unsafeDepth_;
    size_t savedFrames = callFrames.size();
    size_t savedIp = ip_;
    try {
        push(callee);
        for (const auto& a : args) push(a);
        Instruction callInst(Opcode::CALL, args.size());
        runInstruction(callInst);
        // match main run(): after each instruction (including CALL), ip_ advances. For entryPoint==0, ip_ was SIZE_MAX; ++ wraps to 0.
        if (callFrames.size() > savedFrames) ip_++;
        size_t guard = 0;
        const size_t maxGuard = callbackStepGuard_ == 0
            ? std::max<size_t>(
                  500000,
                  code_.empty() ? 4096 : (code_.size() * 64 + 4096))
            : static_cast<size_t>(callbackStepGuard_);
        while (callFrames.size() > savedFrames && ip_ < code_.size()) {
            const Instruction& inst = code_[ip_];
            runInstruction(inst);
            if (++guard > maxGuard) break;
            ip_++;
            // Break on coroutine yield — OP_YIELD already saved state via saveCurrentCoroutineState()
            if (coroutineYieldRequested_) {
                break;
            }
        }

        // safety: if callback did not unwind naturally, restore the caller frame.
        while (callFrames.size() > savedFrames) {
            ip_ = callFrames.back().returnPc - reinterpret_cast<const uint8_t*>(0);
            callFrames.pop_back();
            if (!frameLocals_.empty()) {
                locals_ = std::move(frameLocals_.back());
                frameLocals_.pop_back();
            }
            if (!deferStack.empty()) deferStack.pop_back();
            if (!callStack.empty()) callStack.pop_back();
            if (!codeFrameStack.empty()) {
                auto t = std::move(codeFrameStack.back());
                codeFrameStack.pop_back();
                code_ = std::move(std::get<0>(t));
                stringConstants_ = std::move(std::get<1>(t));
                valueConstants_ = std::move(std::get<2>(t));
                activeSourcePath_ = std::move(std::get<3>(t));
            }
            push(std::make_shared<Value>(Value::nil()));
        }
        ip_ = savedIp;
        if (stack.empty()) return std::make_shared<Value>(Value::nil());
        ValuePtr result = popStack();
        return result;
    } catch (...) {
        code_ = std::move(savedCodeState);
        stringConstants_ = std::move(savedStrState);
        valueConstants_ = std::move(savedValState);
        stack = std::move(savedStackState);
        locals_ = std::move(savedLocalsState);
        callFrames = std::move(savedCallFramesState);
        frameLocals_ = std::move(savedFrameLocalsState);
        deferStack = std::move(savedDeferState);
        callStack = std::move(savedCallStackState);
        iterStack = std::move(savedIterState);
        tryStack = std::move(savedTryState);
        exceptionStack = std::move(savedExceptionState);
        codeFrameStack = std::move(savedCodeFramesState);
        currentScript = std::move(savedCurScriptState);
        activeSourcePath_ = std::move(savedActivePathState);
        unsafeDepth_ = savedUnsafeDepthState;
        ip_ = savedIp;
        throw;
    }
}

void VM::runDeferredCalls() {
    if (deferStack.empty()) return;
    auto& list = deferStack.back();
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
                deferStack.push_back({});
                size_t savedFrames = callFrames.size();
                callFrames.push_back(CallFrame{reinterpret_cast<const uint8_t*>(ip_), nullptr, stack.size(), fn->name, "", 0, 0});
                frameLocals_.push_back(std::move(locals_));
                locals_.clear();
                for (size_t i = 0; i < args.size(); ++i)
                    locals_.push_back(ensureNonNull(ValuePtr(args[i])));
                while (locals_.size() < fn->arity)
                    locals_.push_back(std::make_shared<Value>(Value::nil()));
                // match main run(): after CALL, ip_ is entryPoint-1 then incremented to entryPoint before executing.
                ip_ = fn->entryPoint - 1;
                while (callFrames.size() > savedFrames) {
                    ip_++;
                    if (ip_ >= code_.size()) break;
                    runInstruction(code_[ip_]);
                }
                if (!stack.empty()) popStack();  // discard deferred user function return value
            }
        }
    }
}
// ─── Cooperative coroutine scheduler ──────────────────────────────────
// Iterates all YIELDED coroutines, restoring state and running each
// until they yield again or complete.  This is a single round-robin
// pass — the host should call resumeAll() in a loop to keep coroutines
// alive across multiple frames.
void VM::resumeAll(uint64_t currentTimeMs) {
    currentTimeMs_ = currentTimeMs;

    // Lazily ensure coroutine 0 (the main coroutine) exists
    if (coroutines_.empty()) {
        coroutines_.resize(1);
        saveCurrentCoroutineState();
        coroutines_[0].state = CoroutineState::RUNNING;
    }

    for (auto& cor : coroutines_) {
        if (cor.state == CoroutineState::DEAD) continue;

        // Track which coroutine is active
        activeCoroutineId_ = static_cast<size_t>(&cor - coroutines_.data());

        // Skip coroutine 0 (the main thread).  Its state was saved
        // temporarily by startCoroutine() but the main-thread execution
        // has already run to completion via VM::run().  Restoring and
        // resuming it would re-execute the tail of the main script.
        if (activeCoroutineId_ == 0) continue;

        // ⏱ Time-aware scheduling: if this coroutine asked to sleep, check
        // whether the host clock has advanced far enough.
        if (cor.state == CoroutineState::YIELDED && cor.wakeTimestampMs > 0) {
            if (currentTimeMs < cor.wakeTimestampMs) {
                continue;  // Not ready yet — try again on the next tick
            }
            // Ready to wake up — clear the timer
            cor.wakeTimestampMs = 0;
        }

        // 📂 Async I/O future check: if this coroutine is waiting on a
        // background file read, check whether the future has completed.
        if (cor.state == CoroutineState::YIELDED && cor.pendingStringTask.has_value()) {
            if (cor.pendingStringTask->wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                continue;  // Still reading — skip this coroutine for now
            }
            // Future is ready — extract the result string
            std::string result;
            try {
                result = cor.pendingStringTask->get();
            } catch (...) {
                result = "[kern_fs_read_async error]";
            }
            cor.pendingStringTask.reset();
            // Replace the top of the saved stack (the nil that was returned by
            // the builtin before it yielded) with the actual file contents,
            // so the Kern script receives the correct return value.
            if (!cor.stack.empty()) {
                cor.stack.back() = std::make_shared<Value>(Value::fromString(result));
            }
            // The coroutine will be restored below and resume normally.
        }

        restoreCoroutineState(cor);

        // Clear the yield flag before running
        coroutineYieldRequested_ = false;

        // Run until yield or completion
        try {
            while (ip_ < code_.size()) {
                const Instruction& inst = code_[ip_];

                if (stack.size() > 10000) {
                    throw VMError("Stack overflow - too many values on stack", inst.line, inst.column, 1,
                                  static_cast<int>(VMErrorCode::STACK_OVERFLOW));
                }

                runInstruction(inst);

                ip_++;
                if (scriptExitCode_ >= 0) break;
                if (coroutineYieldRequested_) {
                    coroutineYieldRequested_ = false;
                    cor.state = CoroutineState::YIELDED;
                    // OP_YIELD saved ip_ at the YIELD instruction, but ip_++
                    // above has advanced past it.  Re-save so that the next
                    // resume continues at the instruction *after* YIELD
                    // instead of re-executing YIELD forever.
                    // For builtin-triggered yields (e.g. kern_sleep), this also
                    // saves the state with ip_ past the call_builtin instruction.
                    saveCurrentCoroutineState();
                    break;
                }
            }
        } catch (...) {
            saveCurrentCoroutineState();
            cor.state = CoroutineState::DEAD;
            throw;
        }

        // Check if this coroutine finished (didn't yield)
        if (ip_ >= code_.size() || scriptExitCode_ >= 0) {
            saveCurrentCoroutineState();
            cor.state = CoroutineState::DEAD;
        }
        // If yielded, state was already saved by the yield check above (or by OP_YIELD + the re-save)
    }
}

// ─── sleepCurrentCoroutine: cooperative sleep via time-aware scheduling ──
// Called from the kern_sleep builtin.  Sets the active coroutine's
// wakeTimestampMs and signals a yield so resumeAll() breaks out.
// The time-aware check in resumeAll() will skip this coroutine until
// the host clock advances past wakeTimestampMs.
void VM::sleepCurrentCoroutine(uint64_t ms) {
    if (activeCoroutineId_ >= coroutines_.size()) return;
    if (activeCoroutineId_ == 0) return;  // Cannot sleep the main thread

    uint64_t wakeTime = currentTimeMs_ + ms;
    coroutines_[activeCoroutineId_].wakeTimestampMs = wakeTime;
    coroutines_[activeCoroutineId_].state = CoroutineState::YIELDED;
    coroutineYieldRequested_ = true;
}

// ─── startAsyncFileRead: non-blocking file read via std::async ──────────
// Called from the kern_fs_read_async builtin.  Launches a background
// thread to read the file, stores the future, and yields the coroutine.
void VM::startAsyncFileRead(const std::string& path) {
    if (activeCoroutineId_ >= coroutines_.size()) return;
    if (activeCoroutineId_ == 0) return;  // Cannot async-read on the main thread

    auto& cor = coroutines_[activeCoroutineId_];
    cor.pendingStringTask = std::async(std::launch::async, [path]() -> std::string {
        // Read the entire file on a background thread
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return std::string();
        }
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string content;
        content.resize(static_cast<size_t>(size));
        file.read(&content[0], size);
        return content;
    });

    // Yield this coroutine until the future completes
    cor.state = CoroutineState::YIELDED;
    coroutineYieldRequested_ = true;
}

// ─── Async HTTP GET ────────────────────────────────────────────────────
// Launches a background thread to perform an HTTP GET request, stores
// the future in the active coroutine's pendingStringTask, and yields.
void VM::startAsyncHttpGet(const std::string& url) {
    if (activeCoroutineId_ >= coroutines_.size()) return;
    if (activeCoroutineId_ == 0) return;  // Cannot async-http on main thread

    auto& cor = coroutines_[activeCoroutineId_];
    cor.pendingStringTask = std::async(std::launch::async, [url]() -> std::string {
#ifdef _WIN32
        return kernHttpGetWinHttp(url);
#else
        // On non-Windows, fall back to empty (curl/powershell could be added)
        (void)url;
        return std::string();
#endif
    });

    // Yield this coroutine until the HTTP response arrives
    cor.state = CoroutineState::YIELDED;
    coroutineYieldRequested_ = true;
}

// ─── Async HTTP POST ───────────────────────────────────────────────────
// Launches a background thread to perform an HTTP POST request, stores
// the future in the active coroutine's pendingStringTask, and yields.
void VM::startAsyncHttpPost(const std::string& url, const std::string& payload) {
    if (activeCoroutineId_ >= coroutines_.size()) return;
    if (activeCoroutineId_ == 0) return;  // Cannot async-http on main thread

    auto& cor = coroutines_[activeCoroutineId_];
    cor.pendingStringTask = std::async(std::launch::async, [url, payload]() -> std::string {
#ifdef _WIN32
        return kernHttpPostWinHttp(url, payload);
#else
        // On non-Windows, fall back to empty (curl/powershell could be added)
        (void)url;
        (void)payload;
        return std::string();
#endif
    });

    // Yield this coroutine until the HTTP response arrives
    cor.state = CoroutineState::YIELDED;
    coroutineYieldRequested_ = true;
}

// ─── Hot reload ────────────────────────────────────────────────────────
// Kills all active coroutines (except the main thread), resets execution
// state, loads new bytecode, and re-runs the top-level script so that
// global function definitions are rebound.  After this call the host
// should continue its resumeAll() loop as normal.
void VM::hotReload(const Bytecode& code,
                   const std::vector<std::string>& stringConstants,
                   const std::vector<Value>& valueConstants) {
    // ── 1. Kill all coroutines except coroutine 0 (main thread) ──────────
    for (size_t i = 1; i < coroutines_.size(); ++i) {
        auto& cor = coroutines_[i];
        cor.state = CoroutineState::DEAD;
        // Free all saved state from the old bytecode run
        cor.code.clear();
        cor.stringConstants.clear();
        cor.valueConstants.clear();
        cor.stack.clear();
        cor.locals.clear();
        cor.callFrames.clear();
        cor.callStack.clear();
        cor.frameLocals.clear();
        cor.deferStack.clear();
        cor.iterStack.clear();
        cor.tryStack.clear();
        cor.exceptionStack.clear();
        cor.codeFrameStack.clear();
        cor.currentScript.reset();
        cor.wakeTimestampMs = 0;
        cor.pendingStringTask.reset();  // Abandon any in-flight async I/O
    }

    // ── 2. Reset VM execution state ──────────────────────────────────────
    scriptExitCode_ = -1;
    ip_ = 0;
    stack.clear();
    callFrames.clear();
    callStack.clear();
    frameLocals_.clear();
    deferStack.clear();
    iterStack.clear();
    tryStack.clear();
    exceptionStack.clear();
    codeFrameStack.clear();
    currentScript.reset();
    locals_.clear();
    unsafeDepth_ = 0;
    coroutineYieldRequested_ = false;
    pendingYield_ = false;
    doneGenerator_ = false;
    inGeneratorExecution_ = false;
    activeGenerator.reset();

    // ── 3. Load new bytecode and constants ───────────────────────────────
    code_ = code;
    stringConstants_ = stringConstants;
    valueConstants_ = valueConstants;
    activeSourcePath_.clear();

    // ── 4. Re-execute top-level script to rebind globals ─────────────────
    // Clear exit code in case the previous script called exit()
    scriptExitCode_ = -1;
    run();
}
// ───────────────────────────────────────────────────────────────────────

// Production-optimized VM execution loop with crash protection

// 
// Performance notes:
// - Pre-reserved vectors avoid allocations
// - No heap allocations in hot path
// - Switch-based dispatch is branch-prediction friendly
// - vmTraceEnabled_ only checked in debug builds (constexpr optimization possible)
// - All errors are caught and reported safely - engine never crashes
void VM::run() {
    if (code_.empty()) return;
    try {
        verifyBytecodeOrThrow(code_, stringConstants_.size(), valueConstants_.size());
    } catch (const VMError& e) {
        std::cerr << "[Kern] Bytecode Verification Error [" << e.category_ << "]: " << e.what()
                  << " at line " << e.line_ << ", column " << e.column_ << std::endl;
        scriptExitCode_ = e.category_;
        return;
    } catch (...) {
        std::cerr << "[Kern] Unknown bytecode verification error" << std::endl;
        scriptExitCode_ = 1;
        return;
    }
    
    resetCycleCount();
    
    // Pre-reserve to prevent reallocations in hot path
    if (stack.capacity() < 512) stack.reserve(512);
    
    try {
        while (ip_ < code_.size()) {
            const Instruction& inst = code_[ip_];
            
            // Check for stack overflow
            if (stack.size() > 10000) {
                throw VMError("Stack overflow - too many values on stack", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::STACK_OVERFLOW));
            }
            
            runInstruction(inst);
            
#if defined(KERN_DEBUG) && defined(KERN_DEBUG_VM_TRACE)
            if (vmTraceEnabled_ && cycleCount_ <= 500000u) {
                std::cerr << "[vm] op=" << static_cast<int>(inst.op) 
                          << " line=" << inst.line << " col=" << inst.column
                          << " sp=" << stack.size() << "\n";
            }
#endif
            ip_++;
            if (scriptExitCode_ >= 0) break;
            if (coroutineYieldRequested_) {
                coroutineYieldRequested_ = false;
                // State was saved by OP_YIELD → saveCurrentCoroutineState();
                break;
            }
        }

    } catch (const VMError& e) {
        // Format and print runtime error with full context
        std::cerr << "[Kern] Runtime Error [" << e.category_ << "]: " << e.what();
        if (e.line_ > 0) {
            std::cerr << " at line " << e.line_;
            if (e.column_ > 0) {
                std::cerr << ", column " << e.column_;
            }
        }
        std::cerr << std::endl;
        scriptExitCode_ = e.category_;
        
        // Attach traceback to any error value on stack
        if (!stack.empty() && stack.back()) {
            attachTracebackToError(stack.back());
        }
    } catch (const std::exception& e) {
        std::cerr << "[Kern] Internal Error: " << e.what() << std::endl;
        scriptExitCode_ = 1;
    } catch (...) {
        std::cerr << "[Kern] Unknown internal error occurred" << std::endl;
        scriptExitCode_ = 1;
    }
}

bool VM::hasActiveCoroutines() const {
    for (const auto& cor : coroutines_) {
        if (cor.state == CoroutineState::YIELDED || cor.state == CoroutineState::RUNNING)
            return true;
    }
    return false;
}

// ─── startCoroutine: bypass generator call path ─────────────────────────
// The codegen marks ANY function containing `yield` as a generator via
// SET_FUNC_GENERATOR.  When CALL sees fn->isGenerator, it creates a
// GeneratorObject instead of executing the function body.  This method
// lets the host (or a builtin) start a yield-capable function as a
// cooperative coroutine directly, skipping the generator branch.
size_t VM::startCoroutine(FunctionPtr fn, std::vector<ValuePtr> args) {
    // Ensure coroutine 0 (main thread) exists
    if (coroutines_.empty()) {
        coroutines_.resize(1);
        saveCurrentCoroutineState();
        coroutines_[0].state = CoroutineState::RUNNING;
    }

    // Save current coroutine state before switching
    saveCurrentCoroutineState();

    // Create new coroutine entry
    size_t corId = coroutines_.size();
    coroutines_.emplace_back();
    Coroutine& cor = coroutines_[corId];
    cor.state = CoroutineState::YIELDED;  // start as YIELDED so resumeAll() picks it up

    // Set up function bytecode context
    if (fn->script) {
        cor.code = fn->script->code;
        cor.stringConstants = fn->script->stringConstants;
        cor.valueConstants = fn->script->valueConstants;
        cor.currentScript = fn->script;
        cor.activeSourcePath = fn->script->sourcePath;
    } else {
        cor.code = code_;
        cor.stringConstants = stringConstants_;
        cor.valueConstants = valueConstants_;
        cor.currentScript = currentScript;
        cor.activeSourcePath = activeSourcePath_;
    }

    // Starting instruction pointer = function entry point
    cor.ip = fn->entryPoint;

    // Set up locals: args first, then nil-pad to arity, then captures
    cor.locals.clear();
    for (const auto& a : args)
        cor.locals.push_back(ensureNonNull(a));
    while (cor.locals.size() < fn->arity)
        cor.locals.push_back(std::make_shared<Value>(Value::nil()));
    for (const auto& c : fn->captures)
        cor.locals.push_back(ensureNonNull(c ? c : std::make_shared<Value>(Value::nil())));

    // All execution stacks start empty for a fresh coroutine
    cor.stack.clear();
    cor.callFrames.clear();
    cor.callStack.clear();
    cor.frameLocals.clear();
    cor.deferStack.clear();
    cor.iterStack.clear();
    cor.tryStack.clear();
    cor.exceptionStack.clear();
    cor.codeFrameStack.clear();
    cor.unsafeDepth = 0;
    cor.yieldedValue = nullptr;

    return corId;
}
// ─────────────────────────────────────────────────────────────────────────

void VM::runSubScript(Bytecode code, std::vector<std::string> stringConstants, std::vector<Value> valueConstants,
                      const std::string& sourcePath) {
    auto savedScript = currentScript;
    std::string savedActive = activeSourcePath_;
    int savedExitCode = scriptExitCode_;
    scriptExitCode_ = -1;  // imported scripts must not terminate parent script
    currentScript = std::make_shared<ScriptCode>();
    currentScript->code = std::move(code);
    currentScript->stringConstants = std::move(stringConstants);
    currentScript->valueConstants = std::move(valueConstants);
    currentScript->sourcePath = sourcePath;
    
    try {
        verifyBytecodeOrThrow(currentScript->code, currentScript->stringConstants.size(), currentScript->valueConstants.size());
    } catch (const VMError& e) {
        std::cerr << "[Kern] Import Error [" << e.category_ << "]: " << e.what()
                  << " in " << sourcePath << " at line " << e.line_ << std::endl;
        scriptExitCode_ = savedExitCode;
        return;
    }
    
    Bytecode savedCode = std::move(code_);
    std::vector<std::string> savedStr = std::move(stringConstants_);
    std::vector<Value> savedVal = std::move(valueConstants_);
    size_t savedIp = ip_;
    code_ = currentScript->code;
    stringConstants_ = currentScript->stringConstants;
    valueConstants_ = currentScript->valueConstants;
    activeSourcePath_ = sourcePath;
    ip_ = 0;
    
    try {
        while (ip_ < code_.size()) {
            const Instruction& inst = code_[ip_];
            
            // Check for stack overflow in subscript
            if (stack.size() > 10000) {
                throw VMError("Stack overflow in imported script", inst.line, inst.column, 1,
                              static_cast<int>(VMErrorCode::STACK_OVERFLOW));
            }
            
            runInstruction(inst);
            ip_++;
            if (scriptExitCode_ >= 0) break;
        }
    } catch (const VMError& e) {
        std::cerr << "[Kern] Runtime Error in " << sourcePath << " [" << e.category_ << "]: " << e.what();
        if (e.line_ > 0) {
            std::cerr << " at line " << e.line_;
        }
        std::cerr << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Kern] Internal Error in " << sourcePath << ": " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[Kern] Unknown error in imported script: " << sourcePath << std::endl;
    }
    
    code_ = std::move(savedCode);
    stringConstants_ = std::move(savedStr);
    valueConstants_ = std::move(savedVal);
    ip_ = savedIp;
    activeSourcePath_ = std::move(savedActive);
    currentScript = savedScript;
    scriptExitCode_ = savedExitCode;
}

void VM::resetCycleCount() {
    cycleCount_ = 0;
}

std::vector<StackFrame> VM::getCallStackSlice(size_t start, size_t count) const {
    std::vector<StackFrame> result;
    
    // Bounds checking
    if (start >= callStack.size()) {
        return result;
    }
    
    // Calculate end index
    size_t end = start + count;
    if (end > callStack.size()) {
        end = callStack.size();
    }
    
    // Extract slice
    for (size_t i = start; i < end; ++i) {
        const auto& frame = callStack[i];
        std::string frameStr = frame.functionName + " at " + frame.filePath + ":" + std::to_string(frame.line);
        // Note: We're returning StackFrame objects, not strings, as per the method signature
        result.push_back(frame);
    }
    
    return result;
}

void VM::setTracing(bool enabled) {
    vmTraceEnabled_ = enabled;
}

void VM::requestStop() {
    stopRequested = true;
}

bool VM::isStopped() const {
    return stopRequested;
}

bool VM::isInUnsafeContext() const {
    return unsafeDepth_ > 0;
}

size_t VM::unsafeDepth() const {
    return static_cast<size_t>(unsafeDepth_);
}

size_t VM::getCallStackDepth() const {
    return callStack.size();
}

uint64_t VM::getCycleCount() const {
    return cycleCount_;
}

void VM::setActiveSourcePath(const std::string& path) {
    activeSourcePath_ = path;
}

bool VM::hasResult() const {
    return scriptExitCode_ >= 0;
}

const std::vector<std::string>& VM::getCliArgs() const {
    return cliArgs;
}

void VM::setCliArgs(std::vector<std::string> args) {
    cliArgs = std::move(args);
}

void VM::setStepLimit(uint64_t limit) {
    stepLimit_ = limit;
}

uint64_t VM::getStepLimit() const {
    return stepLimit_;
}

void VM::setMaxCallDepth(size_t depth) {
    maxCallDepth_ = depth;
}

size_t VM::getMaxCallDepth() const {
    return maxCallDepth_;
}

void VM::setCallbackStepGuard(uint64_t enabled) {
    callbackStepGuard_ = enabled;
}

uint64_t VM::getCallbackStepGuard() const {
    return callbackStepGuard_;
}

void VM::setScriptExitCode(int64_t code) {
    scriptExitCode_ = code;
}

int64_t VM::getScriptExitCode() const {
    return scriptExitCode_;
}

RuntimeGuardPolicy VM::getRuntimeGuards() const {
    return runtimeGuards_;
}

RuntimeGuardPolicy& VM::mutableRuntimeGuards() {
    return runtimeGuards_;
}

void VM::setRuntimeGuards(RuntimeGuardPolicy policy) {
    runtimeGuards_ = policy;
}

ValuePtr VM::getDecoratorRegistry() const {
    return decoratorRegistry;
}

void VM::setDecoratorRegistry(ValuePtr registry) {
    decoratorRegistry = std::move(registry);
}

kern::ValuePtr VM::getResult() const {
    if (stack.empty()) return std::make_shared<Value>(Value::nil());
    ValuePtr v = stack.back();
    return v;
}

Result<void> VM::loadBytecode(const Bytecode& code,
                               const std::vector<std::string>& stringPool,
                               const std::vector<Value>& valuePool) {
    setBytecode(code);
    setStringConstants(stringPool);
    setValueConstants(valuePool);
    return {};
}

bool VM::loadBytecodeFromFile(const std::string& path) {
    Bytecode bc;
    std::vector<std::string> strPool;
    std::vector<Value> valPool;
    if (!kern::loadBytecodeFromFile(path, bc, strPool, valPool)) {
        return false;
    }
    setBytecode(std::move(bc));
    setStringConstants(std::move(strPool));
    setValueConstants(std::move(valPool));
    return true;
}

} // namespace kern
