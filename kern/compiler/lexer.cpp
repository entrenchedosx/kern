/* *
 * kern/compiler/lexer.cpp - Scanner (Lexer) implementation for raw text processing
 */

#include "lexer.hpp"
#include <stdexcept>
#include <cctype>

namespace kern {

Lexer::Lexer(const std::string& source) 
    : source(source), start(0), current(0), line(1), column(1) {}

char Lexer::advance() {
    if (current >= source.size()) return '\0';
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

char Lexer::peek() const {
    if (current >= source.size()) return '\0';
    return source[current];
}

bool Lexer::isAtEnd() const {
    return current >= source.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') {
            advance();
        } else if (c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

bool Lexer::isDigit(char c) const {
    return std::isdigit(static_cast<unsigned char>(c));
}

bool Lexer::isAlpha(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

Token Lexer::number() {
    while (isDigit(peek())) advance();
    
    // Check for decimal point
    if (peek() == '.' && isDigit(source[current + 1])) {
        advance(); // consume the dot
        while (isDigit(peek())) advance();
    }
    
    std::string text = source.substr(start, current - start);
    return Token(TokenType::NUMBER, text, line, column - static_cast<int>(text.length()));
}

Token Lexer::identifier() {
    while (isAlphaNumeric(peek())) advance();
    std::string text = source.substr(start, current - start);
    return Token(TokenType::IDENTIFIER, text, line, column - static_cast<int>(text.length()));
}

Token Lexer::singleCharToken(TokenType type) {
    std::string text = source.substr(start, 1);
    return Token(type, text, line, column - 1);
}

Token Lexer::twoCharToken(TokenType type, char second) {
    std::string text = source.substr(start, 2);
    return Token(type, text, line, column - 2);
}

Token Lexer::errorToken(const std::string& message) {
    return Token(TokenType::ILLEGAL, message, line, column);
}

Token Lexer::scanToken() {
    skipWhitespace();
    start = current;
    
    if (isAtEnd()) return Token(TokenType::EOF_TOKEN, "", line, column);
    
    char c = advance();
    
    // Numbers
    if (isDigit(c)) return number();
    
    // Identifiers
    if (isAlpha(c)) return identifier();
    
    // Single character tokens
    switch (c) {
        case '+': return singleCharToken(TokenType::PLUS);
        case '-': return singleCharToken(TokenType::MINUS);
        case '*': return singleCharToken(TokenType::MULTIPLY);
        case '/': return singleCharToken(TokenType::DIVIDE);
        case '(': return singleCharToken(TokenType::LPAREN);
        case ')': return singleCharToken(TokenType::RPAREN);
        case '{': return singleCharToken(TokenType::LBRACE);
        case '}': return singleCharToken(TokenType::RBRACE);
        case ';': return singleCharToken(TokenType::SEMICOLON);
        case ',': return singleCharToken(TokenType::COMMA);
        case '.': return singleCharToken(TokenType::DOT);
        case '!':
            if (match('=')) return twoCharToken(TokenType::NOT_EQUAL, '=');
            return singleCharToken(TokenType::NOT);
        case '=':
            if (match('=')) return twoCharToken(TokenType::EQUAL, '=');
            return singleCharToken(TokenType::EQUAL);
        case '<':
            if (match('=')) return twoCharToken(TokenType::LESS_EQUAL, '=');
            return singleCharToken(TokenType::LESS);
        case '>':
            if (match('=')) return twoCharToken(TokenType::GREATER_EQUAL, '=');
            return singleCharToken(TokenType::GREATER);
        case '&':
            if (match('&')) return twoCharToken(TokenType::AND, '&');
            return errorToken("Unexpected '&'");
        case '|':
            if (match('|')) return twoCharToken(TokenType::OR, '|');
            return errorToken("Unexpected '|'");
        default:
            return errorToken(std::string("Unexpected character: '") + c + "'");
    }
}

std::vector<Token> Lexer::scanTokens() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        Token token = scanToken();
        if (token.type != TokenType::EOF_TOKEN) {
            tokens.push_back(token);
        } else {
            tokens.push_back(token); // Always add EOF token
            break;
        }
    }
    
    return tokens;
}

} // namespace kern
