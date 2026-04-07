/* *
 * kern Lexer - Lexical Analyzer
 * tokenizes Kern source code into a stream of tokens.
 */

#ifndef KERN_LEXER_HPP
#define KERN_LEXER_HPP

#include "token.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace kern {

class LexerError : public std::runtime_error {
public:
    int line, column;
    LexerError(const std::string& msg, int ln, int col)
        : std::runtime_error(msg), line(ln), column(col) {}
};

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source_;
    size_t start_;
    size_t current_;
    int line_;
    int column_;
    std::vector<Token> tokens_;
    static std::unordered_map<std::string, TokenType> keywords_;

    char peek() const;
    char peekNext() const;
    bool isAtEnd() const;
    char advance();
    bool match(char c);
    void addToken(TokenType type);
    void addToken(TokenType type, int64_t value);
    void addToken(TokenType type, double value);
    void addToken(TokenType type, const std::string& value);
    void scanToken();
    void number();
    void stringLiteral(char quote);
    void multilineString(char quote);
    void identifier();
    void blockComment();
    void lineComment();
};

} // namespace kern

#endif // kERN_LEXER_HPP
