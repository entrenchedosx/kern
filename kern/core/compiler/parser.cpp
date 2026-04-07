/* *
 * kern Parser Implementation - Recursive descent
 */

#include "parser.hpp"
#include <sstream>

namespace kern {

namespace {
struct ParsedDecorator {
    std::string name;
    std::vector<CallArg> args;
};

static bool isIdentifierToken(const Token& t, const char* text) {
    return t.type == TokenType::IDENTIFIER && t.lexeme == text;
}
} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), current_(0) {}

const Token& Parser::peek() const {
    if (tokens_.empty()) {
        static const Token kEmptyEof(TokenType::END_OF_FILE, "", 0, 0);
        return kEmptyEof;
    }
    if (isAtEnd()) return tokens_.back();
    return tokens_[current_];
}

const Token& Parser::previous() const {
    if (tokens_.empty()) {
        static const Token kEmptyEof(TokenType::END_OF_FILE, "", 0, 0);
        return kEmptyEof;
    }
    // keep previous() total-safe for error-recovery paths that may call advance() at EOF.
    if (current_ == 0) return tokens_.front();
    size_t idx = current_ - 1;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

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
    if (tokens_.empty()) return peek();
    if (!isAtEnd()) {
        current_++;
        return previous();
    }
    // at EOF we must not underflow previous().
    return peek();
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
            case TokenType::STRUCT:
            case TokenType::EXTERN:
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
    if (check(TokenType::AT)) return decoratedFunctionDeclaration();
    if (isIdentifierToken(peek(), "async")) {
        advance();
        if (!(match(TokenType::DEF) || match(TokenType::FUNCTION)))
            throw ParserError("Expected 'def' after 'async'", peek().line, peek().column);
        return functionDeclaration();
    }
    if (match(TokenType::FROM)) return fromImportStatement();
    if (match(TokenType::IMPORT)) return importStatement();
    if (match(TokenType::WITH)) return withStatement();
    if (match(TokenType::ENUM)) return enumDeclaration();
    if (match(TokenType::STRUCT)) return structDeclaration();
    if (match(TokenType::EXTERN)) return externDeclaration();
    if (match(TokenType::CLASS)) return classDeclaration();
    if (match(TokenType::DEF) || match(TokenType::FUNCTION)) return functionDeclaration();
    if (check(TokenType::LET) || check(TokenType::CONST) || check(TokenType::VAR) ||
        (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
         check(TokenType::STRING_TYPE) || check(TokenType::VOID))) {
        return varDeclaration();
    }
    return statement();
}

StmtPtr Parser::decoratedFunctionDeclaration() {
    std::vector<ParsedDecorator> decorators;
    while (match(TokenType::AT)) {
        consume(TokenType::IDENTIFIER, "Expected decorator name after '@'");
        ParsedDecorator d;
        d.name = std::get<std::string>(previous().value);
        if (match(TokenType::LPAREN)) d.args = argumentList();
        decorators.push_back(std::move(d));
        while (match(TokenType::NEWLINE) || match(TokenType::SEMICOLON)) {}
    }
    if (isIdentifierToken(peek(), "async")) advance();
    if (!(match(TokenType::DEF) || match(TokenType::FUNCTION)))
        throw ParserError("Decorator must be followed by a function declaration", peek().line, peek().column);
    StmtPtr fnStmt = functionDeclaration();
    auto* fn = dynamic_cast<FunctionDeclStmt*>(fnStmt.get());
    if (!fn)
        throw ParserError("Decorators are currently supported on functions only", peek().line, peek().column);
    std::string fnName = fn->name;

    auto out = std::make_unique<SequenceStmt>();
    out->statements.push_back(std::move(fnStmt));
    for (auto& d : decorators) {
        std::vector<CallArg> callArgs;
        callArgs.push_back(CallArg{"", std::make_unique<StringLiteral>(d.name), false});
        callArgs.push_back(CallArg{"", std::make_unique<Identifier>(fnName), false});
        std::vector<std::pair<ExprPtr, ExprPtr>> namedEntries;
        for (auto& a : d.args) {
            if (a.name.empty()) {
                callArgs.push_back(CallArg{"", std::move(a.expr), a.spread});
            } else {
                namedEntries.push_back(
                    {std::make_unique<StringLiteral>(a.name), std::move(a.expr)});
            }
        }
        if (!namedEntries.empty()) {
            callArgs.push_back(
                CallArg{"", std::make_unique<MapLiteral>(std::move(namedEntries)), false});
        }
        out->statements.push_back(std::make_unique<ExprStmt>(
            std::make_unique<CallExpr>(std::make_unique<Identifier>("__apply_decorator"), std::move(callArgs))));
    }
    return out;
}

StmtPtr Parser::structDeclaration() {
    consume(TokenType::IDENTIFIER, "Expected struct name");
    std::string structName = std::get<std::string>(previous().value);
    consume(TokenType::LBRACE, "Expected '{' after struct name");
    std::vector<StructFieldDecl> fields;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        while (match(TokenType::NEWLINE) || match(TokenType::SEMICOLON)) {}
        if (check(TokenType::RBRACE)) break;
        consume(TokenType::IDENTIFIER, "Expected struct field name");
        std::string fieldName = std::get<std::string>(previous().value);
        consume(TokenType::COLON, "Expected ':' after struct field name");
        if (!(check(TokenType::IDENTIFIER) || check(TokenType::INT) || check(TokenType::FLOAT_TYPE) ||
              check(TokenType::BOOL) || check(TokenType::STRING_TYPE) || check(TokenType::VOID) ||
              check(TokenType::CHAR) || check(TokenType::LONG) || check(TokenType::DOUBLE) ||
              check(TokenType::PTR) || check(TokenType::REF))) {
            throw ParserError("Expected struct field type", peek().line, peek().column);
        }
        advance();
        fields.emplace_back(fieldName, previous().lexeme);
        (void)match(TokenType::COMMA);
        (void)match(TokenType::SEMICOLON);
    }
    consume(TokenType::RBRACE, "Expected '}' after struct declaration");
    return std::make_unique<StructDeclStmt>(structName, std::move(fields));
}

StmtPtr Parser::externDeclaration() {
    if (!(match(TokenType::DEF) || match(TokenType::FUNCTION)))
        throw ParserError("Expected 'def' after 'extern'", peek().line, peek().column);
    consume(TokenType::IDENTIFIER, "Expected extern function name");
    std::string name = std::get<std::string>(previous().value);
    consume(TokenType::LPAREN, "Expected '(' after extern function name");
    std::vector<Param> parsedParams = parameterList();
    consume(TokenType::RPAREN, "Expected ')' after extern parameters");
    std::string returnType = "int";
    if (match(TokenType::COLON)) {
        if (!(check(TokenType::IDENTIFIER) || check(TokenType::INT) || check(TokenType::FLOAT_TYPE) ||
              check(TokenType::BOOL) || check(TokenType::STRING_TYPE) || check(TokenType::VOID) ||
              check(TokenType::CHAR) || check(TokenType::LONG) || check(TokenType::DOUBLE) ||
              check(TokenType::PTR) || check(TokenType::REF))) {
            throw ParserError("Expected return type after ':' in extern declaration", peek().line, peek().column);
        }
        advance();
        returnType = previous().lexeme;
    }
    consume(TokenType::FROM, "Expected 'from' in extern declaration");
    consume(TokenType::STRING, "Expected DLL name string after 'from'");
    std::string dllName = std::get<std::string>(previous().value);
    std::string symbolName = name;
    if (match(TokenType::AS)) {
        if (match(TokenType::STRING)) symbolName = std::get<std::string>(previous().value);
        else {
            consume(TokenType::IDENTIFIER, "Expected symbol name after 'as'");
            symbolName = std::get<std::string>(previous().value);
        }
    }
    std::vector<FfiParamDecl> ffiParams;
    ffiParams.reserve(parsedParams.size());
    for (auto& p : parsedParams) {
        ffiParams.emplace_back(p.name, p.typeName.empty() ? "int" : p.typeName);
    }
    return std::make_unique<FfiDeclStmt>(name, dllName, symbolName, returnType, "cdecl", std::move(ffiParams));
}

StmtPtr Parser::enumDeclaration() {
    consume(TokenType::IDENTIFIER, "Expected enum name");
    std::string enumName = std::get<std::string>(previous().value);
    consume(TokenType::LBRACE, "Expected '{' after enum name");
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
    int64_t value = 0;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        while (match(TokenType::NEWLINE)) {}
        consume(TokenType::IDENTIFIER, "Expected enum member name");
        std::string member = std::get<std::string>(previous().value);
        int64_t memberValue = value;
        if (match(TokenType::ASSIGN)) {
            consume(TokenType::INTEGER, "Expected integer literal after '=' in enum");
            memberValue = std::get<int64_t>(previous().value);
        }
        auto key = std::make_unique<StringLiteral>(member);
        auto val = std::make_unique<IntLiteral>(memberValue);
        entries.push_back({std::move(key), std::move(val)});
        value = memberValue + 1;
        if (!match(TokenType::COMMA)) break;
    }
    while (match(TokenType::NEWLINE)) {}
    consume(TokenType::RBRACE, "Expected '}' after enum body");
    auto init = std::make_unique<MapLiteral>(std::move(entries));
    return std::make_unique<VarDeclStmt>(enumName, true, false, "", std::move(init));
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
    if (match(TokenType::AS)) {
        consume(TokenType::IDENTIFIER, "Expected alias name");
        alias = std::get<std::string>(previous().value);
        hasAlias = true;
    }
    return std::make_unique<ImportStmt>(name, alias, hasAlias);
}

StmtPtr Parser::fromImportStatement() {
    std::string name;
    if (match(TokenType::LPAREN)) {
        if (!match(TokenType::STRING))
            throw ParserError("Expected quoted module name after 'from'", peek().line, peek().column);
        name = std::get<std::string>(previous().value);
        consume(TokenType::RPAREN, "Expected ')' after from module path");
    } else if (match(TokenType::STRING)) {
        name = std::get<std::string>(previous().value);
    } else {
        throw ParserError("Expected module path: from \"math\" import ...", peek().line, peek().column);
    }
    if (!match(TokenType::IMPORT))
        throw ParserError("Expected 'import' after module path in from-import", peek().line, peek().column);
    std::vector<std::string> names;
    consume(TokenType::IDENTIFIER, "Expected name to import");
    names.push_back(std::get<std::string>(previous().value));
    while (match(TokenType::COMMA)) {
        consume(TokenType::IDENTIFIER, "Expected name in import list");
        names.push_back(std::get<std::string>(previous().value));
    }
    return std::make_unique<ImportStmt>(name, "", false, std::move(names));
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
        check(TokenType::LONG) || check(TokenType::DOUBLE) || check(TokenType::PTR) ||
        check(TokenType::REF)) {
        advance();
        hasType = true;
        typeName = previous().lexeme;
    }
    consume(TokenType::IDENTIFIER, "Expected variable name");
    std::string name = std::get<std::string>(previous().value);
    // optional ": Type" after name (Rust/TypeScript style)
    if (match(TokenType::COLON)) {
        bool ptrType = match(TokenType::STAR);
        if (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
            check(TokenType::STRING_TYPE) || check(TokenType::VOID) || check(TokenType::CHAR) ||
            check(TokenType::LONG) || check(TokenType::DOUBLE) || check(TokenType::PTR) ||
            check(TokenType::REF) || check(TokenType::IDENTIFIER)) {
            advance();
            hasType = true;
            typeName = ptrType ? ("*" + previous().lexeme) : previous().lexeme;
        } else if (ptrType) {
            throw ParserError("Expected pointer base type after '*'", peek().line, peek().column);
        }
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
        check(TokenType::BOOL) || check(TokenType::STRING_TYPE) || check(TokenType::VOID) ||
        check(TokenType::CHAR) || check(TokenType::LONG) || check(TokenType::DOUBLE) ||
        check(TokenType::PTR) || check(TokenType::REF) || check(TokenType::IDENTIFIER) ||
        check(TokenType::STAR)) {
        bool ptrType = false;
        if (match(TokenType::COLON)) ptrType = match(TokenType::STAR);
        else if (match(TokenType::STAR)) ptrType = true;
        if (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
            check(TokenType::STRING_TYPE) || check(TokenType::VOID) || check(TokenType::CHAR) ||
            check(TokenType::LONG) || check(TokenType::DOUBLE) || check(TokenType::PTR) ||
            check(TokenType::REF) || check(TokenType::IDENTIFIER)) {
            advance();
            returnType = ptrType ? ("*" + previous().lexeme) : previous().lexeme;
            hasReturnType = true;
        }
    }
    bool isExport = check(TokenType::EXPORT);
    if (isExport) advance();
    StmtPtr body;
    funcBodyDepth_++;
    try {
        if (match(TokenType::COLON)) {
            body = std::make_unique<BlockStmt>();
            if (peek().type != TokenType::NEWLINE && peek().type != TokenType::END_OF_FILE)
                static_cast<BlockStmt*>(body.get())->statements.push_back(expressionStatement());
        } else {
            consume(TokenType::LBRACE, "Expected '{' or ':' for function body");
            body = blockStatement();
        }
    } catch (...) {
        funcBodyDepth_--;
        throw;
    }
    funcBodyDepth_--;
    return std::make_unique<FunctionDeclStmt>(name, std::move(params), returnType, hasReturnType, std::move(body), isExport);
}

std::vector<Param> Parser::parameterList() {
    std::vector<Param> params;
    while (!check(TokenType::RPAREN)) {
        if (check(TokenType::ELLIPSIS)) { advance(); params.emplace_back("...", "", nullptr); break; }
        std::string typeName;
        if (check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
            check(TokenType::STRING_TYPE) || check(TokenType::VOID) || check(TokenType::CHAR) ||
            check(TokenType::LONG) || check(TokenType::DOUBLE) || check(TokenType::PTR) ||
            check(TokenType::REF) || check(TokenType::STAR)) {
            bool ptrType = false;
            if (match(TokenType::STAR)) ptrType = true;
            else advance();
            if (!ptrType) {
                typeName = previous().lexeme;
            } else {
                if (!(check(TokenType::INT) || check(TokenType::FLOAT_TYPE) || check(TokenType::BOOL) ||
                    check(TokenType::STRING_TYPE) || check(TokenType::VOID) || check(TokenType::CHAR) ||
                    check(TokenType::LONG) || check(TokenType::DOUBLE) || check(TokenType::PTR) ||
                    check(TokenType::REF) || check(TokenType::IDENTIFIER))) {
                    throw ParserError("Expected pointer base type after '*'", peek().line, peek().column);
                }
                advance();
                typeName = "*" + previous().lexeme;
            }
        }
        consume(TokenType::IDENTIFIER, "Expected parameter name");
        std::string name = std::get<std::string>(previous().value);
        ExprPtr defaultExpr = nullptr;
        if (match(TokenType::ASSIGN)) defaultExpr = expression();
        params.emplace_back(std::move(name), std::move(typeName), std::move(defaultExpr));
        if (!match(TokenType::COMMA)) break;
    }
    return params;
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
            funcBodyDepth_++;
            try {
                if (match(TokenType::LBRACE)) initBody = blockStatement();
                else {
                    consume(TokenType::COLON, "Expected body");
                    initBody = std::make_unique<BlockStmt>();
                }
            } catch (...) {
                funcBodyDepth_--;
                throw;
            }
            funcBodyDepth_--;
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
            funcBodyDepth_++;
            try {
                if (match(TokenType::LBRACE)) body = blockStatement();
                else {
                    consume(TokenType::COLON, "Expected body");
                    body = std::make_unique<BlockStmt>();
                }
            } catch (...) {
                funcBodyDepth_--;
                throw;
            }
            funcBodyDepth_--;
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
            funcBodyDepth_++;
            try {
                if (match(TokenType::LBRACE)) body = blockStatement();
                else {
                    consume(TokenType::COLON, "Expected body");
                    body = std::make_unique<BlockStmt>();
                }
            } catch (...) {
                funcBodyDepth_--;
                throw;
            }
            funcBodyDepth_--;
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
        std::vector<CallArg> args;
        if (match(TokenType::LPAREN)) {
            args.push_back(CallArg{"", expression(), false});
            while (match(TokenType::COMMA)) args.push_back(CallArg{"", expression(), false});
            consume(TokenType::RPAREN, "Expected ')' after print arguments");
        } else {
            args.push_back(CallArg{"", expression(), false});
            while (match(TokenType::COMMA)) args.push_back(CallArg{"", expression(), false});
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
    if (match(TokenType::UNSAFE)) return unsafeStatement();
    if (match(TokenType::YIELD)) {
        ExprPtr val = nullptr;
        if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) && !check(TokenType::RBRACE))
            val = expression();
        return std::make_unique<YieldStmt>(std::move(val));
    }
    if (match(TokenType::SPAWN)) {
        std::vector<CallArg> args;
        args.push_back(CallArg{"", expression(), false});
        return std::make_unique<ExprStmt>(
            std::make_unique<CallExpr>(std::make_unique<Identifier>("__spawn_task"), std::move(args)));
    }
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

StmtPtr Parser::unsafeStatement() {
    StmtPtr body;
    if (match(TokenType::LBRACE)) body = blockStatement();
    else {
        consume(TokenType::COLON, "Expected '{' or ':' after unsafe");
        body = std::make_unique<BlockStmt>();
        static_cast<BlockStmt*>(body.get())->statements.push_back(statement());
    }
    return std::make_unique<UnsafeBlockStmt>(std::move(body));
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
    if (check(TokenType::LBRACE)) {
        consume(TokenType::LBRACE, "Expected '{' after repeat");
        auto body = std::make_unique<BlockStmt>();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (peek().type == TokenType::NEWLINE) { advance(); continue; }
            if (peek().type == TokenType::SEMICOLON) { advance(); continue; }
            body->statements.push_back(statement());
        }
        consume(TokenType::RBRACE, "Expected '}' after repeat body");
        while (match(TokenType::NEWLINE)) {}
        consume(TokenType::WHILE, "Expected 'while' after repeat body");
        consume(TokenType::LPAREN, "Expected '('");
        ExprPtr cond = expression();
        consume(TokenType::RPAREN, "Expected ')'");
        return std::make_unique<RepeatWhileStmt>(std::move(body), std::move(cond));
    }
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

StmtPtr Parser::withStatement() {
    ExprPtr resource = expression();
    std::string asName;
    if (match(TokenType::AS)) {
        consume(TokenType::IDENTIFIER, "Expected binding name after 'as'");
        asName = std::get<std::string>(previous().value);
    }
    StmtPtr body;
    if (match(TokenType::LBRACE)) body = blockStatement();
    else {
        consume(TokenType::COLON, "Expected '{' or ':' after with expression");
        body = std::make_unique<BlockStmt>();
        static_cast<BlockStmt*>(body.get())->statements.push_back(statement());
    }
    auto outer = std::make_unique<BlockStmt>();
    outer->statements.push_back(std::make_unique<VarDeclStmt>("__with_r", false, false, "", std::move(resource)));
    auto tryBody = std::make_unique<BlockStmt>();
    if (!asName.empty())
        tryBody->statements.push_back(std::make_unique<VarDeclStmt>(asName, false, false, "", std::make_unique<Identifier>("__with_r")));
    if (auto* bb = dynamic_cast<BlockStmt*>(body.get())) {
        for (auto& st : bb->statements) tryBody->statements.push_back(std::move(st));
    } else {
        tryBody->statements.push_back(std::move(body));
    }
    std::vector<CallArg> wcArgs;
    wcArgs.push_back(CallArg{"", std::make_unique<Identifier>("__with_r"), false});
    auto fin = std::make_unique<BlockStmt>();
    fin->statements.push_back(std::make_unique<ExprStmt>(std::make_unique<CallExpr>(
        std::make_unique<Identifier>("with_cleanup"), std::move(wcArgs))));
    outer->statements.push_back(std::make_unique<TryStmt>(std::move(tryBody), "", "", nullptr, nullptr, std::move(fin)));
    return outer;
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
    if (funcBodyDepth_ <= 0) {
        throw ParserError(
            "'return' is only allowed inside a function or method — not at module or class "
            "body scope. Use if/else instead of early return, or move logic into a `def`.",
            previous().line, previous().column);
    }
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
    if (match(TokenType::COALESCE_EQ) || match(TokenType::ASSIGN) || match(TokenType::PLUS_EQ) || match(TokenType::MINUS_EQ) ||
        match(TokenType::STAR_EQ) || match(TokenType::SLASH_EQ) || match(TokenType::PERCENT_EQ)) {
        TokenType op = previous().type;
        ExprPtr value = assignment();
        if (dynamic_cast<Identifier*>(expr.get())) {
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        if (dynamic_cast<MemberExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        if (dynamic_cast<IndexExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
        }
        if (auto* un = dynamic_cast<UnaryExpr*>(expr.get())) {
            if (un->op == TokenType::STAR) {
                return std::make_unique<AssignExpr>(std::move(expr), std::move(value), op);
            }
        }
        throw ParserError("Invalid assignment target", peek().line, peek().column);
    }
    return expr;
}

ExprPtr Parser::coalesce() {
    ExprPtr left = parsePrecedence(2);
    // left-associative chain (matches JS/C#); iterative avoids stack overflow on huge `a ?? b ?? …` inputs.
    while (match(TokenType::COALESCE)) {
        ExprPtr right = parsePrecedence(2);
        left = std::make_unique<CoalesceExpr>(std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::pipeline() {
    ExprPtr left = coalesce();
    while (match(TokenType::PIPE)) {
        const Token& pipeTok = previous();
        ExprPtr right = coalesce();
        auto pipe = std::make_unique<PipelineExpr>(std::move(left), std::move(right));
        pipe->line = pipeTok.line;
        pipe->column = pipeTok.column;
        left = std::move(pipe);
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
    // keep spawn(expr) callable as a normal identifier function call.
    // treat spawn as unary sugar only for spawn <expr>.
    if (isIdentifierToken(peek(), "spawn") &&
        !(current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == TokenType::LPAREN)) {
        advance();
        std::vector<CallArg> args;
        args.push_back(CallArg{"", unary(), false});
        return std::make_unique<CallExpr>(std::make_unique<Identifier>("__spawn_task"), std::move(args));
    }
    if (isIdentifierToken(peek(), "await")) {
        advance();
        const Token& awaitTok = previous();
        auto aw = std::make_unique<AwaitExpr>(unary());
        aw->line = awaitTok.line;
        aw->column = awaitTok.column;
        return aw;
    }
    // fold repeated prefix ops so adversarial `!!!!…x` cannot blow the C++ stack.
    std::vector<TokenType> prefixOps;
    while (match(TokenType::NOT) || match(TokenType::MINUS) || match(TokenType::STAR))
        prefixOps.push_back(previous().type);
    ExprPtr expr = postfix();
    for (auto it = prefixOps.rbegin(); it != prefixOps.rend(); ++it)
        expr = std::make_unique<UnaryExpr>(*it, std::move(expr));
    return expr;
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
            const Token& openParen = previous();
            auto args = argumentList();
            auto call = std::make_unique<CallExpr>(std::move(expr), std::move(args));
            call->line = openParen.line;
            call->column = openParen.column;
            expr = std::move(call);
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
            else if (match(TokenType::SPAWN) || match(TokenType::AWAIT) || match(TokenType::ASYNC))
                memberName = previous().lexeme;
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
        std::vector<CallArg> args;
        args.push_back(CallArg{"", std::make_unique<StringLiteral>(name), false});
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
        while (match(TokenType::NEWLINE)) {}
        consume(TokenType::RBRACKET, "Expected ']' after array elements");
        return std::make_unique<ArrayLiteral>(std::move(elements));
    }
    if (match(TokenType::LBRACE)) {
        while (match(TokenType::NEWLINE)) {}
        if (check(TokenType::RBRACE)) {
            consume(TokenType::RBRACE, "Expected '}'");
            return std::make_unique<MapLiteral>();
        }
        ExprPtr key = expression();
        consume(TokenType::COLON, "Expected ':' in map literal");
        ExprPtr val = expression();
        if (match(TokenType::FOR)) {
            consume(TokenType::IDENTIFIER, "Expected variable name in map comprehension");
            std::string varName = std::get<std::string>(previous().value);
            consume(TokenType::IN, "Expected 'in' in map comprehension");
            ExprPtr iterExpr = expression();
            ExprPtr filterExpr = nullptr;
            if (match(TokenType::IF)) filterExpr = expression();
            consume(TokenType::RBRACE, "Expected '}' after map comprehension");
            return std::make_unique<MapComprehensionExpr>(std::move(key), std::move(val), std::move(varName), std::move(iterExpr), std::move(filterExpr));
        }
        std::vector<std::pair<ExprPtr, ExprPtr>> entries;
        entries.push_back({std::move(key), std::move(val)});
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            while (match(TokenType::NEWLINE)) {}
            if (check(TokenType::RBRACE)) break;
            if (!match(TokenType::COMMA)) break;
            if (check(TokenType::RBRACE)) break; // trailing comma after entry
            ExprPtr k = expression();
            consume(TokenType::COLON, "Expected ':' in map literal");
            ExprPtr v = expression();
            entries.push_back({std::move(k), std::move(v)});
            // don't consume a trailing comma here: the next loop iteration expects that comma
            // as its leading separator (line below). Consuming twice skipped every other entry.
        }
        while (match(TokenType::NEWLINE)) {}
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
    funcBodyDepth_++;
    try {
        if (match(TokenType::LBRACE)) body = blockStatement();
        else body = std::make_unique<ExprStmt>(expression());
    } catch (...) {
        funcBodyDepth_--;
        throw;
    }
    funcBodyDepth_--;
    return std::make_unique<LambdaExpr>(params, std::move(body));
}

std::vector<CallArg> Parser::argumentList() {
    std::vector<CallArg> args;
    while (!check(TokenType::RPAREN)) {
        while (match(TokenType::NEWLINE)) {}
        if (check(TokenType::ELLIPSIS)) {
            advance();
            ExprPtr e = expression();
            args.push_back(CallArg{"", std::move(e), true});
        } else if (check(TokenType::IDENTIFIER) && current_ + 1 < tokens_.size() && tokens_[current_ + 1].type == TokenType::ASSIGN) {
            advance();
            std::string name = std::holds_alternative<std::string>(previous().value) ? std::get<std::string>(previous().value) : "";
            advance();  // consume ASSIGN
            ExprPtr e = expression();
            args.push_back(CallArg{std::move(name), std::move(e), false});
        } else {
            args.push_back(CallArg{"", expression(), false});
        }
        if (!match(TokenType::COMMA)) break;
    }
    consume(TokenType::RPAREN, "Expected ')'");
    return args;
}

} // namespace kern
