/* *
 * kern VM Implementation
 */

#include "vm.hpp"
#include <sstream>
#include <string>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace kern {

VM::VM() : ip_(0) { initBuiltins(); }

void VM::setBytecode(Bytecode code) { code_ = std::move(code); ip_ = 0; }

void VM::setStringConstants(std::vector<std::string> constants) { stringConstants_ = std::move(constants); }

void VM::setValueConstants(std::vector<Value> constants) { valueConstants_ = std::move(constants); }

void VM::registerBuiltin(size_t index, BuiltinFn fn) {
    builtins_[index] = std::move(fn);
    if (index < 256u) {
        if (builtinsVec_.size() <= index) builtinsVec_.resize(index + 1);
        builtinsVec_[index] = builtins_[index];
    }
}

void VM::setGlobal(const std::string& name, ValuePtr value) {
    globals_[name] = std::move(value);
}

ValuePtr VM::getGlobal(const std::string& name) const {
    auto it = globals_.find(name);
    return it != globals_.end() ? it->second : nullptr;
}

std::unordered_map<std::string, ValuePtr> VM::getGlobalsSnapshot() const {
    return globals_;
}

ValuePtr VM::popStack() {
    if (stack_.empty()) throw VMError("Stack underflow", 0);
    ValuePtr v = stack_.back();
    stack_.pop_back();
    return v;
}

ValuePtr VM::getResult() {
    if (stack_.empty()) return std::make_shared<Value>(Value::nil());
    return stack_.back();
}

std::string VM::getOperandStr(const Instruction& inst) {
    if (!std::holds_alternative<size_t>(inst.operand)) return "";
    size_t idx = std::get<size_t>(inst.operand);
    if (idx >= stringConstants_.size()) return "";
    return stringConstants_[idx];
}

size_t VM::getOperandU64(const Instruction& inst) {
    if (!std::holds_alternative<size_t>(inst.operand)) return 0;
    return std::get<size_t>(inst.operand);
}

void VM::push(ValuePtr v) { stack_.push_back(std::move(v)); }

ValuePtr VM::peek() {
    if (stack_.empty()) throw VMError("Stack underflow");
    return stack_.back();
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
    if (v->type == Value::Type::FLOAT) return static_cast<int64_t>(std::get<double>(v->data));
    return 0;
}

static void* toPtr(ValuePtr v) {
    if (!v || v->type != Value::Type::PTR) return nullptr;
    return std::get<void*>(v->data);
}

void VM::runInstruction(const Instruction& inst) {
    ++cycleCount_;
    if (stepLimit_ != 0 && cycleCount_ > stepLimit_)
        throw VMError("Step limit exceeded", inst.line);
    switch (inst.op) {
        case Opcode::CONST_I64:
            push(std::make_shared<Value>(Value::fromInt(std::get<int64_t>(inst.operand))));
            break;
        case Opcode::CONST_F64:
            push(std::make_shared<Value>(Value::fromFloat(std::get<double>(inst.operand))));
            break;
        case Opcode::CONST_STR: {
            size_t idx = std::holds_alternative<size_t>(inst.operand) ? std::get<size_t>(inst.operand) : 0;
            push(std::make_shared<Value>(Value::fromString(idx < stringConstants_.size() ? stringConstants_[idx] : "")));
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
            size_t slot = static_cast<size_t>(std::get<int64_t>(inst.operand));
            if (slot < locals_.size() && locals_[slot]) push(locals_[slot]);
            else push(std::make_shared<Value>(Value::nil()));
            break;
        }
        case Opcode::STORE: {
            size_t slot = static_cast<size_t>(std::get<int64_t>(inst.operand));
            while (locals_.size() <= slot) locals_.push_back(nullptr);
            locals_[slot] = popStack();
            break;
        }
        case Opcode::LOAD_GLOBAL: {
            std::string name = getOperandStr(inst);
            auto it = globals_.find(name);
            ValuePtr v = (it != globals_.end() && it->second) ? it->second : std::make_shared<Value>(Value::nil());
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
                int64_t off = toInt(b);
                push(std::make_shared<Value>(Value::fromPtr(static_cast<char*>(p) + off)));
            } else if (a->type == Value::Type::INT && b->type == Value::Type::PTR) {
                int64_t off = toInt(a);
                void* p = toPtr(b);
                push(std::make_shared<Value>(Value::fromPtr(static_cast<char*>(p) + off)));
            } else if (a->type == Value::Type::FLOAT || b->type == Value::Type::FLOAT)
                push(std::make_shared<Value>(Value::fromFloat(toDouble(a) + toDouble(b))));
            else
                push(std::make_shared<Value>(Value::fromInt(toInt(a) + toInt(b))));
            break;
        }
        case Opcode::SUB: {
            ValuePtr b = popStack(), a = popStack();
            if (a->type == Value::Type::PTR && b->type == Value::Type::INT) {
                void* p = toPtr(a);
                int64_t off = toInt(b);
                push(std::make_shared<Value>(Value::fromPtr(static_cast<char*>(p) - off)));
            } else if (a->type == Value::Type::PTR && b->type == Value::Type::PTR) {
                char* pa = static_cast<char*>(toPtr(a));
                char* pb = static_cast<char*>(toPtr(b));
                push(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(pa - pb))));
            } else
                push(std::make_shared<Value>(Value::fromFloat(toDouble(a) - toDouble(b))));
            break;
        }
        case Opcode::MUL: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) * toDouble(b))));
            break;
        }
        case Opcode::DIV: {
            ValuePtr b = popStack(), a = popStack();
            double den = toDouble(b);
            if (den == 0) throw VMError("Division by zero", inst.line, 0, 4);
            push(std::make_shared<Value>(Value::fromFloat(toDouble(a) / den)));
            break;
        }
        case Opcode::MOD: {
            ValuePtr b = popStack(), a = popStack();
            int64_t den = toInt(b);
            if (den == 0) throw VMError("Division by zero", inst.line, 0, 4);
            push(std::make_shared<Value>(Value::fromInt(toInt(a) % den)));
            break;
        }
        case Opcode::POW: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromFloat(std::pow(toDouble(a), toDouble(b)))));
            break;
        }
        case Opcode::NEG: {
            ValuePtr v = popStack();
            if (!v) { push(std::make_shared<Value>(Value::nil())); break; }
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
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) < toDouble(b))));
            break;
        }
        case Opcode::LE: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) <= toDouble(b))));
            break;
        }
        case Opcode::GT: {
            ValuePtr b = popStack(), a = popStack();
            push(std::make_shared<Value>(Value::fromBool(toDouble(a) > toDouble(b))));
            break;
        }
        case Opcode::GE: {
            ValuePtr b = popStack(), a = popStack();
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
        case Opcode::BIT_AND: { ValuePtr b = popStack(), a = popStack(); push(std::make_shared<Value>(Value::fromInt(toInt(a) & toInt(b)))); break; }
        case Opcode::BIT_OR: { ValuePtr b = popStack(), a = popStack(); push(std::make_shared<Value>(Value::fromInt(toInt(a) | toInt(b)))); break; }
        case Opcode::BIT_XOR: { ValuePtr b = popStack(), a = popStack(); push(std::make_shared<Value>(Value::fromInt(toInt(a) ^ toInt(b)))); break; }
        case Opcode::SHL: { ValuePtr b = popStack(), a = popStack(); int64_t sh = toInt(b) & 63; push(std::make_shared<Value>(Value::fromInt(toInt(a) << sh))); break; }
        case Opcode::SHR: { ValuePtr b = popStack(), a = popStack(); int64_t sh = toInt(b) & 63; push(std::make_shared<Value>(Value::fromInt(toInt(a) >> sh))); break; }
        case Opcode::JMP: {
            size_t target = getOperandU64(inst);
            if (target == 0 || target > code_.size()) throw VMError("Invalid jump target", inst.line);
            ip_ = target - 1;
            break;
        }
        case Opcode::JMP_IF_FALSE: {
            ValuePtr v = popStack();
            if (!v || !v->isTruthy()) {
                size_t target = getOperandU64(inst);
                if (target == 0 || target > code_.size()) throw VMError("Invalid jump target", inst.line);
                ip_ = target - 1;
            }
            break;
        }
        case Opcode::JMP_IF_TRUE: {
            ValuePtr v = popStack();
            if (v && v->isTruthy()) {
                size_t target = getOperandU64(inst);
                if (target == 0 || target > code_.size()) throw VMError("Invalid jump target", inst.line);
                ip_ = target - 1;
            }
            break;
        }
        case Opcode::CALL: {
            size_t argc = getOperandU64(inst);
            if (stack_.size() < argc + 1)
                throw VMError("Stack underflow in call (not enough arguments)", inst.line, 0, 5);
            std::vector<ValuePtr> args;
            for (size_t i = 0; i < argc; ++i) args.push_back(popStack());
            std::reverse(args.begin(), args.end());
            ValuePtr callee = popStack();
            if (!callee) throw VMError("Call on null", inst.line, 0, 2);
            if (callee->type == Value::Type::FUNCTION) {
                auto& fn = std::get<FunctionPtr>(callee->data);
                if (fn->isBuiltin) {
                    BuiltinFn* fast = (fn->builtinIndex < builtinsVec_.size()) ? &builtinsVec_[fn->builtinIndex] : nullptr;
                    if (fast && *fast)
                        push(std::make_shared<Value>((*fast)(this, args)));
                    else {
                        auto it = builtins_.find(fn->builtinIndex);
                        if (it != builtins_.end()) push(std::make_shared<Value>(it->second(this, args)));
                        else push(std::make_shared<Value>(Value::nil()));
                    }
                } else {
                    bool tailCall = (ip_ + 1 < code_.size() && code_[ip_ + 1].op == Opcode::RETURN)
                        && !deferStack_.empty() && deferStack_.back().empty();
                    // if function was defined in an imported script, switch to its bytecode for the call
                    if (fn->script) {
                        codeFrameStack_.push_back(std::make_tuple(std::move(code_), std::move(stringConstants_), std::move(valueConstants_)));
                        code_ = fn->script->code;
                        stringConstants_ = fn->script->stringConstants;
                        valueConstants_ = fn->script->valueConstants;
                    }
                    if (tailCall) {
                        callStack_.back() = {fn->name.empty() ? "<anonymous>" : fn->name, inst.line, 0};
                        locals_.clear();
                        for (size_t i = 0; i < args.size(); ++i) locals_.push_back(args[i]);
                        while (locals_.size() < fn->arity) locals_.push_back(std::make_shared<Value>(Value::nil()));
                        ip_ = fn->entryPoint - 1;
                    } else {
                        deferStack_.push_back({});
                        callStack_.push_back({fn->name.empty() ? "<anonymous>" : fn->name, inst.line, 0});
                        callFrames_.push_back(ip_);
                        frameLocals_.push_back(std::move(locals_));
                        locals_.clear();
                        for (size_t i = 0; i < args.size(); ++i)
                            locals_.push_back(args[i]);
                        while (locals_.size() < fn->arity)
                            locals_.push_back(std::make_shared<Value>(Value::nil()));
                        ip_ = fn->entryPoint - 1;
                    }
                }
            } else {
                push(std::make_shared<Value>(Value::nil()));
            }
            break;
        }
        case Opcode::DEFER: {
            size_t n = getOperandU64(inst);
            if (stack_.size() < n || deferStack_.empty())
                break;
            std::vector<ValuePtr> args;
            for (size_t i = 0; i < n - 1; ++i) args.push_back(popStack());
            std::reverse(args.begin(), args.end());
            ValuePtr callee = popStack();
            deferStack_.back().emplace_back(std::move(callee), std::move(args));
            break;
        }
        case Opcode::RETURN: {
            ValuePtr result = stack_.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            if (callFrames_.empty()) throw VMError("Return outside function", inst.line, 0, 1);
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
            }
            push(result);
            break;
        }
        case Opcode::BUILD_FUNC: {
            size_t entry = getOperandU64(inst);
            if (entry >= code_.size()) throw VMError("Invalid function entry point", inst.line);
            auto fn = std::make_shared<FunctionObject>();
            fn->entryPoint = entry;
            fn->arity = 0;
            if (currentScript_) {
                fn->script = currentScript_;
            } else {
                if (!entryScriptCache_) {
                    entryScriptCache_ = std::make_shared<ScriptCode>();
                    entryScriptCache_->code = code_;
                    entryScriptCache_->stringConstants = stringConstants_;
                    entryScriptCache_->valueConstants = valueConstants_;
                }
                fn->script = entryScriptCache_;
            }
            push(std::make_shared<Value>(Value::fromFunction(fn)));
            break;
        }
        case Opcode::SET_FUNC_ARITY: {
            size_t arity = getOperandU64(inst);
            ValuePtr v = popStack();
            if (v && v->type == Value::Type::FUNCTION)
                std::get<FunctionPtr>(v->data)->arity = arity;
            push(std::move(v));
            break;
        }
        case Opcode::SET_FUNC_PARAM_NAMES: {
            std::string joined = getOperandStr(inst);
            ValuePtr v = popStack();
            if (v && v->type == Value::Type::FUNCTION) {
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
            if (v && v->type == Value::Type::FUNCTION)
                std::get<FunctionPtr>(v->data)->name = name;
            push(std::move(v));
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
                throw VMError("Array size too large", inst.line, 0, 1);
            if (stack_.size() < n)
                throw VMError("Stack underflow building array (need " + std::to_string(n) + " values)", inst.line, 0, 1);
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
            if (!accVal || accVal->type != Value::Type::ARRAY) { push(std::move(accVal)); push(std::move(spreadVal)); break; }
            if (!spreadVal || spreadVal->type != Value::Type::ARRAY) { push(std::move(accVal)); push(std::move(spreadVal)); break; }
            auto& acc = std::get<std::vector<ValuePtr>>(accVal->data);
            const auto& sp = std::get<std::vector<ValuePtr>>(spreadVal->data);
            for (const auto& v : sp) acc.push_back(v);
            push(accVal);
            break;
        }
        case Opcode::GET_FIELD: {
            ValuePtr obj = popStack();
            std::string field = getOperandStr(inst);
            if (obj && obj->type == Value::Type::MAP) {
                auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(obj->data);
                auto it = m.find(field);
                if (it != m.end()) push(it->second);
                else {
                    auto proto = m.find("__class");
                    if (proto != m.end() && proto->second && proto->second->type == Value::Type::MAP) {
                        auto& cm = std::get<std::unordered_map<std::string, ValuePtr>>(proto->second->data);
                        auto cit = cm.find(field);
                        if (cit != cm.end()) push(cit->second);
                        else push(std::make_shared<Value>(Value::nil()));
                    } else push(std::make_shared<Value>(Value::nil()));
                }
            } else push(std::make_shared<Value>(Value::nil()));
            break;
        }
        case Opcode::SET_FIELD: {
            ValuePtr val = popStack();
            ValuePtr obj = popStack();
            std::string field = getOperandStr(inst);
            if (obj && obj->type == Value::Type::MAP)
                std::get<std::unordered_map<std::string, ValuePtr>>(obj->data)[field] = val;
            push(val);
            break;
        }
        case Opcode::GET_INDEX: {
            ValuePtr index = popStack(), obj = popStack();
            if (obj && obj->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
                int64_t raw = toInt(index);
                size_t i = static_cast<size_t>(raw >= 0 ? raw : std::max(int64_t(0), raw + static_cast<int64_t>(arr.size())));
                if (i < arr.size()) push(arr[i]);
                else push(std::make_shared<Value>(Value::nil()));
            } else if (obj && obj->type == Value::Type::STRING) {
                const std::string& s = std::get<std::string>(obj->data);
                int64_t raw = toInt(index);
                int64_t len = static_cast<int64_t>(s.size());
                int64_t i = (raw >= 0) ? raw : raw + len;
                if (i >= 0 && i < len)
                    push(std::make_shared<Value>(Value::fromString(s.substr(static_cast<size_t>(i), 1))));
                else
                    push(std::make_shared<Value>(Value::nil()));
            } else if (obj && obj->type == Value::Type::MAP && index && index->type == Value::Type::STRING) {
                auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(obj->data);
                auto it = m.find(std::get<std::string>(index->data));
                if (it != m.end()) push(it->second);
                else push(std::make_shared<Value>(Value::nil()));
            } else push(std::make_shared<Value>(Value::nil()));
            break;
        }
        case Opcode::SET_INDEX: {
            ValuePtr val = popStack(), index = popStack(), obj = popStack();
            if (obj && obj->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
                int64_t raw = toInt(index);
                if (raw < 0) raw += static_cast<int64_t>(arr.size());
                if (raw < 0) raw = 0;
                const size_t kMaxArraySize = 64 * 1024 * 1024;
                size_t i = static_cast<size_t>(raw);
                if (i > kMaxArraySize)
                    throw VMError("Array index out of range", inst.line, 0, 6);
                while (arr.size() <= i) arr.push_back(std::make_shared<Value>(Value::nil()));
                arr[i] = val;
            } else if (obj && obj->type == Value::Type::MAP && index && index->type == Value::Type::STRING) {
                std::get<std::unordered_map<std::string, ValuePtr>>(obj->data)[std::get<std::string>(index->data)] = val;
            }
            push(val);
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
            break;
        }
        case Opcode::TRY_BEGIN: {
            size_t catchAddr = getOperandU64(inst);
            tryStack_.push_back(catchAddr);
            break;
        }
        case Opcode::TRY_END:
            if (!tryStack_.empty()) tryStack_.pop_back();
            break;
        case Opcode::THROW: {
            ValuePtr val = stack_.empty() ? std::make_shared<Value>(Value::nil()) : popStack();
            if (tryStack_.empty()) throw VMError(val ? val->toString() : "exception", inst.line, 0, 1);
            lastThrown_ = val;
            size_t catchAddr = tryStack_.back();
            tryStack_.pop_back();
            if (catchAddr == 0 || catchAddr > code_.size()) throw VMError("Invalid catch target", inst.line, 0, 1);
            push(val);
            ip_ = catchAddr - 1;
            break;
        }
        case Opcode::RETHROW: {
            ValuePtr val = lastThrown_ ? lastThrown_ : std::make_shared<Value>(Value::nil());
            if (tryStack_.empty()) throw VMError(val ? val->toString() : "exception", inst.line, 0, 1);
            size_t catchAddr = tryStack_.back();
            tryStack_.pop_back();
            if (catchAddr == 0 || catchAddr > code_.size()) throw VMError("Invalid catch target", inst.line, 0, 1);
            push(val);
            ip_ = catchAddr - 1;
            break;
        }
        case Opcode::SLICE: {
            ValuePtr stepVal = popStack(), endVal = popStack(), startVal = popStack(), obj = popStack();
            if (!obj || obj->type != Value::Type::ARRAY) { push(std::make_shared<Value>(Value::fromArray({}))); break; }
            auto& arr = std::get<std::vector<ValuePtr>>(obj->data);
            int64_t len = static_cast<int64_t>(arr.size());
            int64_t start = (startVal && startVal->type != Value::Type::NIL) ? toInt(startVal) : 0;
            int64_t end = (endVal && endVal->type != Value::Type::NIL) ? toInt(endVal) : len;
            int64_t step = (stepVal && stepVal->type != Value::Type::NIL) ? toInt(stepVal) : 1;
            if (step == 0) step = 1;
            if (start < 0) start = std::max(int64_t(0), start + len);
            if (end < 0) end = std::max(int64_t(0), end + len);
            if (end > len) end = len;
            std::vector<ValuePtr> out;
            if (step > 0) for (int64_t i = start; i < end; i += step) out.push_back(arr[static_cast<size_t>(i)]);
            else if (step < 0) for (int64_t i = start; i > end; i += step) { if (i >= 0 && i < len) out.push_back(arr[static_cast<size_t>(i)]); }
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
            if (!v) { iterStack_.pop_back(); push(std::make_shared<Value>(Value::fromBool(false))); break; }
            if (v->type == Value::Type::ARRAY) {
                auto& arr = std::get<std::vector<ValuePtr>>(v->data);
                if (i < arr.size()) {
                    while (locals_.size() <= slot1) locals_.push_back(nullptr);
                    locals_[slot1] = arr[i];
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
                if (i < keys.size()) {
                    while (locals_.size() <= slot1) locals_.push_back(nullptr);
                    locals_[slot1] = std::make_shared<Value>(Value::fromString(keys[i]));
                    if (slot2 != static_cast<size_t>(-1)) {
                        while (locals_.size() <= slot2) locals_.push_back(nullptr);
                        locals_[slot2] = m[keys[i]] ? m[keys[i]] : std::make_shared<Value>(Value::nil());
                    }
                    i++;
                    push(std::make_shared<Value>(Value::fromBool(true)));
                } else {
                    iterStack_.pop_back();
                    push(std::make_shared<Value>(Value::fromBool(false)));
                }
            } else {
                iterStack_.pop_back();
                push(std::make_shared<Value>(Value::fromBool(false)));
            }
            break;
        }
        case Opcode::ALLOC: {
            int64_t n = toInt(popStack());
            if (n <= 0) { push(std::make_shared<Value>(Value::fromPtr(nullptr))); break; }
            const size_t kMaxAlloc = 256 * 1024 * 1024;  // 256 miB
            size_t sz = static_cast<size_t>(n);
            if (sz > kMaxAlloc)
                throw VMError("Allocation size too large", inst.line, 0, 1);
            void* p = std::malloc(sz);
            push(std::make_shared<Value>(Value::fromPtr(p)));
            break;
        }
        case Opcode::FREE: {
            ValuePtr v = popStack();
            if (v && v->type == Value::Type::PTR) {
                void* p = std::get<void*>(v->data);
                if (p) std::free(p);
            }
            break;
        }
        case Opcode::MEM_COPY: {
            ValuePtr vn = popStack(), vsrc = popStack(), vdst = popStack();
            if (!vdst || vdst->type != Value::Type::PTR || !vsrc || vsrc->type != Value::Type::PTR) break;
            void* dest = std::get<void*>(vdst->data);
            void* src = std::get<void*>(vsrc->data);
            size_t n = static_cast<size_t>(std::max(int64_t(0), toInt(vn)));
            if (dest && src) std::memcpy(dest, src, n);
            break;
        }
        case Opcode::HALT:
            ip_ = code_.size();
            break;
        default:
            break;
    }
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
    size_t savedFrames = callFrames_.size();
    push(callee);
    for (const auto& a : args) push(a);
    Instruction callInst(Opcode::CALL, args.size());
    runInstruction(callInst);
    while (callFrames_.size() > savedFrames && ip_ < code_.size()) {
        ip_++;
        runInstruction(code_[ip_ - 1]);
    }
    if (stack_.empty()) return std::make_shared<Value>(Value::nil());
    ValuePtr result = popStack();
    return result;
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
                    if (it != builtins_.end()) push(std::make_shared<Value>(it->second(this, args)));
                    else push(std::make_shared<Value>(Value::nil()));
                }
                popStack();
            } else {
                deferStack_.push_back({});
                size_t savedFrames = callFrames_.size();
                callFrames_.push_back(ip_);
                frameLocals_.push_back(std::move(locals_));
                locals_.clear();
                for (size_t i = 0; i < args.size(); ++i) locals_.push_back(args[i]);
                while (locals_.size() < fn->arity)
                    locals_.push_back(std::make_shared<Value>(Value::nil()));
                ip_ = fn->entryPoint - 1;
                while (callFrames_.size() > savedFrames && ip_ < code_.size()) {
                    ip_++;
                    runInstruction(code_[ip_ - 1]);
                }
                if (!stack_.empty()) popStack();  // discard deferred user function return value
            }
        }
    }
}

void VM::run() {
    if (code_.empty()) return;
    resetCycleCount();
    stack_.reserve(512);
    while (ip_ < code_.size()) {
        const Instruction& inst = code_[ip_];
        runInstruction(inst);
        ip_++;
        if (scriptExitCode_ >= 0) break;
    }
}

void VM::runSubScript(Bytecode code, std::vector<std::string> stringConstants, std::vector<Value> valueConstants) {
    auto savedScript = currentScript_;
    int savedExitCode = scriptExitCode_;
    scriptExitCode_ = -1;  // imported scripts must not terminate parent script
    currentScript_ = std::make_shared<ScriptCode>(ScriptCode{std::move(code), std::move(stringConstants), std::move(valueConstants)});
    Bytecode savedCode = std::move(code_);
    std::vector<std::string> savedStr = std::move(stringConstants_);
    std::vector<Value> savedVal = std::move(valueConstants_);
    size_t savedIp = ip_;
    code_ = currentScript_->code;
    stringConstants_ = currentScript_->stringConstants;
    valueConstants_ = currentScript_->valueConstants;
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
    currentScript_ = savedScript;
    scriptExitCode_ = savedExitCode;
}

} // namespace kern
