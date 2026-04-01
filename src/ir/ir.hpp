#ifndef KERN_IR_IR_HPP
#define KERN_IR_IR_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace kern {

enum class TypedIROp {
    Nop,
    Assign,
    Call,
    Return,
    Branch,
    Import,
    Phi
};

struct TypedIRInstruction {
    TypedIROp op = TypedIROp::Nop;
    std::string dst;
    std::string lhs;
    std::string rhs;
    int line = 0;
};

struct TypedIRBasicBlock {
    std::string id;
    std::vector<TypedIRInstruction> instructions;
    std::vector<std::string> successors;
};

struct TypedIRFunction {
    std::string name;
    std::string returnType = "dynamic";
    std::vector<std::string> params;
    std::vector<TypedIRBasicBlock> blocks;
};

struct TypedIRModule {
    std::string path;
    std::vector<TypedIRFunction> functions;
    std::vector<std::string> imports;
};

struct IRModule {
    std::string path;
    std::string source;
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, int64_t> foldedConstants;
    TypedIRModule typed;
};

struct IRProgram {
    std::vector<IRModule> modules;
    std::vector<TypedIRModule> typedModules;
    std::unordered_map<std::string, std::string> diagnostics;
};

} // namespace kern

#endif // kERN_IR_IR_HPP
