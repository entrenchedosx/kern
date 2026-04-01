#pragma once

#include <string>
#include <vector>

namespace fw::document {

enum class TokenType {
    OpenAngle,
    CloseAngle,
    Slash,
    Equals,
    Identifier,
    String,
    Text,
    End
};

struct Token {
    TokenType type{TokenType::End};
    std::string lexeme;
    int line{1};
    int column{1};
};

class Tokenizer {
public:
    explicit Tokenizer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string source_;
    size_t pos_{0};
    int line_{1};
    int column_{1};
    bool inTag_{false};

    bool atEnd() const noexcept;
    char peek() const;
    char advance();
    void skipWhitespace();
    Token makeToken(TokenType type, std::string lexeme) const;
    Token readIdentifier();
    Token readString();
    Token readText();
};

} // namespace fw::document

