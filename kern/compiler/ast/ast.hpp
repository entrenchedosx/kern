/* *
 * kern Abstract Syntax Tree (AST)
 * node types for the parser output.
 */

#ifndef KERN_AST_HPP
#define KERN_AST_HPP

#include "token.hpp"
#include <memory>
#include <variant>
#include <vector>
#include <string>

namespace kern {

// forward declarations
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// expression nodes ---
struct Expr {
    virtual ~Expr() = default;
    int line = 0, column = 0;
};

struct IntLiteral : Expr {
    int64_t value;
    IntLiteral(int64_t v) : value(v) {}
};

struct FloatLiteral : Expr {
    double value;
    FloatLiteral(double v) : value(v) {}
};

struct StringLiteral : Expr {
    std::string value;
    StringLiteral(std::string v) : value(std::move(v)) {}
};

struct BoolLiteral : Expr {
    bool value;
    BoolLiteral(bool v) : value(v) {}
};

struct NullLiteral : Expr {};

struct Identifier : Expr {
    std::string name;
    Identifier(std::string n) : name(std::move(n)) {}
};

struct BinaryExpr : Expr {
    ExprPtr left;
    TokenType op;
    ExprPtr right;
    BinaryExpr(ExprPtr l, TokenType o, ExprPtr r) : left(std::move(l)), op(o), right(std::move(r)) {}
};

struct UnaryExpr : Expr {
    TokenType op;
    ExprPtr operand;
    UnaryExpr(TokenType o, ExprPtr e) : op(o), operand(std::move(e)) {}
};

// argument: optional name (empty = positional), expression
struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<std::pair<std::string, ExprPtr>> args;  // (name or "", expr)
    CallExpr(ExprPtr c, std::vector<std::pair<std::string, ExprPtr>> a) : callee(std::move(c)), args(std::move(a)) {}
};

struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    IndexExpr(ExprPtr o, ExprPtr i) : object(std::move(o)), index(std::move(i)) {}
};

// safe optional indexing: arr?[i] -> null if arr is null or out of bounds
struct OptionalIndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    OptionalIndexExpr(ExprPtr o, ExprPtr i) : object(std::move(o)), index(std::move(i)) {}
};

struct MemberExpr : Expr {
    ExprPtr object;
    std::string member;
    MemberExpr(ExprPtr o, std::string m) : object(std::move(o)), member(std::move(m)) {}
};

struct AssignExpr : Expr {
    ExprPtr target;
    ExprPtr value;
    TokenType op;  // aSSIGN, PLUS_EQ, etc.
    AssignExpr(ExprPtr t, ExprPtr v, TokenType o = TokenType::ASSIGN) : target(std::move(t)), value(std::move(v)), op(o) {}
};

struct LambdaExpr : Expr {
    std::vector<std::string> params;
    StmtPtr body;
    LambdaExpr(std::vector<std::string> p, StmtPtr b) : params(std::move(p)), body(std::move(b)) {}
};

struct TernaryExpr : Expr {
    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
    TernaryExpr(ExprPtr c, ExprPtr t, ExprPtr e) : condition(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)) {}
};

// range: start..end or start..end step stepVal (for for-in and iteration)
struct RangeExpr : Expr {
    ExprPtr start;
    ExprPtr end;
    ExprPtr step;  // null = 1
    RangeExpr(ExprPtr s, ExprPtr e, ExprPtr st = nullptr) : start(std::move(s)), end(std::move(e)), step(std::move(st)) {}
};

// duration literal: 2s, 5m, 100ms, 1h -> seconds as number (for sleep etc.)
struct DurationExpr : Expr {
    ExprPtr amount;
    std::string unit;  // "ms", "s", "m", "h"
    DurationExpr(ExprPtr a, std::string u) : amount(std::move(a)), unit(std::move(u)) {}
};

// null coalescing: left ?? right (use right if left is null)
struct CoalesceExpr : Expr {
    ExprPtr left;
    ExprPtr right;
    CoalesceExpr(ExprPtr l, ExprPtr r) : left(std::move(l)), right(std::move(r)) {}
};

// pipeline: left |> right => right(left) (right must evaluate to callable)
struct PipelineExpr : Expr {
    ExprPtr left;
    ExprPtr right;
    PipelineExpr(ExprPtr l, ExprPtr r) : left(std::move(l)), right(std::move(r)) {}
};

// list comprehension: [ for x in iter : expr ] or [ for x in iter if cond : expr ]
struct ArrayComprehensionExpr : Expr {
    std::string varName;
    ExprPtr iterExpr;
    ExprPtr bodyExpr;
    ExprPtr filterExpr;  // optional: if present, only include when true
    ArrayComprehensionExpr(std::string v, ExprPtr i, ExprPtr b, ExprPtr f = nullptr)
        : varName(std::move(v)), iterExpr(std::move(i)), bodyExpr(std::move(b)), filterExpr(std::move(f)) {}
};

// spread in array: ... expr (expr must evaluate to array)
struct SpreadExpr : Expr {
    ExprPtr target;
    explicit SpreadExpr(ExprPtr t) : target(std::move(t)) {}
};

// array literal: [ expr, expr, ...expr, ... ]
struct ArrayLiteral : Expr {
    std::vector<ExprPtr> elements;
    ArrayLiteral() = default;
    explicit ArrayLiteral(std::vector<ExprPtr> e) : elements(std::move(e)) {}
};

// map literal: { "key": value, ... }
struct MapLiteral : Expr {
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
    MapLiteral() = default;
    explicit MapLiteral(std::vector<std::pair<ExprPtr, ExprPtr>> e) : entries(std::move(e)) {}
};

// f-string / string interpolation: f"Hello {name}!"
struct FStringExpr : Expr {
    std::vector<std::variant<std::string, ExprPtr>> parts;  // literal string or expression
    FStringExpr() = default;
};

// optional chaining: obj?.prop
struct OptionalChainExpr : Expr {
    ExprPtr object;
    std::string member;
    OptionalChainExpr(ExprPtr o, std::string m) : object(std::move(o)), member(std::move(m)) {}
};

// slice: obj[start:end] or obj[start:end:step]; null = omit
struct SliceExpr : Expr {
    ExprPtr object;
    ExprPtr start;
    ExprPtr end;
    ExprPtr step;
    SliceExpr(ExprPtr o, ExprPtr s, ExprPtr e, ExprPtr st = nullptr) : object(std::move(o)), start(std::move(s)), end(std::move(e)), step(std::move(st)) {}
};

// statement nodes ---
struct Stmt {
    virtual ~Stmt() = default;
    int line = 0, column = 0;
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    BlockStmt() = default;
    explicit BlockStmt(std::vector<StmtPtr> stmts) : statements(std::move(stmts)) {}
};

struct VarDeclStmt : Stmt {
    std::string name;
    bool isConst;
    bool hasType;
    std::string typeName;
    ExprPtr initializer;
    VarDeclStmt(std::string n, bool c, bool ht, std::string tn, ExprPtr i)
        : name(std::move(n)), isConst(c), hasType(ht), typeName(std::move(tn)), initializer(std::move(i)) {}
};

// destructuring: let [x, y] = arr or let {a, b} = obj
struct DestructureStmt : Stmt {
    bool isArray;  // true = [x,y], false = {a,b}
    std::vector<std::string> names;
    ExprPtr initializer;
    DestructureStmt(bool arr, std::vector<std::string> n, ExprPtr i)
        : isArray(arr), names(std::move(n)), initializer(std::move(i)) {}
};

struct IfStmt : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
    std::vector<std::pair<ExprPtr, StmtPtr>> elifBranches;
    IfStmt(ExprPtr c, StmtPtr t, StmtPtr e = nullptr) : condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct ForRangeStmt : Stmt {
    std::string label;   // optional loop label for "break label" / "continue label"
    std::string varName;
    ExprPtr start;
    ExprPtr end;
    ExprPtr step;
    StmtPtr body;
    ForRangeStmt(std::string v, ExprPtr s, ExprPtr e, ExprPtr st, StmtPtr b)
        : varName(std::move(v)), start(std::move(s)), end(std::move(e)), step(std::move(st)), body(std::move(b)) {}
};

struct ForCStyleStmt : Stmt {
    std::string label;
    StmtPtr init;
    ExprPtr condition;
    ExprPtr update;
    StmtPtr body;
    ForCStyleStmt(StmtPtr i, ExprPtr c, ExprPtr u, StmtPtr b)
        : init(std::move(i)), condition(std::move(c)), update(std::move(u)), body(std::move(b)) {}
};

struct WhileStmt : Stmt {
    std::string label;
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr c, StmtPtr b) : condition(std::move(c)), body(std::move(b)) {}
};

struct BreakStmt : Stmt {
    std::string label;  // optional: break outer
    BreakStmt() = default;
    explicit BreakStmt(std::string l) : label(std::move(l)) {}
};
struct ContinueStmt : Stmt {
    std::string label;
    ContinueStmt() = default;
    explicit ContinueStmt(std::string l) : label(std::move(l)) {}
};

struct RepeatStmt : Stmt {
    ExprPtr count;
    StmtPtr body;
    RepeatStmt(ExprPtr c, StmtPtr b) : count(std::move(c)), body(std::move(b)) {}
};

struct DeferStmt : Stmt {
    ExprPtr expr;  // typically a call, e.g. close(f)
    DeferStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct TryStmt : Stmt {
    StmtPtr tryBlock;
    std::string catchVar;
    std::string catchTypeName;  // empty = catch any; else only catch when error_name(e) == catchTypeName
    StmtPtr catchBlock;
    StmtPtr elseBlock;   // runs when try completes without throwing (Python-style)
    StmtPtr finallyBlock;
    TryStmt(StmtPtr t, std::string cv, std::string ctn, StmtPtr c, StmtPtr el, StmtPtr f = nullptr)
        : tryBlock(std::move(t)), catchVar(std::move(cv)), catchTypeName(std::move(ctn)), catchBlock(std::move(c)), elseBlock(std::move(el)), finallyBlock(std::move(f)) {}
};

struct ThrowStmt : Stmt {
    ExprPtr value;
    ThrowStmt(ExprPtr v) : value(std::move(v)) {}
};

struct RethrowStmt : Stmt {
    RethrowStmt() = default;
};

struct AssertStmt : Stmt {
    ExprPtr condition;
    ExprPtr message;  // optional; if null use default "Assertion failed"
    AssertStmt(ExprPtr c, ExprPtr m = nullptr) : condition(std::move(c)), message(std::move(m)) {}
};

struct MatchCase {
    ExprPtr pattern;   // literal, _, or object pattern
    ExprPtr guard;     // optional: e.g. "if x > 10"
    bool isDefault;
    StmtPtr body;
};
struct MatchStmt : Stmt {
    ExprPtr value;
    std::vector<MatchCase> cases;
    MatchStmt(ExprPtr v, std::vector<MatchCase> c) : value(std::move(v)), cases(std::move(c)) {}
};

// for x in iterable, or for key, value in map
struct ForInStmt : Stmt {
    std::string label;
    std::string varName;       // key (or single variable for array)
    std::string valueVarName; // optional: value when iterating map
    ExprPtr iterable;
    StmtPtr body;
    ForInStmt(std::string v, ExprPtr i, StmtPtr b) : varName(std::move(v)), iterable(std::move(i)), body(std::move(b)) {}
    ForInStmt(std::string k, std::string v, ExprPtr i, StmtPtr b) : varName(std::move(k)), valueVarName(std::move(v)), iterable(std::move(i)), body(std::move(b)) {}
};

struct ReturnStmt : Stmt {
    std::vector<ExprPtr> values;  // multiple return values: return a, b -> [a, b]
    ReturnStmt() = default;
    explicit ReturnStmt(ExprPtr v) { if (v) values.push_back(std::move(v)); }
    explicit ReturnStmt(std::vector<ExprPtr> v) : values(std::move(v)) {}
};

struct Param {
    std::string name;
    std::string typeName;
    ExprPtr defaultExpr;  // null = no default
    Param(std::string n, std::string t, ExprPtr d = nullptr) : name(std::move(n)), typeName(std::move(t)), defaultExpr(std::move(d)) {}
};

struct FunctionDeclStmt : Stmt {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    bool hasReturnType;
    StmtPtr body;
    bool isExport;
    FunctionDeclStmt(std::string n, std::vector<Param> p, std::string rt, bool hrt, StmtPtr b, bool exp = false)
        : name(std::move(n)), params(std::move(p)), returnType(std::move(rt)), hasReturnType(hrt), body(std::move(b)), isExport(exp) {}
};

struct ClassDeclStmt : Stmt {
    std::string name;
    std::string superClass;
    bool hasSuper;
    std::vector<std::pair<std::string, std::string>> members;  // access, name
    std::vector<StmtPtr> methods;
    std::vector<StmtPtr> constructorBody;
    ClassDeclStmt(std::string n) : name(std::move(n)), hasSuper(false) {}
};

struct ImportStmt : Stmt {
    std::string moduleName;
    std::string alias;
    bool hasAlias;
    ImportStmt(std::string m, std::string a = "", bool ha = false) : moduleName(std::move(m)), alias(std::move(a)), hasAlias(ha) {}
};

struct Program : Stmt {
    std::vector<StmtPtr> statements;
    Program() = default;
    explicit Program(std::vector<StmtPtr> stmts) : statements(std::move(stmts)) {}
};

} // namespace kern

#endif // kERN_AST_HPP
