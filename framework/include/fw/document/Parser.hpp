#pragma once

#include "fw/document/Document.hpp"
#include "fw/document/Tokenizer.hpp"

#include <string>
#include <vector>

namespace fw::document {

class Parser {
public:
    Document parse(const std::string& source);
    const std::vector<std::string>& errors() const noexcept { return errors_; }

private:
    std::vector<Token> tokens_;
    size_t current_{0};
    std::vector<std::string> errors_;

    bool atEnd() const noexcept;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);

    Node::Ptr parseNode();
    Node::Ptr parseElement();
    Node::Ptr parseTextNode();
    void synchronizeToTagEnd();
};

} // namespace fw::document

