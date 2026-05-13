/* *
 * kern/compiler/lexer.hpp - Scanner (Lexer) for raw text processing
 */

#ifndef KERN_LEXER_HPP
#define KERN_LEXER_HPP

#include <string>
#include <vector>
#include <cstddef>

namespace kern {

// Token types as specified in the plan
enum class TokenType {
    NUMBER, IDENTIFIER, PLUS, MINUS, MULTIPLY, DIVIDE,
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
    AND, OR, NOT, LPAREN, RPAREN, LBRACE, RBRACE,
    SEMICOLON, COMMA, DOT, EOF_TOKEN, ILLEGAL
};

// Token structure with position information
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    
    Token(TokenType t = TokenType::ILLEGAL, const std::string& lex = "", int l = 1, int c = 1)
        : type(t), lexeme(lex), line(l), column(c) {}
};

// Lexer class for scanning source code
class Lexer {
private:
    std::string source;
    size_t start;
    size_t current;
    int line;
    int column;
    
    // Helper methods
    char advance();
    char peek() const;
    bool isAtEnd() const;
    bool match(char expected);
    void skipWhitespace();
    Token number();
    Token identifier();
    Token singleCharToken(TokenType type);
    Token twoCharToken(TokenType type, char second);
    Token errorToken(const std::string& message);
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    
public:
    explicit Lexer(const std::string& source);
    
    // Main scanning method
    std::vector<Token> scanTokens();
    
    // Scan a single token
    Token scanToken();
};

} // namespace kern

#endif // KERN_LEXER_HPP
