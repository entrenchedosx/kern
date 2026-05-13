/* *
 * kern/compiler/minimal_codegen.hpp - Minimal Code Generator
 * 
 * Generates bytecode for simple Kern programs.
 * Supports: print, variables, arithmetic, if/else, loops
 */
#pragma once

#include "../core/bytecode/bytecode.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>

namespace kern {

// Simple tokenizer
enum class TokenType {
    EOF_TOKEN,
    IDENTIFIER,
    NUMBER,
    STRING,
    PRINT,
    LET,
    IF,
    ELSE,
    WHILE,
    FOR,
    DEF,
    RETURN,
    TRUE,
    FALSE,
    NIL,
    PLUS, MINUS, MUL, DIV, MOD,
    EQ, NE, LT, LE, GT, GE,
    ASSIGN,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    COMMA, SEMICOLON,
    AND, OR, NOT
};

struct Token {
    TokenType type;
    std::string text;
    int line;
};

class MinimalLexer {
    std::string source;
    size_t pos;
    int line;
    
public:
    explicit MinimalLexer(const std::string& src) : source(src), pos(0), line(1) {}
    
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        
        while (pos < source.size()) {
            skipWhitespace();
            if (pos >= source.size()) break;
            
            char c = source[pos];
            
            // Numbers
            if (isdigit(c) || (c == '.' && peek() && isdigit(*peek()))) {
                tokens.push_back(readNumber());
                continue;
            }
            
            // Identifiers and keywords
            if (isalpha(c) || c == '_') {
                tokens.push_back(readIdentifier());
                continue;
            }
            
            // Strings
            if (c == '"') {
                tokens.push_back(readString());
                continue;
            }
            
            // Operators and punctuation
            switch (c) {
                case '+': tokens.push_back({TokenType::PLUS, "+", line}); pos++; break;
                case '-': tokens.push_back({TokenType::MINUS, "-", line}); pos++; break;
                case '*': tokens.push_back({TokenType::MUL, "*", line}); pos++; break;
                case '/': tokens.push_back({TokenType::DIV, "/", line}); pos++; break;
                case '%': tokens.push_back({TokenType::MOD, "%", line}); pos++; break;
                case '=':
                    if (peek() && *peek() == '=') {
                        tokens.push_back({TokenType::EQ, "==", line}); pos += 2;
                    } else {
                        tokens.push_back({TokenType::ASSIGN, "=", line}); pos++;
                    }
                    break;
                case '!':
                    if (peek() && *peek() == '=') {
                        tokens.push_back({TokenType::NE, "!=", line}); pos += 2;
                    } else {
                        tokens.push_back({TokenType::NOT, "!", line}); pos++;
                    }
                    break;
                case '<':
                    if (peek() && *peek() == '=') {
                        tokens.push_back({TokenType::LE, "<=", line}); pos += 2;
                    } else {
                        tokens.push_back({TokenType::LT, "<", line}); pos++;
                    }
                    break;
                case '>':
                    if (peek() && *peek() == '=') {
                        tokens.push_back({TokenType::GE, ">=", line}); pos += 2;
                    } else {
                        tokens.push_back({TokenType::GT, ">", line}); pos++;
                    }
                    break;
                case '&':
                    if (peek() && *peek() == '&') {
                        tokens.push_back({TokenType::AND, "&&", line}); pos += 2;
                    }
                    break;
                case '|':
                    if (peek() && *peek() == '|') {
                        tokens.push_back({TokenType::OR, "||", line}); pos += 2;
                    }
                    break;
                case '(': tokens.push_back({TokenType::LPAREN, "(", line}); pos++; break;
                case ')': tokens.push_back({TokenType::RPAREN, ")", line}); pos++; break;
                case '{': tokens.push_back({TokenType::LBRACE, "{", line}); pos++; break;
                case '}': tokens.push_back({TokenType::RBRACE, "}", line}); pos++; break;
                case '[': tokens.push_back({TokenType::LBRACKET, "[", line}); pos++; break;
                case ']': tokens.push_back({TokenType::RBRACKET, "]", line}); pos++; break;
                case ',': tokens.push_back({TokenType::COMMA, ",", line}); pos++; break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";", line}); pos++; break;
                case '#': skipComment(); break;
                default: pos++; break;  // Skip unknown
            }
        }
        
        tokens.push_back({TokenType::EOF_TOKEN, "", line});
        return tokens;
    }
    
private:
    void skipWhitespace() {
        while (pos < source.size() && isspace(source[pos])) {
            if (source[pos] == '\n') line++;
            pos++;
        }
    }
    
    void skipComment() {
        while (pos < source.size() && source[pos] != '\n') {
            pos++;
        }
    }
    
    std::optional<char> peek() {
        if (pos + 1 < source.size()) return source[pos + 1];
        return std::nullopt;
    }
    
    Token readNumber() {
        size_t start = pos;
        while (pos < source.size() && (isdigit(source[pos]) || source[pos] == '.')) {
            pos++;
        }
        return {TokenType::NUMBER, source.substr(start, pos - start), line};
    }
    
    Token readIdentifier() {
        size_t start = pos;
        while (pos < source.size() && (isalnum(source[pos]) || source[pos] == '_')) {
            pos++;
        }
        std::string text = source.substr(start, pos - start);
        
        // Keywords
        if (text == "print") return {TokenType::PRINT, text, line};
        if (text == "let") return {TokenType::LET, text, line};
        if (text == "if") return {TokenType::IF, text, line};
        if (text == "else") return {TokenType::ELSE, text, line};
        if (text == "while") return {TokenType::WHILE, text, line};
        if (text == "for") return {TokenType::FOR, text, line};
        if (text == "def") return {TokenType::DEF, text, line};
        if (text == "return") return {TokenType::RETURN, text, line};
        if (text == "true") return {TokenType::TRUE, text, line};
        if (text == "false") return {TokenType::FALSE, text, line};
        if (text == "nil") return {TokenType::NIL, text, line};
        
        return {TokenType::IDENTIFIER, text, line};
    }
    
    Token readString() {
        pos++;  // Skip opening quote
        size_t start = pos;
        while (pos < source.size() && source[pos] != '"') {
            if (source[pos] == '\\' && pos + 1 < source.size()) {
                pos += 2;  // Skip escape
            } else {
                pos++;
            }
        }
        std::string text = source.substr(start, pos - start);
        if (pos < source.size()) pos++;  // Skip closing quote
        return {TokenType::STRING, text, line};
    }
};

// Simple code generator
class MinimalCodeGen {
    std::vector<Token> tokens;
    size_t pos;
    int localCount;
    std::unordered_map<std::string, int> locals;
    std::vector<std::string> constants;
    
public:
    Bytecode compile(const std::string& source) {
        MinimalLexer lexer(source);
        tokens = lexer.tokenize();
        pos = 0;
        localCount = 0;
        locals.clear();
        constants.clear();
        
        Bytecode code;
        
        while (!check(TokenType::EOF_TOKEN)) {
            compileStatement(code);
        }
        
        code.push_back({Opcode::HALT, (int64_t)0});
        return code;
    }
    
    const std::vector<std::string>& getConstants() const { return constants; }
    
private:
    bool check(TokenType type) const {
        return pos < tokens.size() && tokens[pos].type == type;
    }
    
    bool match(TokenType type) {
        if (check(type)) {
            pos++;
            return true;
        }
        return false;
    }
    
    const Token& current() const {
        return tokens[pos];
    }
    
    const Token& consume(TokenType type, const std::string& msg) {
        if (!check(type)) {
            throw std::runtime_error(msg + " at line " + std::to_string(current().line));
        }
        return tokens[pos++];
    }
    
    void compileStatement(Bytecode& code) {
        if (match(TokenType::PRINT)) {
            compilePrint(code);
        } else if (match(TokenType::LET)) {
            compileLet(code);
        } else if (match(TokenType::IF)) {
            compileIf(code);
        } else if (match(TokenType::WHILE)) {
            compileWhile(code);
        } else if (match(TokenType::LBRACE)) {
            compileBlock(code);
        } else {
            compileExpression(code);
            match(TokenType::SEMICOLON);  // Optional
        }
    }
    
    void compilePrint(Bytecode& code) {
        compileExpression(code);
        code.push_back({Opcode::PRINT, (int64_t)0});
        match(TokenType::SEMICOLON);
    }
    
    void compileLet(Bytecode& code) {
        std::string name = consume(TokenType::IDENTIFIER, "Expected variable name").text;
        consume(TokenType::ASSIGN, "Expected '='");
        compileExpression(code);
        
        // Store in locals or globals
        auto it = locals.find(name);
        if (it != locals.end()) {
            code.push_back({Opcode::STORE, (int64_t)it->second});
        } else {
            int idx = addConstant(name);
            locals[name] = localCount++;
            code.push_back({Opcode::STORE, (int64_t)locals[name]});
        }
        match(TokenType::SEMICOLON);
    }
    
    void compileIf(Bytecode& code) {
        consume(TokenType::LPAREN, "Expected '(' after 'if'");
        compileExpression(code);
        consume(TokenType::RPAREN, "Expected ')'");
        
        // Jump to else if false
        size_t jumpElseIdx = code.size();
        code.push_back({Opcode::JMP_IF_FALSE, (size_t)0});  // Placeholder
        
        compileStatement(code);
        
        if (match(TokenType::ELSE)) {
            size_t jumpEndIdx = code.size();
            code.push_back({Opcode::JMP, (size_t)0});  // Placeholder
            
            // Patch else jump - use absolute target (1-based)
            code[jumpElseIdx].operand = (size_t)(code.size());
            
            compileStatement(code);
            
            // Patch end jump - use absolute target (1-based)
            code[jumpEndIdx].operand = (size_t)(code.size());
        } else {
            // Patch else jump (no else) - use absolute target (1-based)
            code[jumpElseIdx].operand = (size_t)(code.size());
        }
    }
    
    void compileWhile(Bytecode& code) {
        size_t loopStart = code.size();
        
        consume(TokenType::LPAREN, "Expected '(' after 'while'");
        compileExpression(code);
        consume(TokenType::RPAREN, "Expected ')'");
        
        size_t jumpEndIdx = code.size();
        code.push_back({Opcode::JMP_IF_FALSE, (size_t)0});  // Placeholder
        
        compileStatement(code);
        
        // Jump back to start - use absolute target (1-based)
        code.push_back({Opcode::JMP, (size_t)(loopStart + 1)});
        
        // Patch exit jump - use absolute target (1-based)
        code[jumpEndIdx].operand = (size_t)(code.size());
    }
    
    void compileBlock(Bytecode& code) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
            compileStatement(code);
        }
        consume(TokenType::RBRACE, "Expected '}'");
    }
    
    void compileExpression(Bytecode& code) {
        compileOr(code);
    }
    
    void compileOr(Bytecode& code) {
        compileAnd(code);
        while (match(TokenType::OR)) {
            compileAnd(code);
            // Or is short-circuit - simplified
        }
    }
    
    void compileAnd(Bytecode& code) {
        compileEquality(code);
        while (match(TokenType::AND)) {
            compileEquality(code);
            // And is short-circuit - simplified
        }
    }
    
    void compileEquality(Bytecode& code) {
        compileComparison(code);
        while (true) {
            if (match(TokenType::EQ)) {
                compileComparison(code);
                code.push_back({Opcode::EQ, (int64_t)0});
            } else if (match(TokenType::NE)) {
                compileComparison(code);
                code.push_back({Opcode::EQ, (int64_t)0});
                code.push_back({Opcode::NOT, (int64_t)0});
            } else {
                break;
            }
        }
    }
    
    void compileComparison(Bytecode& code) {
        compileTerm(code);
        while (true) {
            if (match(TokenType::LT)) {
                compileTerm(code);
                code.push_back({Opcode::LT, (int64_t)0});
            } else if (match(TokenType::LE)) {
                compileTerm(code);
                code.push_back({Opcode::LE, (int64_t)0});
            } else if (match(TokenType::GT)) {
                compileTerm(code);
                code.push_back({Opcode::GT, (int64_t)0});
            } else if (match(TokenType::GE)) {
                compileTerm(code);
                code.push_back({Opcode::GE, (int64_t)0});
            } else {
                break;
            }
        }
    }
    
    void compileTerm(Bytecode& code) {
        compileFactor(code);
        while (true) {
            if (match(TokenType::PLUS)) {
                compileFactor(code);
                code.push_back({Opcode::ADD, (int64_t)0});
            } else if (match(TokenType::MINUS)) {
                compileFactor(code);
                code.push_back({Opcode::SUB, (int64_t)0});
            } else {
                break;
            }
        }
    }
    
    void compileFactor(Bytecode& code) {
        compileUnary(code);
        while (true) {
            if (match(TokenType::MUL)) {
                compileUnary(code);
                code.push_back({Opcode::MUL, (int64_t)0});
            } else if (match(TokenType::DIV)) {
                compileUnary(code);
                code.push_back({Opcode::DIV, (int64_t)0});
            } else if (match(TokenType::MOD)) {
                compileUnary(code);
                code.push_back({Opcode::MOD, (int64_t)0});
            } else {
                break;
            }
        }
    }
    
    void compileUnary(Bytecode& code) {
        if (match(TokenType::MINUS)) {
            compileUnary(code);
            code.push_back({Opcode::NEG, (int64_t)0});
        } else if (match(TokenType::NOT)) {
            compileUnary(code);
            code.push_back({Opcode::NOT, (int64_t)0});
        } else {
            compilePrimary(code);
        }
    }
    
    void compilePrimary(Bytecode& code) {
        if (match(TokenType::TRUE)) {
            code.push_back({Opcode::CONST_TRUE, (int64_t)0});
        } else if (match(TokenType::FALSE)) {
            code.push_back({Opcode::CONST_FALSE, (int64_t)0});
        } else if (match(TokenType::NIL)) {
            code.push_back({Opcode::CONST_NULL, (int64_t)0});
        } else if (match(TokenType::NUMBER)) {
            std::string num = tokens[pos - 1].text;
            if (num.find('.') != std::string::npos) {
                // Float - push as constant for now
                int idx = addConstant(num);
                code.push_back({Opcode::CONST_STR, (size_t)idx});
            } else {
                // Integer - compile-time constant
                int64_t val = std::stoll(num);
                // For now, push as constant
                int idx = addConstant(num);
                code.push_back({Opcode::CONST_I64, (int64_t)idx});
            }
        } else if (match(TokenType::STRING)) {
            int idx = addConstant(tokens[pos - 1].text);
            code.push_back({Opcode::CONST_STR, (size_t)idx});
        } else if (match(TokenType::IDENTIFIER)) {
            std::string name = tokens[pos - 1].text;
            auto it = locals.find(name);
            if (it != locals.end()) {
                code.push_back({Opcode::LOAD, (int64_t)it->second});
            } else {
                int idx = addConstant(name);
                code.push_back({Opcode::LOAD_GLOBAL, (size_t)idx});
            }
        } else if (match(TokenType::LPAREN)) {
            compileExpression(code);
            consume(TokenType::RPAREN, "Expected ')'");
        } else {
            throw std::runtime_error("Unexpected token: " + current().text);
        }
    }
    
    int addConstant(const std::string& s) {
        for (size_t i = 0; i < constants.size(); i++) {
            if (constants[i] == s) return (int)i;
        }
        constants.push_back(s);
        return (int)(constants.size() - 1);
    }
};

} // namespace kern
