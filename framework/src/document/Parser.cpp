#include "fw/document/Parser.hpp"

#include <sstream>

namespace fw::document {

Document Parser::parse(const std::string& source) {
    Tokenizer tokenizer(source);
    tokens_ = tokenizer.tokenize();
    current_ = 0;
    errors_.clear();

    auto root = Node::createElement("root");
    while (!atEnd()) {
        auto n = parseNode();
        if (n) root->appendChild(n);
        else advance();
    }
    return Document(root);
}

bool Parser::atEnd() const noexcept { return current_ >= tokens_.size() || tokens_[current_].type == TokenType::End; }
const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::previous() const { return tokens_[current_ - 1]; }
const Token& Parser::advance() { if (!atEnd()) ++current_; return previous(); }
bool Parser::check(TokenType type) const { return !atEnd() && peek().type == type; }
bool Parser::match(TokenType type) { if (!check(type)) return false; advance(); return true; }

Node::Ptr Parser::parseNode() {
    if (check(TokenType::Text)) return parseTextNode();
    if (!match(TokenType::OpenAngle)) return nullptr;
    if (match(TokenType::Slash)) {
        synchronizeToTagEnd();
        return nullptr;
    }
    return parseElement();
}

Node::Ptr Parser::parseTextNode() {
    std::string text = advance().lexeme;
    if (text.empty()) return nullptr;
    return Node::createText(std::move(text));
}

Node::Ptr Parser::parseElement() {
    if (!match(TokenType::Identifier)) {
        errors_.push_back("Malformed element start.");
        synchronizeToTagEnd();
        return nullptr;
    }
    const std::string name = previous().lexeme;
    auto node = Node::createElement(name);

    while (!atEnd() && !check(TokenType::CloseAngle) && !check(TokenType::Slash)) {
        if (!match(TokenType::Identifier)) {
            advance();
            continue;
        }
        const std::string key = previous().lexeme;
        std::string value = "true";
        if (match(TokenType::Equals)) {
            if (match(TokenType::String) || match(TokenType::Identifier)) value = previous().lexeme;
            else errors_.push_back("Malformed attribute value for key: " + key);
        }
        node->setAttribute(key, value);
    }

    bool selfClose = false;
    if (match(TokenType::Slash)) selfClose = true;
    if (!match(TokenType::CloseAngle)) {
        errors_.push_back("Unterminated tag: " + name);
        synchronizeToTagEnd();
        return node;
    }
    if (selfClose) return node;

    while (!atEnd()) {
        if (check(TokenType::OpenAngle) && (current_ + 1) < tokens_.size() && tokens_[current_ + 1].type == TokenType::Slash) {
            advance();
            advance();
            if (match(TokenType::Identifier)) {
                const std::string closing = previous().lexeme;
                if (closing != name) errors_.push_back("Mismatched closing tag: " + closing + " expected " + name);
            }
            if (!match(TokenType::CloseAngle)) synchronizeToTagEnd();
            return node;
        }
        auto child = parseNode();
        if (child) node->appendChild(child);
        else if (!atEnd()) advance();
    }

    errors_.push_back("Missing closing tag for: " + name);
    return node;
}

void Parser::synchronizeToTagEnd() {
    while (!atEnd() && !match(TokenType::CloseAngle)) advance();
}

} // namespace fw::document

