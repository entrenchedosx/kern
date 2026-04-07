/* *
 * kern Code Generator - AST to Bytecode
 */

#ifndef KERN_CODEGEN_HPP
#define KERN_CODEGEN_HPP

#include "ast.hpp"
#include "bytecode/bytecode.hpp"
#include "bytecode/value.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdexcept>

namespace kern {

class CodegenError : public std::runtime_error {
public:
    int line;
    explicit CodegenError(const std::string& msg, int ln) : std::runtime_error(msg), line(ln) {}
};

class CodeGenerator {
public:
    CodeGenerator();
    Bytecode generate(std::unique_ptr<Program> program);
    const std::vector<std::string>& getConstants() const { return stringConstants_; }
    const std::vector<Value>& getValueConstants() const { return valueConstants_; }

private:
    Bytecode code_;
    int currentLine_ = 0;
    int currentColumn_ = 0;
    std::vector<std::string> stringConstants_;
    std::vector<Value> valueConstants_;
    std::unordered_map<std::string, size_t> globals_;
    std::vector<std::unordered_map<std::string, size_t>> scopes_;
    std::vector<size_t> loopEndStack_;
    std::vector<size_t> loopStartStack_;
    std::vector<std::string> loopLabels_;
    std::vector<std::vector<size_t>> breakPatches_;
    std::vector<std::vector<size_t>> continuePatches_;
    std::unordered_map<std::string, std::vector<std::string>> functionParams_;

    struct LambdaCodegenLayer {
        size_t captureBoundary = 0;  // scopes_.size() before lambda beginScope
        size_t arity = 0;
        std::vector<std::string> captureOrder;
        std::unordered_map<std::string, size_t> captureIndex;
        size_t nextInnerSlot = 0;
    };
    std::vector<LambdaCodegenLayer> lambdaCtxStack_;

    size_t addConstant(const std::string& s);
    size_t addValueConstant(Value v);
    size_t emit(Opcode op);
    size_t emit(Opcode op, int64_t arg);
    size_t emit(Opcode op, double arg);
    size_t emit(Opcode op, const std::string& arg);
    size_t emit(Opcode op, size_t arg);
    size_t emit(Opcode op, size_t a, size_t b);
    void patchJump(size_t at, size_t target);
    size_t resolveLocal(const std::string& name);
    int findDefiningScopeIndex(const std::string& name) const;
    bool tryResolveLocalSlot(const std::string& name, int64_t* outSlot);
    void scanLambdaCapturesStmt(const Stmt* s, size_t boundary, std::unordered_set<std::string>& seen,
                                std::vector<std::string>& order);
    void scanLambdaCapturesExpr(const Expr* e, size_t boundary, std::unordered_set<std::string>& seen,
                                std::vector<std::string>& order);
    void scanAssignTargetForCaptures(const Expr* t, size_t boundary, std::unordered_set<std::string>& seen,
                                     std::vector<std::string>& order);
    size_t totalLocalCount();
    void declareLocal(const std::string& name);
    void beginScope();
    void endScope();

    void emitExpr(const Expr* e);
    void emitStmt(const Stmt* s);
    void emitProgram(const Program* p);
    void emitFunctionOntoStack(const FunctionDeclStmt* x);

    bool tryConstantFoldBinary(const BinaryExpr* x);
    bool tryConstantFoldUnary(const UnaryExpr* x);
};

} // namespace kern

#endif // kERN_CODEGEN_HPP
