/* *
 * kern Bytecode - Instruction set for the VM
 */

#ifndef KERN_BYTECODE_HPP
#define KERN_BYTECODE_HPP

#include <cstdint>
#include <utility>
#include <vector>
#include <string>
#include <variant>

namespace kern {

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
    SET_FUNC_ARITY,  // operand = param count; pop function, set arity, push
    SET_FUNC_PARAM_NAMES,  // operand = string constant index "a,b,c"; pop function, set paramNames, push
    SET_FUNC_NAME,   // operand = string constant index; pop function, set name, push

    // objects & slicing
    NEW_OBJECT,
    BUILD_ARRAY,   // pop N values, push array of those values
    SPREAD,     // pop spread_array, pop acc_array; push acc_array concatenated with spread_array
    GET_FIELD,
    SET_FIELD,
    GET_INDEX,
    SET_INDEX,
    SLICE,
    NEW_INSTANCE,
    INVOKE_METHOD,
    LOAD_THIS,

    // memory / DMA
    ALLOC,
    FREE,
    MEM_COPY,

    // builtins
    PRINT,
    BUILTIN,  // index of builtin

    // meta
    NOP,
    HALT
};

struct Instruction {
    Opcode op;
    int line = 0;
    std::variant<std::monostate, int64_t, double, std::string, size_t, std::pair<size_t, size_t>> operand;

    Instruction(Opcode o) : op(o) {}
    Instruction(Opcode o, int64_t v) : op(o), operand(v) {}
    Instruction(Opcode o, double v) : op(o), operand(v) {}
    Instruction(Opcode o, std::string v) : op(o), operand(std::move(v)) {}
    Instruction(Opcode o, size_t v) : op(o), operand(v) {}
    Instruction(Opcode o, size_t a, size_t b) : op(o), operand(std::pair<size_t, size_t>(a, b)) {}
};

using Bytecode = std::vector<Instruction>;

} // namespace kern

#endif // kERN_BYTECODE_HPP
