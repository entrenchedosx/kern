#include "fw/document/Tokenizer.hpp"

#include <cctype>

namespace fw::document {

Tokenizer::Tokenizer(std::string source)
    : source_(std::move(source)) {}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> out;
    out.reserve((source_.size() / 3) + 8);
    const auto oneCharLexeme = [](char c) {
        return std::string(1, c);
    };
    while (!atEnd()) {
        if (!inTag_) {
            if (peek() == '<') {
                out.push_back(makeToken(TokenType::OpenAngle, oneCharLexeme(advance())));
                inTag_ = true;
                continue;
            }
            out.push_back(readText());
            continue;
        }

        skipWhitespace();
        if (atEnd()) break;
        const char c = peek();
        if (c == '<') out.push_back(makeToken(TokenType::OpenAngle, oneCharLexeme(advance())));
        else if (c == '>') {
            out.push_back(makeToken(TokenType::CloseAngle, oneCharLexeme(advance())));
            inTag_ = false;
        } else if (c == '/') out.push_back(makeToken(TokenType::Slash, oneCharLexeme(advance())));
        else if (c == '=') out.push_back(makeToken(TokenType::Equals, oneCharLexeme(advance())));
        else if (c == '"' || c == '\'') out.push_back(readString());
        else out.push_back(readIdentifier());
    }
    out.push_back(makeToken(TokenType::End, ""));
    return out;
}

bool Tokenizer::atEnd() const noexcept { return pos_ >= source_.size(); }
char Tokenizer::peek() const { return atEnd() ? '\0' : source_[pos_]; }

char Tokenizer::advance() {
    if (atEnd()) return '\0';
    char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

void Tokenizer::skipWhitespace() {
    while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) advance();
}

Token Tokenizer::makeToken(TokenType type, std::string lexeme) const {
    return Token{type, std::move(lexeme), line_, column_};
}

Token Tokenizer::readIdentifier() {
    const int line = line_;
    const int col = column_;
    std::string s;
    s.reserve(24);
    while (!atEnd()) {
        const char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':') s.push_back(advance());
        else break;
    }
    return Token{TokenType::Identifier, std::move(s), line, col};
}

Token Tokenizer::readString() {
    const int line = line_;
    const int col = column_;
    const char quote = advance();
    std::string s;
    s.reserve(32);
    while (!atEnd() && peek() != quote) s.push_back(advance());
    if (!atEnd()) advance();
    return Token{TokenType::String, std::move(s), line, col};
}

Token Tokenizer::readText() {
    const int line = line_;
    const int col = column_;
    std::string s;
    s.reserve(64);
    while (!atEnd() && peek() != '<') s.push_back(advance());
    return Token{TokenType::Text, std::move(s), line, col};
}

} // namespace fw::document

