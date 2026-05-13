/* *
 * kern/runtime/vm_minimal.hpp - Minimal Working VM
 * 
 * A simplified VM that works with the refactored Value system.
 * Supports: print, variables, arithmetic, loops, functions
 */
#pragma once

#include "core/value_refactored.hpp"
#include <vector>
#include <unordered_map>
#include <functional>
#include <iostream>

namespace kern {

// Simplified instruction format
struct Instruction {
    enum Op : uint8_t {
        NOP = 0,
        PUSH_CONST,    // operand: constant index
        PUSH_NIL,
        PUSH_TRUE,
        PUSH_FALSE,
        POP,
        LOAD_LOCAL,    // operand: local index
        STORE_LOCAL,   // operand: local index
        LOAD_GLOBAL,   // operand: constant index (name)
        STORE_GLOBAL,  // operand: constant index (name)
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        NEG,
        EQ,
        LT,
        LE,
        GT,
        GE,
        NOT,
        JUMP,          // operand: offset
        JUMP_IF_FALSE, // operand: offset
        CALL,          // operand: argc
        RETURN,
        PRINT,         // Print top of stack
        HALT
    };
    
    Op op;
    int16_t operand;  // Optional operand
};

using Bytecode = std::vector<Instruction>;

// Minimal VM
class MinimalVM {
    std::vector<Value> stack;
    std::vector<Value> locals;
    std::unordered_map<std::string, Value> globals;
    std::unordered_map<std::string, Bytecode> functions;
    std::vector<std::string> constants;
    size_t ip;
    bool running;
    
public:
    MinimalVM() : ip(0), running(false) {
        // Pre-reserve stack
        stack.reserve(256);
        locals.reserve(64);
    }
    
    // Add a constant string
    size_t addConstant(const std::string& s) {
        constants.push_back(s);
        return constants.size() - 1;
    }
    
    // Define a function
    void defineFunction(const std::string& name, const Bytecode& code) {
        functions[name] = code;
    }
    
    // Set global
    void setGlobal(const std::string& name, const Value& val) {
        globals[name] = val;
    }
    
    // Get global
    Value getGlobal(const std::string& name) const {
        auto it = globals.find(name);
        if (it != globals.end()) return it->second;
        return Value::nil();
    }
    
    // Execute bytecode
    Result<Value> execute(const Bytecode& code) {
        ip = 0;
        running = true;
        
        while (running && ip < code.size()) {
            const auto& inst = code[ip];
            
            switch (inst.op) {
                case Instruction::NOP:
                    break;
                    
                case Instruction::PUSH_CONST: {
                    if (inst.operand >= 0 && inst.operand < (int)constants.size()) {
                        stack.push_back(Value(constants[inst.operand]));
                    } else {
                        stack.push_back(Value::nil());
                    }
                    break;
                }
                    
                case Instruction::PUSH_NIL:
                    stack.push_back(Value::nil());
                    break;
                    
                case Instruction::PUSH_TRUE:
                    stack.push_back(Value::fromBool(true));
                    break;
                    
                case Instruction::PUSH_FALSE:
                    stack.push_back(Value::fromBool(false));
                    break;
                    
                case Instruction::POP:
                    if (!stack.empty()) stack.pop_back();
                    break;
                    
                case Instruction::LOAD_LOCAL: {
                    if (inst.operand >= 0 && inst.operand < (int)locals.size()) {
                        stack.push_back(locals[inst.operand]);
                    } else {
                        stack.push_back(Value::nil());
                    }
                    break;
                }
                    
                case Instruction::STORE_LOCAL: {
                    if (!stack.empty()) {
                        if (inst.operand >= (int)locals.size()) {
                            locals.resize(inst.operand + 1);
                        }
                        locals[inst.operand] = stack.back();
                        stack.pop_back();
                    }
                    break;
                }
                    
                case Instruction::LOAD_GLOBAL: {
                    if (inst.operand >= 0 && inst.operand < (int)constants.size()) {
                        auto it = globals.find(constants[inst.operand]);
                        if (it != globals.end()) {
                            stack.push_back(it->second);
                        } else {
                            stack.push_back(Value::nil());
                        }
                    }
                    break;
                }
                    
                case Instruction::STORE_GLOBAL: {
                    if (!stack.empty() && inst.operand >= 0 && inst.operand < (int)constants.size()) {
                        globals[constants[inst.operand]] = stack.back();
                        stack.pop_back();
                    }
                    break;
                }
                    
                case Instruction::ADD: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        if (a.isInt() && b.isInt()) {
                            stack.push_back(Value::fromInt(a.asInt() + b.asInt()));
                        } else {
                            stack.push_back(Value::fromFloat(a.asFloat() + b.asFloat()));
                        }
                    }
                    break;
                }
                    
                case Instruction::SUB: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        if (a.isInt() && b.isInt()) {
                            stack.push_back(Value::fromInt(a.asInt() - b.asInt()));
                        } else {
                            stack.push_back(Value::fromFloat(a.asFloat() - b.asFloat()));
                        }
                    }
                    break;
                }
                    
                case Instruction::MUL: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        if (a.isInt() && b.isInt()) {
                            stack.push_back(Value::fromInt(a.asInt() * b.asInt()));
                        } else {
                            stack.push_back(Value::fromFloat(a.asFloat() * b.asFloat()));
                        }
                    }
                    break;
                }
                    
                case Instruction::DIV: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        if (b.asFloat() == 0) {
                            return Result<Value>(ErrorValue{1, "Division by zero", {}});
                        }
                        stack.push_back(Value::fromFloat(a.asFloat() / b.asFloat()));
                    }
                    break;
                }
                    
                case Instruction::MOD: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        if (b.asInt() == 0) {
                            return Result<Value>(ErrorValue{1, "Division by zero", {}});
                        }
                        stack.push_back(Value::fromInt(a.asInt() % b.asInt()));
                    }
                    break;
                }
                    
                case Instruction::NEG: {
                    if (!stack.empty()) {
                        Value a = stack.back(); stack.pop_back();
                        if (a.isInt()) {
                            stack.push_back(Value::fromInt(-a.asInt()));
                        } else {
                            stack.push_back(Value::fromFloat(-a.asFloat()));
                        }
                    }
                    break;
                }
                    
                case Instruction::EQ: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        stack.push_back(Value::fromBool(a == b));
                    }
                    break;
                }
                    
                case Instruction::LT: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        stack.push_back(Value::fromBool(a.asFloat() < b.asFloat()));
                    }
                    break;
                }
                    
                case Instruction::LE: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        stack.push_back(Value::fromBool(a.asFloat() <= b.asFloat()));
                    }
                    break;
                }
                    
                case Instruction::GT: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        stack.push_back(Value::fromBool(a.asFloat() > b.asFloat()));
                    }
                    break;
                }
                    
                case Instruction::GE: {
                    if (stack.size() >= 2) {
                        Value b = stack.back(); stack.pop_back();
                        Value a = stack.back(); stack.pop_back();
                        stack.push_back(Value::fromBool(a.asFloat() >= b.asFloat()));
                    }
                    break;
                }
                    
                case Instruction::NOT: {
                    if (!stack.empty()) {
                        Value a = stack.back(); stack.pop_back();
                        stack.push_back(Value::fromBool(!a.isTruthy()));
                    }
                    break;
                }
                    
                case Instruction::JUMP:
                    ip += inst.operand;
                    continue;  // Skip ip++
                    
                case Instruction::JUMP_IF_FALSE: {
                    if (!stack.empty()) {
                        Value cond = stack.back(); stack.pop_back();
                        if (!cond.isTruthy()) {
                            ip += inst.operand;
                            continue;
                        }
                    }
                    break;
                }
                    
                case Instruction::CALL: {
                    // For now, just call native functions
                    if (!stack.empty()) {
                        Value func = stack.back();
                        if (func.isNativeFunction()) {
                            std::vector<Value> args;
                            for (int i = 0; i < inst.operand && !stack.empty(); i++) {
                                args.push_back(stack.back());
                                stack.pop_back();
                            }
                            stack.pop_back();  // Pop function
                            std::reverse(args.begin(), args.end());
                            Value result = func.call(args);
                            stack.push_back(result);
                        }
                    }
                    break;
                }
                    
                case Instruction::RETURN: {
                    if (!stack.empty()) {
                        return Result<Value>(stack.back());
                    }
                    return Result<Value>(Value::nil());
                }
                    
                case Instruction::PRINT: {
                    if (!stack.empty()) {
                        std::cout << stack.back().toString() << std::endl;
                        stack.pop_back();
                    }
                    break;
                }
                    
                case Instruction::HALT:
                    running = false;
                    break;
            }
            
            ip++;
        }
        
        return Result<Value>(Value::nil());
    }
    
    // Get stack size for debugging
    size_t getStackSize() const { return stack.size(); }
    
    // Print stack for debugging
    void printStack() const {
        std::cout << "Stack (" << stack.size() << "): ";
        for (const auto& v : stack) {
            std::cout << v.toString() << " ";
        }
        std::cout << std::endl;
    }
};

} // namespace kern
