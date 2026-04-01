/* *
 * kern Lexer Implementation
 */

#include "lexer.hpp"
#include <cctype>
#include <sstream>

namespace kern {

std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"let", TokenType::LET},
    {"const", TokenType::CONST},
    {"var", TokenType::VAR},
    {"int", TokenType::INT},
    {"float", TokenType::FLOAT_TYPE},
    {"bool", TokenType::BOOL},
    {"string", TokenType::STRING_TYPE},
    {"void", TokenType::VOID},
    {"char", TokenType::CHAR},
    {"long", TokenType::LONG},
    {"double", TokenType::DOUBLE},
    {"ptr", TokenType::PTR},
    {"ref", TokenType::REF},
    {"def", TokenType::DEF},
    {"function", TokenType::FUNCTION},
    {"return", TokenType::RETURN},
    {"lambda", TokenType::LAMBDA},
    {"export", TokenType::EXPORT},
    {"import", TokenType::IMPORT},
    {"if", TokenType::IF},
    {"elif", TokenType::ELIF},
    {"else", TokenType::ELSE},
    {"for", TokenType::FOR},
    {"in", TokenType::IN},
    {"range", TokenType::RANGE},
    {"while", TokenType::WHILE},
    {"do", TokenType::DO},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"repeat", TokenType::REPEAT},
    {"defer", TokenType::DEFER},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"finally", TokenType::FINALLY},
    {"throw", TokenType::THROW},
    {"rethrow", TokenType::RETHROW},
    {"assert", TokenType::ASSERT},
    {"match", TokenType::MATCH},
    {"case", TokenType::CASE},
    {"yield", TokenType::YIELD},
    {"class", TokenType::CLASS},
    {"constructor", TokenType::CONSTRUCTOR},
    {"init", TokenType::INIT},
    {"public", TokenType::PUBLIC},
    {"private", TokenType::PRIVATE},
    {"protected", TokenType::PROTECTED},
    {"extends", TokenType::EXTENDS},
    {"this", TokenType::THIS},
    {"super", TokenType::SUPER},
    {"new", TokenType::NEW},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"null", TokenType::NULL_LIT},
    {"nil", TokenType::NULL_LIT},
    {"and", TokenType::AND},
    {"or", TokenType::OR},
};

Lexer::Lexer(const std::string& source)
    : source_(source), start_(0), current_(0), line_(1), column_(1) {}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[current_];
}

char Lexer::peekNext() const {
    if (current_ + 1 >= source_.size()) return '\0';
    return source_[current_ + 1];
}

bool Lexer::isAtEnd() const { return current_ >= source_.size(); }

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    if (source_[current_] == '\n') { line_++; column_ = 1; }
    else column_++;
    return source_[current_++];
}

bool Lexer::match(char c) {
    if (isAtEnd() || source_[current_] != c) return false;
    advance();
    return true;
}

void Lexer::addToken(TokenType type) {
    tokens_.emplace_back(type, source_.substr(start_, current_ - start_), line_, column_);
}

void Lexer::addToken(TokenType type, int64_t value) {
    Token t(type, source_.substr(start_, current_ - start_), line_, column_);
    t.value = value;
    tokens_.push_back(t);
}

void Lexer::addToken(TokenType type, double value) {
    Token t(type, source_.substr(start_, current_ - start_), line_, column_);
    t.value = value;
    tokens_.push_back(t);
}

void Lexer::addToken(TokenType type, const std::string& value) {
    Token t(type, source_.substr(start_, current_ - start_), line_, column_);
    t.value = value;
    tokens_.push_back(t);
}

void Lexer::scanToken() {
    char c = advance();
    switch (c) {
        case '(': addToken(TokenType::LPAREN); break;
        case ')': addToken(TokenType::RPAREN); break;
        case '{': addToken(TokenType::LBRACE); break;
        case '}': addToken(TokenType::RBRACE); break;
        case '[': addToken(TokenType::LBRACKET); break;
        case ']': addToken(TokenType::RBRACKET); break;
        case ',': addToken(TokenType::COMMA); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case ':': addToken(TokenType::COLON); break;
        case '-':
            if (match('=')) addToken(TokenType::MINUS_EQ);
            else if (match('>')) addToken(TokenType::ARROW);
            else addToken(TokenType::MINUS);
            break;
        case '+':
            addToken(match('=') ? TokenType::PLUS_EQ : TokenType::PLUS);
            break;
        case '*':
            if (match('*')) addToken(TokenType::STAR_STAR);
            else addToken(match('=') ? TokenType::STAR_EQ : TokenType::STAR);
            break;
        case '/':
            if (match('/')) { lineComment(); return; }
            if (match('*')) { blockComment(); return; }
            addToken(match('=') ? TokenType::SLASH_EQ : TokenType::SLASH);
            break;
        case '%': addToken(match('=') ? TokenType::PERCENT_EQ : TokenType::PERCENT); break;
        case '!': addToken(match('=') ? TokenType::NEQ : TokenType::NOT); break;
        case '=':
            if (match('=')) addToken(TokenType::EQ);
            else if (match('>')) addToken(TokenType::ARROW);
            else addToken(TokenType::ASSIGN);
            break;
        case '<':
            if (match('<')) addToken(TokenType::SHL);
            else addToken(match('=') ? TokenType::LE : TokenType::LT);
            break;
        case '>':
            if (match('>')) addToken(TokenType::SHR);
            else addToken(match('=') ? TokenType::GE : TokenType::GT);
            break;
        case '&':
            addToken(match('&') ? TokenType::AND : TokenType::BIT_AND);
            break;
        case '|':
            if (match('>')) addToken(TokenType::PIPE);
            else addToken(match('|') ? TokenType::OR : TokenType::BIT_OR);
            break;
        case '^': addToken(TokenType::BIT_XOR); break;
        case '?':
            if (peek() == '?') {
                advance();  // second '?'; '=' (if any) left for match()
                addToken(match('=') ? TokenType::COALESCE_EQ : TokenType::COALESCE);
            } else if (peek() == '.') { advance(); addToken(TokenType::QUESTION_DOT); }
            else addToken(TokenType::QUESTION);
            break;
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            addToken(TokenType::NEWLINE);
            break;
        case '"':
        case '\'':
            if (peek() == c && peekNext() == c) {
                advance(); advance();  // consume 2 more quotes
                multilineString(c);
            } else {
                stringLiteral(c);
            }
            break;
        case '.':
            if (peek() == '.') {
                advance();
                if (peek() == '.') { advance(); addToken(TokenType::ELLIPSIS); }
                else addToken(TokenType::DOT_DOT);
            } else if (std::isdigit(peek())) number();
            else addToken(TokenType::DOT);
            break;
        default:
            if (std::isdigit(c)) number();
            else if (std::isalpha(c) || c == '_') identifier();
            else {
                std::ostringstream oss;
                oss << "Unexpected character '" << c << "' at " << line_ << ":" << column_;
                throw LexerError(oss.str(), line_, column_);
            }
    }
}

void Lexer::number() {
    while (std::isdigit(peek()) || peek() == '_') advance();
    try {
        if (source_[start_] == '.') {
            if (peek() == 'e' || peek() == 'E') {
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (std::isdigit(peek())) advance();
            }
            std::string numStr = source_.substr(start_, current_ - start_);
            for (auto it = numStr.begin(); it != numStr.end(); ) { if (*it == '_') it = numStr.erase(it); else ++it; }
            if (numStr.empty()) numStr = "0";
            addToken(TokenType::FLOAT, std::stod(numStr));
            return;
        }
        if (peek() == '.' && std::isdigit(peekNext())) {
            advance();
            while (std::isdigit(peek()) || peek() == '_') advance();
            if (peek() == 'e' || peek() == 'E') {
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (std::isdigit(peek())) advance();
            }
            std::string numStr = source_.substr(start_, current_ - start_);
            for (auto it = numStr.begin(); it != numStr.end(); ) { if (*it == '_') it = numStr.erase(it); else ++it; }
            if (numStr.empty()) numStr = "0";
            addToken(TokenType::FLOAT, std::stod(numStr));
        } else {
            std::string numStr = source_.substr(start_, current_ - start_);
            for (auto it = numStr.begin(); it != numStr.end(); ) { if (*it == '_') it = numStr.erase(it); else ++it; }
            if (numStr.empty()) numStr = "0";
            addToken(TokenType::INTEGER, std::stoll(numStr));
        }
    } catch (const std::out_of_range&) {
        throw LexerError("Number literal out of range", line_, column_);
    } catch (const std::invalid_argument&) {
        throw LexerError("Invalid number literal", line_, column_);
    }
}

void Lexer::multilineString(char quote) {
    start_ = current_;
    while (!isAtEnd()) {
        if (peek() == quote && peekNext() == quote && current_ + 2 < source_.size() && source_[current_ + 2] == quote) {
            advance(); advance(); advance();  // consume closing """
            break;
        }
        if (peek() == '\n') { line_++; column_ = 1; }
        advance();
    }
    std::string value = source_.substr(start_, current_ - start_ - 3);  // before """
    std::string unescaped;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            switch (value[++i]) {
                case 'n': unescaped += '\n'; break;
                case 't': unescaped += '\t'; break;
                case 'r': unescaped += '\r'; break;
                case '\\': unescaped += '\\'; break;
                case '"': unescaped += '"'; break;
                case '\'': unescaped += '\''; break;
                default: unescaped += value[i];
            }
        } else unescaped += value[i];
    }
    addToken(TokenType::STRING, unescaped);
}

void Lexer::stringLiteral(char quote) {
    start_ = current_;  // start after quote
    while (!isAtEnd() && peek() != quote) {
        if (peek() == '\\') advance();  // escape
        if (peek() == '\n') { line_++; column_ = 1; }
        advance();
    }
    if (isAtEnd()) throw LexerError("Unterminated string", line_, column_);
    std::string value = source_.substr(start_, current_ - start_);
    advance();  // closing quote
    // simple unescape
    std::string unescaped;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            switch (value[++i]) {
                case 'n': unescaped += '\n'; break;
                case 't': unescaped += '\t'; break;
                case 'r': unescaped += '\r'; break;
                case '\\': unescaped += '\\'; break;
                case '"': unescaped += '"'; break;
                case '\'': unescaped += '\''; break;
                default: unescaped += value[i];
            }
        } else unescaped += value[i];
    }
    addToken(TokenType::STRING, unescaped);
}

void Lexer::identifier() {
    while (peek() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) advance();
    std::string text = source_.substr(start_, current_ - start_);
    auto it = keywords_.find(text);
    if (it != keywords_.end()) addToken(it->second);
    else addToken(TokenType::IDENTIFIER, text);
}

void Lexer::blockComment() {
    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') { advance(); advance(); return; }
        if (peek() == '\n') { line_++; column_ = 1; }
        advance();
    }
    throw LexerError("Unterminated block comment", line_, column_);
}

void Lexer::lineComment() {
    while (!isAtEnd() && peek() != '\n') advance();
}

std::vector<Token> Lexer::tokenize() {
    tokens_.clear();
    while (!isAtEnd()) {
        start_ = current_;
        scanToken();
    }
    tokens_.emplace_back(TokenType::END_OF_FILE, "", line_, column_);
    return tokens_;
}

} // namespace kern
