/* *
 * kern Parser Implementation - Recursive descent
 */

#include "parser.hpp"
#include <sstream>

namespace kern {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), current_(0) {}

const Token& Parser::peek() const {
    if (isAtEnd()) return tokens_.back();
    return tokens_[current_];
}

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

bool Parser::isAtEnd() const {
    return current_ >= tokens_.size() || tokens_[current_].type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType t) const { return !isAtEnd() && peek().type == t; }

bool Parser::match(TokenType t) {
    if (!check(t)) return false;
    advance();
    return true;
}

const Token& Parser::advance() {
    if (!isAtEnd()) current_++;
    return previous();
}

const Token& Parser::consume(TokenType t, const std::string& message) {
    if (check(t)) return advance();
    std::ostringstream oss;
    oss << message << " at " << peek().line << ":" << peek().column << " (got " << tokenTypeToString(peek().type) << ")";
    throw ParserError(oss.str(), peek().line, peek().column);
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON || previous().type == TokenType::NEWLINE) return;
        switch (peek().type) {
            case TokenType::CLASS:
            case TokenType::DEF:
            case TokenType::FUNCTION:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::RETURN:
            case TokenType::LET:
            case TokenType::CONST:
            case TokenType::VAR:
                return;
            default:
                break;
        }
        advance();
    }
}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    while (!isAtEnd() && !check(TokenType::END_OF_FILE)) {
        if (peek().type == TokenType::NEWLINE) { advance(); continue; }
        try {
            program->statements.push_back(declaration());
        } catch (const ParserError&) {
            throw;
        }
    }
    return program;
}

StmtPtr Parser::declaration() {
    if (match(TokenType::IMPORT)) return importStatement();
    if (match(TokenType::CLASS)) return classDeclaration();
    if (match(TokenType::DEF) || match(TokenType::FUNCTION)) return functionDeclaration();
    if (check(TokenType::LET) || check(TokenType::CONST) || check(TokenType::VAR) ||
        (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
         check(TokenType::STRING_TYPE) || check(TokenType::VOID))) {
        return varDeclaration();
    }
    return statement();
}

StmtPtr Parser::importStatement() {
    std::string name;
    if (match(TokenType::LPAREN)) {
        if (!match(TokenType::STRING))
            throw ParserError("Expected quoted module name, e.g. import(\"math\")", peek().line, peek().column);
        name = std::get<std::string>(previous().value);
        consume(TokenType::RPAREN, "Expected ')' after import module name");
    } else if (match(TokenType::STRING)) {
        name = std::get<std::string>(previous().value);
    } else {
        throw ParserError("Expected module name: use import(\"math\") or import \"math\"", peek().line, peek().column);
    }
    std::string alias;
    bool hasAlias = false;
    if (match(TokenType::IDENTIFIER) && previous().lexeme == "as") {
        consume(TokenType::IDENTIFIER, "Expected alias name");
        alias = std::get<std::string>(previous().value);
        hasAlias = true;
    }
    return std::make_unique<ImportStmt>(name, alias, hasAlias);
}

StmtPtr Parser::varDeclaration() {
    bool isConst = match(TokenType::CONST);
    if (!isConst) match(TokenType::LET) || match(TokenType::VAR);
    if (match(TokenType::LBRACKET)) {
        std::vector<std::string> names;
        while (!check(TokenType::RBRACKET) && !isAtEnd()) {
            consume(TokenType::IDENTIFIER, "Expected variable name in array destructuring");
            names.push_back(std::get<std::string>(previous().value));
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RBRACKET, "Expected ']' after array destructuring");
        consume(TokenType::ASSIGN, "Expected '=' after destructuring pattern");
        ExprPtr init = expression();
        return std::make_unique<DestructureStmt>(true, std::move(names), std::move(init));
    }
    if (match(TokenType::LBRACE)) {
        std::vector<std::string> names;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            consume(TokenType::IDENTIFIER, "Expected property name in object destructuring");
            names.push_back(std::get<std::string>(previous().value));
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RBRACE, "Expected '}' after object destructuring");
        consume(TokenType::ASSIGN, "Expected '=' after destructuring pattern");
        ExprPtr init = expression();
        return std::make_unique<DestructureStmt>(false, std::move(names), std::move(init));
    }
    bool hasType = false;
    std::string typeName;
    if (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
        check(TokenType::STRING_TYPE) || check(TokenType::VOID) || check(TokenType::CHAR) ||
        check(TokenType::LONG) || check(TokenType::DOUBLE)) {
        advance();
        hasType = true;
        typeName = previous().lexeme;
    }
    consume(TokenType::IDENTIFIER, "Expected variable name");
    std::string name = std::get<std::string>(previous().value);
    // optional ": Type" after name (Rust/TypeScript style)
    if (match(TokenType::COLON) && (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) ||
        check(TokenType::BOOL) || check(TokenType::STRING_TYPE) || check(TokenType::VOID) ||
        check(TokenType::CHAR) || check(TokenType::LONG) || check(TokenType::DOUBLE))) {
        advance();
        hasType = true;
        typeName = previous().lexeme;
    }
    ExprPtr init = nullptr;
    if (match(TokenType::ASSIGN)) init = expression();
    return std::make_unique<VarDeclStmt>(name, isConst, hasType, typeName, std::move(init));
}

StmtPtr Parser::functionDeclaration() {
    consume(TokenType::IDENTIFIER, "Expected function name");
    std::string name = std::get<std::string>(previous().value);
    consume(TokenType::LPAREN, "Expected '(' after function name");
    auto params = parameterList();
    consume(TokenType::RPAREN, "Expected ')' after parameters");
    std::string returnType;
    bool hasReturnType = false;
    if (check(TokenType::COLON) || check(TokenType::INT) || check(TokenType::FLOAT_TYPE) ||
        check(TokenType::BOOL) || check(TokenType::STRING_TYPE) || check(TokenType::VOID)) {
        if (match(TokenType::COLON)) advance();
        if (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
            check(TokenType::STRING_TYPE) || check(TokenType::VOID)) {
            advance();
            returnType = previous().lexeme;
            hasReturnType = true;
        }
    }
    bool isExport = check(TokenType::EXPORT);
    if (isExport) advance();
    StmtPtr body;
    if (match(TokenType::COLON)) {
        body = std::make_unique<BlockStmt>();
        if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
            static_cast<BlockStmt*>(body.get())->statements.push_back(expressionStatement());
    } else {
        consume(TokenType::LBRACE, "Expected '{' or ':' for function body");
        body = blockStatement();
    }
    return std::make_unique<FunctionDeclStmt>(name, std::move(params), returnType, hasReturnType, std::move(body), isExport);
}

std::vector<Param> Parser::parameterList() {
    std::vector<Param> params;
    while (!check(TokenType::RPAREN)) {
        if (check(TokenType::ELLIPSIS)) { advance(); params.emplace_back("...", "", nullptr); break; }
        std::string typeName;
        if (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
            check(TokenType::STRING_TYPE) || check(TokenType::VOID)) {
            advance();
            typeName = previous().lexeme;
        }
        consume(TokenType::IDENTIFIER, "Expected parameter name");
        std::string name = std::get<std::string>(previous().value);
        ExprPtr defaultExpr = nullptr;
        if (match(TokenType::ASSIGN)) defaultExpr = expression();
        params.emplace_back(std::move(name), std::move(typeName), std::move(defaultExpr));
        if (!match(TokenType::COMMA)) break;
    }
    return std::move(params);
}

StmtPtr Parser::classDeclaration() {
    consume(TokenType::IDENTIFIER, "Expected class name");
    std::string name = std::get<std::string>(previous().value);
    auto cls = std::make_unique<ClassDeclStmt>(name);
    if (match(TokenType::EXTENDS)) {
        consume(TokenType::IDENTIFIER, "Expected superclass name");
        cls->superClass = std::get<std::string>(previous().value);
        cls->hasSuper = true;
    }
    consume(TokenType::LBRACE, "Expected '{' after class name");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE) { advance(); continue; }
        if (match(TokenType::CONSTRUCTOR) || match(TokenType::INIT)) {
            consume(TokenType::LPAREN, "Expected '('");
            auto initParams = parameterList();
            consume(TokenType::RPAREN, "Expected ')'");
            StmtPtr initBody;
            if (match(TokenType::LBRACE)) initBody = blockStatement();
            else { consume(TokenType::COLON, "Expected body"); initBody = std::make_unique<BlockStmt>(); }
            cls->methods.push_back(std::make_unique<FunctionDeclStmt>("init", std::move(initParams), "", false, std::move(initBody), false));
            continue;
        }
        if (match(TokenType::PUBLIC) || match(TokenType::PRIVATE) || match(TokenType::PROTECTED)) {
            std::string access = previous().lexeme;
            consume(TokenType::IDENTIFIER, "Expected member name");
            cls->members.push_back({access, std::get<std::string>(previous().value)});
            if (match(TokenType::ASSIGN)) expression();
            continue;
        }
        if (match(TokenType::DEF)) {
            consume(TokenType::IDENTIFIER, "Expected method name");
            std::string methodName = std::get<std::string>(previous().value);
            consume(TokenType::LPAREN, "Expected '('");
            auto methodParams = parameterList();
            consume(TokenType::RPAREN, "Expected ')'");
            StmtPtr body;
            if (match(TokenType::LBRACE)) body = blockStatement();
            else { consume(TokenType::COLON, "Expected body"); body = std::make_unique<BlockStmt>(); }
            cls->methods.push_back(std::make_unique<FunctionDeclStmt>(methodName, std::move(methodParams), "", false, std::move(body), false));
            continue;
        }
        if (check(TokenType::IDENTIFIER) && current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == TokenType::LPAREN) {
            advance();
            std::string methodName = std::get<std::string>(previous().value);
            consume(TokenType::LPAREN, "Expected '('");
            auto methodParams = parameterList();
            consume(TokenType::RPAREN, "Expected ')'");
            StmtPtr body;
            if (match(TokenType::LBRACE)) body = blockStatement();
            else { consume(TokenType::COLON, "Expected body"); body = std::make_unique<BlockStmt>(); }
            cls->methods.push_back(std::make_unique<FunctionDeclStmt>(methodName, std::move(methodParams), "", false, std::move(body), false));
            continue;
        }
        break;
    }
    consume(TokenType::RBRACE, "Expected '}'");
    return cls;
}

StmtPtr Parser::statement() {
    if (check(TokenType::LET) || check(TokenType::CONST) || check(TokenType::VAR) ||
        (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
         check(TokenType::STRING_TYPE) || check(TokenType::VOID))) {
        return varDeclaration();
    }
    // print statement: print ( expr (, expr)* ) or print expr (, expr)*
    if (check(TokenType::IDENTIFIER) && peek().lexeme == "print") {
        advance();
        std::vector<std::pair<std::string, ExprPtr>> args;
        if (match(TokenType::LPAREN)) {
            args.emplace_back("", expression());
            while (match(TokenType::COMMA)) args.emplace_back("", expression());
            consume(TokenType::RPAREN, "Expected ')' after print arguments");
        } else {
            args.emplace_back("", expression());
            while (match(TokenType::COMMA)) args.emplace_back("", expression());
        }
        return std::make_unique<ExprStmt>(std::make_unique<CallExpr>(std::make_unique<Identifier>("print"), std::move(args)));
    }
    if (match(TokenType::IF)) return ifStatement();
    std::string loopLabel;
    if (check(TokenType::IDENTIFIER) && current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == TokenType::COLON) {
        loopLabel = peek().lexeme;
        advance();
        advance();
        while (match(TokenType::NEWLINE)) {}
    }
    if (match(TokenType::FOR)) {
        StmtPtr stmt = forStatement();
        if (auto* r = dynamic_cast<ForRangeStmt*>(stmt.get())) r->label = loopLabel;
        else if (auto* i = dynamic_cast<ForInStmt*>(stmt.get())) i->label = loopLabel;
        else if (auto* c = dynamic_cast<ForCStyleStmt*>(stmt.get())) c->label = loopLabel;
        return stmt;
    }
    if (match(TokenType::WHILE)) {
        StmtPtr stmt = whileStatement();
        if (auto* w = dynamic_cast<WhileStmt*>(stmt.get())) w->label = loopLabel;
        return stmt;
    }
    if (match(TokenType::REPEAT)) return repeatStatement();
    if (match(TokenType::DEFER)) return deferStatement();
    if (match(TokenType::TRY)) return tryStatement();
    if (match(TokenType::MATCH)) return matchStatement();
    if (match(TokenType::RETURN)) return returnStatement();
    if (match(TokenType::RETHROW)) return std::make_unique<RethrowStmt>();
    if (match(TokenType::THROW)) { ExprPtr v = expression(); return std::make_unique<ThrowStmt>(std::move(v)); }
    if (match(TokenType::ASSERT)) {
        ExprPtr cond = expression();
        ExprPtr msg = nullptr;
        if (match(TokenType::COMMA)) msg = expression();
        return std::make_unique<AssertStmt>(std::move(cond), std::move(msg));
    }
    if (match(TokenType::BREAK)) {
        std::string lbl;
        if (check(TokenType::IDENTIFIER)) { lbl = std::get<std::string>(peek().value); advance(); }
        return std::make_unique<BreakStmt>(std::move(lbl));
    }
    if (match(TokenType::CONTINUE)) {
        std::string lbl;
        if (check(TokenType::IDENTIFIER)) { lbl = std::get<std::string>(peek().value); advance(); }
        return std::make_unique<ContinueStmt>(std::move(lbl));
    }
    if (match(TokenType::DO)) {
        consume(TokenType::LBRACE, "Expected '{' after do");
        return blockStatement();
    }
    if (match(TokenType::LBRACE)) return blockStatement();
    return expressionStatement();
}

StmtPtr Parser::ifStatement() {
    ExprPtr cond = expression();
    StmtPtr thenBranch;
    if (match(TokenType::COLON)) {
        thenBranch = std::make_unique<BlockStmt>();
        if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE && !check(TokenType::ELIF) && !check(TokenType::ELSE))
            static_cast<BlockStmt*>(thenBranch.get())->statements.push_back(statement());
    } else {
        consume(TokenType::LBRACE, "Expected '{' or ':'");
        thenBranch = blockStatement();
    }
    auto ifStmt = std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch));
    while (match(TokenType::ELIF)) {
        ExprPtr elifCond = expression();
        StmtPtr elifBody;
        if (match(TokenType::COLON)) {
            elifBody = std::make_unique<BlockStmt>();
            if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE && !check(TokenType::ELIF) && !check(TokenType::ELSE))
                static_cast<BlockStmt*>(elifBody.get())->statements.push_back(statement());
        } else {
            consume(TokenType::LBRACE, "Expected '{' or ':'");
            elifBody = blockStatement();
        }
        ifStmt->elifBranches.push_back({std::move(elifCond), std::move(elifBody)});
    }
    while (match(TokenType::NEWLINE)) {}
    if (match(TokenType::ELSE)) {
        if (match(TokenType::COLON)) {
            ifStmt->elseBranch = std::make_unique<BlockStmt>();
            if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
                static_cast<BlockStmt*>(ifStmt->elseBranch.get())->statements.push_back(statement());
        } else {
            consume(TokenType::LBRACE, "Expected '{' or ':'");
            ifStmt->elseBranch = blockStatement();
        }
    }
    return ifStmt;
}

StmtPtr Parser::forStatement() {
    if (check(TokenType::LPAREN)) {
        advance();
        StmtPtr init = nullptr;
        if (!check(TokenType::SEMICOLON)) init = varDeclaration();
        consume(TokenType::SEMICOLON, "Expected ';'");
        ExprPtr cond = nullptr;
        if (!check(TokenType::SEMICOLON)) cond = expression();
        consume(TokenType::SEMICOLON, "Expected ';'");
        ExprPtr update = nullptr;
        if (!check(TokenType::RPAREN)) update = expression();
        consume(TokenType::RPAREN, "Expected ')'");
        StmtPtr body = statement();
        return std::make_unique<ForCStyleStmt>(std::move(init), std::move(cond), std::move(update), std::move(body));
    }
    consume(TokenType::IDENTIFIER, "Expected loop variable");
    std::string varName = std::get<std::string>(previous().value);
    std::string valueVarName;
    if (match(TokenType::COMMA)) {
        consume(TokenType::IDENTIFIER, "Expected second loop variable (e.g. for key, value in map)");
        valueVarName = std::get<std::string>(previous().value);
    }
    consume(TokenType::IN, "Expected 'in'");
    if (check(TokenType::RANGE)) {
        advance();
        consume(TokenType::LPAREN, "Expected '('");
        ExprPtr start = expression();
        ExprPtr end = nullptr;
        ExprPtr step = nullptr;
        if (match(TokenType::COMMA)) { end = expression(); if (match(TokenType::COMMA)) step = expression(); }
        if (!end) { end = std::move(start); start = std::make_unique<IntLiteral>(0); }
        if (!step) step = std::make_unique<IntLiteral>(1);
        consume(TokenType::RPAREN, "Expected ')'");
        StmtPtr body = statement();
        return std::make_unique<ForRangeStmt>(varName, std::move(start), std::move(end), std::move(step), std::move(body));
    }
    ExprPtr iterable = expression();
    if (auto* r = dynamic_cast<RangeExpr*>(iterable.get())) {
        StmtPtr body;
        if (match(TokenType::COLON)) {
            body = std::make_unique<BlockStmt>();
            if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
                static_cast<BlockStmt*>(body.get())->statements.push_back(statement());
        } else {
            consume(TokenType::LBRACE, "Expected '{' or ':'");
            body = blockStatement();
        }
        ExprPtr step = r->step ? std::move(r->step) : std::make_unique<IntLiteral>(1);
        return std::make_unique<ForRangeStmt>(varName, std::move(r->start), std::move(r->end), std::move(step), std::move(body));
    }
    StmtPtr body;
    if (match(TokenType::COLON)) {
        body = std::make_unique<BlockStmt>();
        if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
            static_cast<BlockStmt*>(body.get())->statements.push_back(statement());
    } else {
        consume(TokenType::LBRACE, "Expected '{' or ':'");
        body = blockStatement();
    }
    if (valueVarName.empty())
        return std::make_unique<ForInStmt>(varName, std::move(iterable), std::move(body));
    return std::make_unique<ForInStmt>(varName, valueVarName, std::move(iterable), std::move(body));
}

StmtPtr Parser::whileStatement() {
    ExprPtr cond = expression();
    StmtPtr body;
    if (match(TokenType::COLON)) {
        body = std::make_unique<BlockStmt>();
        if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
            static_cast<BlockStmt*>(body.get())->statements.push_back(statement());
    } else {
        consume(TokenType::LBRACE, "Expected '{' or ':'");
        body = blockStatement();
    }
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

StmtPtr Parser::repeatStatement() {
    ExprPtr count = expression();
    StmtPtr body;
    if (match(TokenType::COLON)) {
        body = std::make_unique<BlockStmt>();
        if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
            static_cast<BlockStmt*>(body.get())->statements.push_back(statement());
    } else {
        consume(TokenType::LBRACE, "Expected '{' or ':' after repeat count");
        body = blockStatement();
    }
    return std::make_unique<RepeatStmt>(std::move(count), std::move(body));
}

StmtPtr Parser::deferStatement() {
    ExprPtr expr = expression();
    return std::make_unique<DeferStmt>(std::move(expr));
}

StmtPtr Parser::tryStatement() {
    StmtPtr tryBlock;
    if (match(TokenType::LBRACE)) tryBlock = blockStatement();
    else { consume(TokenType::COLON, "Expected '{' or ':'"); tryBlock = std::make_unique<BlockStmt>(); static_cast<BlockStmt*>(tryBlock.get())->statements.push_back(statement()); }
    std::string catchVar;
    std::string catchTypeName;
    StmtPtr catchBlock = nullptr;
    if (match(TokenType::CATCH)) {
        if (match(TokenType::LPAREN)) {
            if (check(TokenType::IDENTIFIER)) {
                advance();
                std::string first = std::get<std::string>(previous().value);
                if (check(TokenType::IDENTIFIER)) {
                    advance();
                    catchTypeName = first;
                    catchVar = std::get<std::string>(previous().value);
                } else {
                    catchVar = first;
                }
            }
            consume(TokenType::RPAREN, "Expected ')'");
        }
        if (match(TokenType::LBRACE)) catchBlock = blockStatement();
        else { consume(TokenType::COLON, "Expected '{'"); catchBlock = std::make_unique<BlockStmt>(); static_cast<BlockStmt*>(catchBlock.get())->statements.push_back(statement()); }
    }
    StmtPtr elseBlock = nullptr;
    if (match(TokenType::ELSE)) {
        if (match(TokenType::LBRACE)) elseBlock = blockStatement();
        else { consume(TokenType::COLON, "Expected '{' or ':' after else"); elseBlock = std::make_unique<BlockStmt>(); static_cast<BlockStmt*>(elseBlock.get())->statements.push_back(statement()); }
    }
    StmtPtr finallyBlock = nullptr;
    if (match(TokenType::FINALLY)) {
        if (match(TokenType::LBRACE)) finallyBlock = blockStatement();
        else { consume(TokenType::COLON, "Expected '{'"); finallyBlock = std::make_unique<BlockStmt>(); static_cast<BlockStmt*>(finallyBlock.get())->statements.push_back(statement()); }
    }
    return std::make_unique<TryStmt>(std::move(tryBlock), catchVar, std::move(catchTypeName), std::move(catchBlock), std::move(elseBlock), std::move(finallyBlock));
}

StmtPtr Parser::matchStatement() {
    ExprPtr value = expression();
    consume(TokenType::LBRACE, "Expected '{' after match value");
    std::vector<MatchCase> cases;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE) { advance(); continue; }
        if (match(TokenType::CASE)) {
            ExprPtr pattern = expression();
            ExprPtr guard = nullptr;
            if (match(TokenType::IF)) guard = expression();
            bool isDef = false;
            if (dynamic_cast<Identifier*>(pattern.get()) && static_cast<Identifier*>(pattern.get())->name == "_") isDef = true;
            consume(TokenType::ARROW, "Expected '=>'");
            StmtPtr body;
            if (match(TokenType::LBRACE)) body = blockStatement();
            else body = std::make_unique<ExprStmt>(expression());
            cases.push_back(MatchCase{std::move(pattern), std::move(guard), isDef, std::move(body)});
        } else if (match(TokenType::IDENTIFIER) && previous().lexeme == "_") {
            consume(TokenType::ARROW, "Expected '=>'");
            StmtPtr body;
            if (match(TokenType::LBRACE)) body = blockStatement();
            else body = std::make_unique<ExprStmt>(expression());
            cases.push_back(MatchCase{std::make_unique<Identifier>("_"), nullptr, true, std::move(body)});
        } else break;
    }
    consume(TokenType::RBRACE, "Expected '}'");
    return std::make_unique<MatchStmt>(std::move(value), std::move(cases));
}

StmtPtr Parser::returnStatement() {
    std::vector<ExprPtr> values;
    if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !isAtEnd()) {
        values.push_back(expression());
        while (match(TokenType::COMMA) && !check(TokenType::NEWLINE) && !check(TokenType::RBRACE) && !isAtEnd())
            values.push_back(expression());
    }
    auto r = std::make_unique<ReturnStmt>();
    r->values = std::move(values);
    return r;
}

StmtPtr Parser::blockStatement() {
    auto block = std::make_unique<BlockStmt>();
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        if (peek().type == TokenType::NEWLINE) { advance(); continue; }
        if (peek().type == TokenType::SEMICOLON) { advance(); continue; }
        block->statements.push_back(statement());
    }
    consume(TokenType::RBRACE, "Expected '}'");
    return block;
}

StmtPtr Parser::expressionStatement() {
    ExprPtr expr = expression();
    return std::make_unique<ExprStmt>(std::move(expr));
}

ExprPtr Parser::expression() { return assignment(); }

int Parser::getPrecedence(TokenType op) {
    switch (op) {
        case TokenType::OR: return 2;
        case TokenType::AND: return 3;
        case TokenType::BIT_OR: return 4;
        case TokenType::BIT_XOR: return 5;
        case TokenType::BIT_AND: return 6;
        case TokenType::EQ: case TokenType::NEQ: return 7;
        case TokenType::LT: case TokenType::LE: case TokenType::GT: case TokenType::GE: return 8;
        case TokenType::SHL: case TokenType::SHR: return 9;
        case TokenType::PLUS: case TokenType::MINUS: return 10;
        case TokenType::STAR: case TokenType::SLASH: case TokenType::PERCENT: case TokenType::STAR_STAR: return 11;
        default: return -1;
    }
}

ExprPtr Parser::parsePrecedence(int minPrec) {
    if (minPrec > 11) return unary();
    ExprPtr left = parsePrecedence(minPrec + 1);
    for (;;) {
        TokenType op = peek().type;
        int prec = getPrecedence(op);
        if (prec < 0 || prec != minPrec) break;
        advance();
        ExprPtr right = parsePrecedence(minPrec + 1);
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
    }
    return left;
}

ExprPtr Parser::assignment() {
    ExprPtr expr = ternary();
    if (match(TokenType::ASSIGN) || match(TokenType::PLUS_EQ) || match(TokenType::MINUS_EQ) ||
        match(TokenType::STAR_EQ) || match(TokenType::SLASH_EQ) || match(TokenType::PERCENT_EQ)) {
        TokenType op = previous().type;
        ExprPtr value = assignment();
        if (auto* id = dynamic_cast<Identifier*>(expr.get())) {
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        if (auto* mem = dynamic_cast<MemberExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        if (auto* idx = dynamic_cast<IndexExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        throw ParserError("Invalid assignment target", peek().line, peek().column);
    }
    return expr;
}

ExprPtr Parser::coalesce() {
    ExprPtr left = parsePrecedence(2);
    while (match(TokenType::COALESCE))
        left = std::make_unique<CoalesceExpr>(std::move(left), coalesce());
    return left;
}

ExprPtr Parser::pipeline() {
    ExprPtr left = coalesce();
    while (match(TokenType::PIPE)) {
        ExprPtr right = coalesce();
        left = std::make_unique<PipelineExpr>(std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::ternary() {
    ExprPtr expr = pipeline();
    if (match(TokenType::QUESTION)) {
        ExprPtr thenExpr = expression();
        consume(TokenType::COLON, "Expected ':' in ternary");
        ExprPtr elseExpr = expression();
        return std::make_unique<TernaryExpr>(std::move(expr), std::move(thenExpr), std::move(elseExpr));
    }
    return expr;
}

ExprPtr Parser::unary() {
    if (match(TokenType::NOT) || match(TokenType::MINUS)) {
        TokenType op = previous().type;
        return std::make_unique<UnaryExpr>(op, unary());
    }
    return postfix();
}

ExprPtr Parser::postfix() {
    ExprPtr expr = primary();
    if (match(TokenType::DOT_DOT)) {
        ExprPtr end = expression();
        ExprPtr step = nullptr;
        if (check(TokenType::IDENTIFIER) && peek().lexeme == "step") {
            advance();
            step = expression();
        }
        return std::make_unique<RangeExpr>(std::move(expr), std::move(end), std::move(step));
    }
    for (;;) {
        if (match(TokenType::LPAREN)) {
            auto args = argumentList();
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
        } else if (check(TokenType::QUESTION) && current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == TokenType::LBRACKET) {
            advance();
            advance();
            ExprPtr idx = expression();
            consume(TokenType::RBRACKET, "Expected ']' after ?[");
            expr = std::make_unique<OptionalIndexExpr>(std::move(expr), std::move(idx));
        } else if (match(TokenType::LBRACKET)) {
            ExprPtr first = expression();
            if (match(TokenType::COLON)) {
                ExprPtr end = nullptr;
                ExprPtr step = nullptr;
                if (!check(TokenType::RBRACKET)) end = expression();
                if (match(TokenType::COLON)) { if (!check(TokenType::RBRACKET)) step = expression(); }
                consume(TokenType::RBRACKET, "Expected ']'");
                expr = std::make_unique<SliceExpr>(std::move(expr), std::move(first), std::move(end), std::move(step));
            } else {
                consume(TokenType::RBRACKET, "Expected ']'");
                expr = std::make_unique<IndexExpr>(std::move(expr), std::move(first));
            }
        } else if (match(TokenType::QUESTION_DOT)) {
            consume(TokenType::IDENTIFIER, "Expected member name after ?.");
            expr = std::make_unique<OptionalChainExpr>(std::move(expr), std::get<std::string>(previous().value));
        } else if (match(TokenType::DOT)) {
            std::string memberName;
            if (match(TokenType::IDENTIFIER)) memberName = std::get<std::string>(previous().value);
            else if (match(TokenType::INIT)) memberName = "init";
            else consume(TokenType::IDENTIFIER, "Expected member name");
            if (memberName.empty()) memberName = std::get<std::string>(previous().value);
            expr = std::make_unique<MemberExpr>(std::move(expr), std::move(memberName));
        } else break;
    }
    return expr;
}

ExprPtr Parser::primary() {
    if (match(TokenType::TRUE)) return std::make_unique<BoolLiteral>(true);
    if (match(TokenType::FALSE)) return std::make_unique<BoolLiteral>(false);
    if (match(TokenType::NULL_LIT)) return std::make_unique<NullLiteral>();
    if (match(TokenType::INTEGER)) {
        auto e = std::make_unique<IntLiteral>(std::get<int64_t>(previous().value));
        e->line = previous().line; e->column = previous().column;
        if (check(TokenType::IDENTIFIER)) {
            std::string u = peek().lexeme;
            if (u == "ms" || u == "s" || u == "m" || u == "h") { advance(); return std::make_unique<DurationExpr>(std::move(e), u); }
        }
        return e;
    }
    if (match(TokenType::FLOAT)) {
        auto e = std::make_unique<FloatLiteral>(std::get<double>(previous().value));
        e->line = previous().line; e->column = previous().column;
        if (check(TokenType::IDENTIFIER)) {
            std::string u = peek().lexeme;
            if (u == "ms" || u == "s" || u == "m" || u == "h") { advance(); return std::make_unique<DurationExpr>(std::move(e), u); }
        }
        return e;
    }
    if (match(TokenType::STRING)) {
        auto e = std::make_unique<StringLiteral>(std::get<std::string>(previous().value));
        e->line = previous().line; e->column = previous().column;
        return e;
    }
    if (check(TokenType::IDENTIFIER) && current_ + 1 < tokens_.size() &&
        std::holds_alternative<std::string>(peek().value) && std::get<std::string>(peek().value) == "f" &&
        tokens_[current_ + 1].type == TokenType::STRING) {
        advance();
        advance();
        std::string raw = std::get<std::string>(previous().value);
        auto fs = std::make_unique<FStringExpr>();
        size_t i = 0;
        while (i < raw.size()) {
            size_t brace = raw.find('{', i);
            if (brace == std::string::npos) {
                if (i < raw.size()) fs->parts.emplace_back(raw.substr(i));
                break;
            }
            if (brace > i) fs->parts.emplace_back(raw.substr(i, brace - i));
            size_t end = raw.find('}', brace);
            if (end == std::string::npos)
                throw ParserError("Unclosed '{' in f-string", peek().line, peek().column);
            std::string inner = raw.substr(brace + 1, end - brace - 1);
            size_t s = inner.find_first_not_of(" \t");
            size_t e2 = (s == std::string::npos) ? 0 : inner.find_last_not_of(" \t");
            if (s != std::string::npos && e2 != std::string::npos)
                inner = inner.substr(s, e2 - s + 1);
            fs->parts.emplace_back(std::make_unique<Identifier>(std::move(inner)));
            i = end + 1;
        }
        return fs;
    }
    if (match(TokenType::THIS)) return std::make_unique<Identifier>("this");
    if (match(TokenType::SUPER)) return std::make_unique<Identifier>("super");
    // import("module") as expression so let g = import("g2d") works
    if (match(TokenType::IMPORT)) {
        consume(TokenType::LPAREN, "Expected '(' after import");
        if (!match(TokenType::STRING))
            throw ParserError("Expected quoted module name, e.g. import(\"math\")", peek().line, peek().column);
        std::string name = std::get<std::string>(previous().value);
        consume(TokenType::RPAREN, "Expected ')' after import module name");
        std::vector<std::pair<std::string, ExprPtr>> args;
        args.push_back({"", std::make_unique<StringLiteral>(name)});
        return std::make_unique<CallExpr>(std::make_unique<Identifier>("__import"), std::move(args));
    }
    if (match(TokenType::IDENTIFIER)) {
        return std::make_unique<Identifier>(std::get<std::string>(previous().value));
    }
    // type keywords, range, and function as identifiers so int(x), float(x), range(0,10), and
    // "function" inside strings (e.g. ok("var, function")) parse correctly
    if (match(TokenType::INT) || match(TokenType::FLOAT_TYPE) || match(TokenType::BOOL) ||
        match(TokenType::STRING_TYPE) || match(TokenType::VOID) || match(TokenType::CHAR) ||
        match(TokenType::LONG) || match(TokenType::DOUBLE) || match(TokenType::RANGE) ||
        match(TokenType::FUNCTION)) {
        return std::make_unique<Identifier>(previous().lexeme);
    }
    if (match(TokenType::LPAREN)) {
        ExprPtr expr = expression();
        consume(TokenType::RPAREN, "Expected ')'");
        return expr;
    }
    if (match(TokenType::LBRACKET)) {
        if (match(TokenType::FOR)) {
            std::string varName = std::get<std::string>(consume(TokenType::IDENTIFIER, "Expected variable name in comprehension").value);
            consume(TokenType::IN, "Expected 'in' in comprehension");
            ExprPtr iterExpr = expression();
            ExprPtr filterExpr = nullptr;
            if (match(TokenType::IF)) filterExpr = expression();
            consume(TokenType::COLON, "Expected ':' before comprehension body");
            ExprPtr bodyExpr = expression();
            consume(TokenType::RBRACKET, "Expected ']' after comprehension");
            return std::make_unique<ArrayComprehensionExpr>(std::move(varName), std::move(iterExpr), std::move(bodyExpr), std::move(filterExpr));
        }
        std::vector<ExprPtr> elements;
        while (!check(TokenType::RBRACKET) && !isAtEnd()) {
            while (match(TokenType::NEWLINE)) {}
            if (check(TokenType::RBRACKET)) break;
            if (match(TokenType::ELLIPSIS)) elements.push_back(std::make_unique<SpreadExpr>(expression()));
            else elements.push_back(expression());
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteral>(std::move(elements));
    }
    if (match(TokenType::LBRACE)) {
        std::vector<std::pair<ExprPtr, ExprPtr>> entries;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            while (match(TokenType::NEWLINE)) {}
            if (check(TokenType::RBRACE)) break;
            ExprPtr key = expression();
            consume(TokenType::COLON, "Expected ':' in map literal");
            ExprPtr val = expression();
            entries.push_back({std::move(key), std::move(val)});
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RBRACE, "Expected '}'");
        return std::make_unique<MapLiteral>(std::move(entries));
    }
    if (match(TokenType::LAMBDA)) return parseLambda();
    std::ostringstream oss;
    oss << "Expected expression at " << peek().line << ":" << peek().column;
    throw ParserError(oss.str(), peek().line, peek().column);
}

ExprPtr Parser::parseLambda() {
    consume(TokenType::LPAREN, "Expected '(' for lambda");
    std::vector<std::string> params;
    if (!check(TokenType::RPAREN) && !check(TokenType::ARROW)) {
        do {
            consume(TokenType::IDENTIFIER, "Expected parameter name");
            params.push_back(std::get<std::string>(previous().value));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "Expected ')'");
    consume(TokenType::ARROW, "Expected '=>'");
    StmtPtr body;
    if (match(TokenType::LBRACE)) body = blockStatement();
    else body = std::make_unique<ExprStmt>(expression());
    return std::make_unique<LambdaExpr>(params, std::move(body));
}

std::vector<std::pair<std::string, ExprPtr>> Parser::argumentList() {
    std::vector<std::pair<std::string, ExprPtr>> args;
    while (!check(TokenType::RPAREN)) {
        while (match(TokenType::NEWLINE)) {}
        if (check(TokenType::IDENTIFIER) && current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == TokenType::ASSIGN) {
            advance();
            std::string name = std::holds_alternative<std::string>(previous().value) ? std::get<std::string>(previous().value) : "";
            advance();  // consume ASSIGN
            ExprPtr e = expression();
            args.emplace_back(std::move(name), std::move(e));
        } else {
            args.emplace_back("", expression());
        }
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RPAREN, "Expected ')'");
    return args;
}

} // namespace kern
