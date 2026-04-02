/* *
 * kern Parser - Builds AST from token stream
 */

#ifndef KERN_PARSER_HPP
#define KERN_PARSER_HPP

#include "ast.hpp"
#include "token.hpp"
#include <vector>
#include <string>
#include <stdexcept>

namespace kern {

class ParserError : public std::runtime_error {
public:
    int line, column;
    ParserError(const std::string& msg, int ln, int col) : std::runtime_error(msg), line(ln), column(col) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<Program> parse();

private:
    std::vector<Token> tokens_;
    size_t current_;
    // / nesting depth of function / method / lambda block bodies (return allowed only when > 0).
    int funcBodyDepth_ = 0;

    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    bool check(TokenType t) const;
    bool match(TokenType t);
    const Token& advance();
    const Token& consume(TokenType t, const std::string& message);
    void synchronize();

    StmtPtr declaration();
    StmtPtr decoratedFunctionDeclaration();
    StmtPtr statement();
    StmtPtr varDeclaration();
    StmtPtr functionDeclaration();
    StmtPtr classDeclaration();
    StmtPtr enumDeclaration();
    StmtPtr structDeclaration();
    StmtPtr externDeclaration();
    StmtPtr importStatement();
    StmtPtr fromImportStatement();
    StmtPtr ifStatement();
    StmtPtr forStatement();
    StmtPtr whileStatement();
    StmtPtr repeatStatement();
    StmtPtr deferStatement();
    StmtPtr tryStatement();
    StmtPtr withStatement();
    StmtPtr matchStatement();
    StmtPtr returnStatement();
    StmtPtr unsafeStatement();
    StmtPtr blockStatement();
    StmtPtr expressionStatement();

    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr pipeline();
    ExprPtr coalesce();
    ExprPtr ternary();
    ExprPtr parsePrecedence(int minPrec);
    ExprPtr unary();
    ExprPtr postfix();
    ExprPtr primary();
    static int getPrecedence(TokenType op);

    ExprPtr parseLambda();
    std::vector<CallArg> argumentList();
    std::vector<Param> parameterList();
};

} // namespace kern

#endif // kERN_PARSER_HPP
