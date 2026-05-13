/* *
 * kern Bytecode - Instruction set for the VM
 * 
 * This file defines the stable bytecode format for KERN releases.
 * For binary format specification, see bytecode_header.hpp
 */

#ifndef KERN_BYTECODE_HPP
#define KERN_BYTECODE_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "bytecode_header.hpp"

namespace kern {

/* * kBytecodeSchemaVersion is defined in bytecode_header.hpp (included above) */

enum class Opcode : uint8_t {
    // constants
    CONST_I64,
    CONST_F64,
    CONST_STR,
    CONST_TRUE,
    CONST_FALSE,
    CONST_NULL,

    // variables
    LOAD,
    STORE,
    LOAD_GLOBAL,
    STORE_GLOBAL,

    // stack
    POP,
    DUP,

    // arithmetic
    ADD, SUB, MUL, DIV, MOD, POW,
    NEG,
    // comparison
    EQ, NE, LT, LE, GT, GE,
    // logical
    AND, OR, NOT,
    // bitwise
    BIT_AND, BIT_OR, BIT_XOR, SHL, SHR,

    // control flow
    JMP,
    JMP_IF_FALSE,
    JMP_IF_TRUE,
    LOOP,

    // error handling
    TRY_BEGIN,
    TRY_END,
    THROW,
    RETHROW,  // rethrow current exception (push lastThrown_, then throw)
    DEFER,   // operand = number of stack values (callee + args); push onto current frame's defer list

    // iteration
    FOR_IN_NEXT,
    FOR_IN_ITER,

    // functions
    CALL,
    RETURN,
    BUILD_FUNC,
    BUILD_CLOSURE,   // operand = pair(entry, captureCount); pop captureCount values, attach as captures, push function
    SET_FUNC_ARITY,  // operand = param count; pop function, set arity, push
    SET_FUNC_PARAM_NAMES,  // operand = string constant index "a,b,c"; pop function, set paramNames, push
    SET_FUNC_NAME,   // operand = string constant index; pop function, set name, push
    SET_FUNC_GENERATOR,  // pop function, mark isGenerator, push
    SET_FUNC_STRUCT,     // pop function, mark isStructConstructor, push

    YIELD,  // pop value, suspend generator (only valid inside generator execution)

    // objects & slicing
    NEW_OBJECT,
    BUILD_ARRAY,   // pop N values, push array of those values
    SPREAD,     // pop spread_array, pop acc_array; push acc_array concatenated with spread_array
    GET_FIELD,
    SET_FIELD,
    GET_INDEX,
    ARRAY_LEN,  // pop array, push int length (0 if not array)
    SET_INDEX,
    SLICE,
    NEW_INSTANCE,
    INVOKE_METHOD,
    LOAD_THIS,

    // Vec3 (minimal implementation)
    BUILD_VEC3,      // pop z, y, x (floats), push Vec3
    VEC3_GET_X,      // pop Vec3, push x (float)
    VEC3_GET_Y,      // pop Vec3, push y (float)
    VEC3_GET_Z,      // pop Vec3, push z (float)

    // memory / DMA
    ALLOC,
    FREE,
    MEM_COPY,

    // builtins
    PRINT,
    BUILTIN,  // index of builtin

    // meta
    NOP,
    UNSAFE_BEGIN,
    UNSAFE_END,
    HALT
};

/* * human-readable opcode name for disassembly (--bytecode) and tooling. Keep in sync with Opcode.*/
inline const char* opcodeName(Opcode op) {
    switch (op) {
        case Opcode::CONST_I64: return "CONST_I64";
        case Opcode::CONST_F64: return "CONST_F64";
        case Opcode::CONST_STR: return "CONST_STR";
        case Opcode::CONST_TRUE: return "CONST_TRUE";
        case Opcode::CONST_FALSE: return "CONST_FALSE";
        case Opcode::CONST_NULL: return "CONST_NULL";
        case Opcode::LOAD: return "LOAD";
        case Opcode::STORE: return "STORE";
        case Opcode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case Opcode::STORE_GLOBAL: return "STORE_GLOBAL";
        case Opcode::POP: return "POP";
        case Opcode::DUP: return "DUP";
        case Opcode::ADD: return "ADD";
        case Opcode::SUB: return "SUB";
        case Opcode::MUL: return "MUL";
        case Opcode::DIV: return "DIV";
        case Opcode::MOD: return "MOD";
        case Opcode::POW: return "POW";
        case Opcode::NEG: return "NEG";
        case Opcode::EQ: return "EQ";
        case Opcode::NE: return "NE";
        case Opcode::LT: return "LT";
        case Opcode::LE: return "LE";
        case Opcode::GT: return "GT";
        case Opcode::GE: return "GE";
        case Opcode::AND: return "AND";
        case Opcode::OR: return "OR";
        case Opcode::NOT: return "NOT";
        case Opcode::BIT_AND: return "BIT_AND";
        case Opcode::BIT_OR: return "BIT_OR";
        case Opcode::BIT_XOR: return "BIT_XOR";
        case Opcode::SHL: return "SHL";
        case Opcode::SHR: return "SHR";
        case Opcode::JMP: return "JMP";
        case Opcode::JMP_IF_FALSE: return "JMP_IF_FALSE";
        case Opcode::JMP_IF_TRUE: return "JMP_IF_TRUE";
        case Opcode::LOOP: return "LOOP";
        case Opcode::TRY_BEGIN: return "TRY_BEGIN";
        case Opcode::TRY_END: return "TRY_END";
        case Opcode::THROW: return "THROW";
        case Opcode::RETHROW: return "RETHROW";
        case Opcode::DEFER: return "DEFER";
        case Opcode::FOR_IN_NEXT: return "FOR_IN_NEXT";
        case Opcode::FOR_IN_ITER: return "FOR_IN_ITER";
        case Opcode::CALL: return "CALL";
        case Opcode::RETURN: return "RETURN";
        case Opcode::BUILD_FUNC: return "BUILD_FUNC";
        case Opcode::BUILD_CLOSURE: return "BUILD_CLOSURE";
        case Opcode::SET_FUNC_ARITY: return "SET_FUNC_ARITY";
        case Opcode::SET_FUNC_PARAM_NAMES: return "SET_FUNC_PARAM_NAMES";
        case Opcode::SET_FUNC_NAME: return "SET_FUNC_NAME";
        case Opcode::SET_FUNC_GENERATOR: return "SET_FUNC_GENERATOR";
        case Opcode::SET_FUNC_STRUCT: return "SET_FUNC_STRUCT";
        case Opcode::YIELD: return "YIELD";
        case Opcode::NEW_OBJECT: return "NEW_OBJECT";
        case Opcode::BUILD_ARRAY: return "BUILD_ARRAY";
        case Opcode::SPREAD: return "SPREAD";
        case Opcode::GET_FIELD: return "GET_FIELD";
        case Opcode::SET_FIELD: return "SET_FIELD";
        case Opcode::GET_INDEX: return "GET_INDEX";
        case Opcode::ARRAY_LEN: return "ARRAY_LEN";
        case Opcode::SET_INDEX: return "SET_INDEX";
        case Opcode::SLICE: return "SLICE";
        case Opcode::NEW_INSTANCE: return "NEW_INSTANCE";
        case Opcode::INVOKE_METHOD: return "INVOKE_METHOD";
        case Opcode::LOAD_THIS: return "LOAD_THIS";
        case Opcode::BUILD_VEC3: return "BUILD_VEC3";
        case Opcode::VEC3_GET_X: return "VEC3_GET_X";
        case Opcode::VEC3_GET_Y: return "VEC3_GET_Y";
        case Opcode::VEC3_GET_Z: return "VEC3_GET_Z";
        case Opcode::ALLOC: return "ALLOC";
        case Opcode::FREE: return "FREE";
        case Opcode::MEM_COPY: return "MEM_COPY";
        case Opcode::PRINT: return "PRINT";
        case Opcode::BUILTIN: return "BUILTIN";
        case Opcode::NOP: return "NOP";
        case Opcode::UNSAFE_BEGIN: return "UNSAFE_BEGIN";
        case Opcode::UNSAFE_END: return "UNSAFE_END";
        case Opcode::HALT: return "HALT";
        default: return "?";
    }
}

struct Instruction {
    Opcode op;
    int line = 0;
    /** Source column (1-based when set by codegen); 0 = unknown / legacy bytecode. */
    int column = 0;
    std::variant<std::monostate, int64_t, double, std::string, size_t, std::pair<size_t, size_t>> operand;

    Instruction(Opcode o) : op(o) {}
    Instruction(Opcode o, int64_t v) : op(o), operand(v) {}
    Instruction(Opcode o, double v) : op(o), operand(v) {}
    Instruction(Opcode o, std::string v) : op(o), operand(std::move(v)) {}
    Instruction(Opcode o, size_t v) : op(o), operand(v) {}
    Instruction(Opcode o, size_t a, size_t b) : op(o), operand(std::pair<size_t, size_t>(a, b)) {}
};

using Bytecode = std::vector<Instruction>;

/* *
 * operand text for bytecode listings (tab-separated line is built by caller).
 * resolves string-pool indices for opcodes that store identifiers/literals in constants.
 * includes pair<size_t,size_t> operands (e.g. FOR_IN_NEXT key/value local slots).
 */
inline std::string formatBytecodeOperandSuffix(const Instruction& inst,
                                               const std::vector<std::string>& constants) {
    if (inst.operand.index() == 0) return "";
    if (inst.operand.index() == 1)
        return " " + std::to_string(std::get<int64_t>(inst.operand));
    if (inst.operand.index() == 2) {
        std::string s = std::to_string(std::get<double>(inst.operand));
        return " " + s;
    }
    if (inst.operand.index() == 3) {
        const std::string& lit = std::get<std::string>(inst.operand);
        return " \"" + lit + "\"";
    }
    if (inst.operand.index() == 4) {
        size_t idx = std::get<size_t>(inst.operand);
        if (inst.op == Opcode::CONST_STR || inst.op == Opcode::LOAD_GLOBAL || inst.op == Opcode::STORE_GLOBAL ||
            inst.op == Opcode::GET_FIELD || inst.op == Opcode::SET_FIELD || inst.op == Opcode::SET_FUNC_NAME ||
            inst.op == Opcode::SET_FUNC_PARAM_NAMES) {
            if (idx < constants.size()) return " " + constants[idx];
            return " ?";
        }
        return " " + std::to_string(idx);
    }
    if (inst.operand.index() == 5) {
        const auto& p = std::get<std::pair<size_t, size_t>>(inst.operand);
        return " " + std::to_string(p.first) + "," + std::to_string(p.second);
    }
    return "";
}

} // namespace kern

#endif // kERN_BYTECODE_HPP
