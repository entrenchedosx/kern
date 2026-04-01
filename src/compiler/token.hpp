/* *
 * kern (Kern) - Token Definition
 * defines all token types for the lexical analyzer.
 */

#ifndef KERN_TOKEN_HPP
#define KERN_TOKEN_HPP

#include <string>
#include <variant>

namespace kern {

// token type enumeration - covers JS/Python/C++ style syntax
enum class TokenType {
    // literals
    INTEGER,
    FLOAT,
    STRING,
    TRUE,
    FALSE,
    NULL_LIT,
    IDENTIFIER,

    // keywords - variables & types
    LET,
    CONST,
    VAR,
    INT,
    FLOAT_TYPE,
    BOOL,
    STRING_TYPE,
    VOID,
    CHAR,
    LONG,
    DOUBLE,
    PTR,
    REF,

    // keywords - functions
    DEF,
    FUNCTION,
    RETURN,
    LAMBDA,
    EXPORT,
    IMPORT,

    // keywords - control flow
    IF,
    ELIF,
    ELSE,
    FOR,
    IN,
    RANGE,
    WHILE,
    DO,
    BREAK,
    CONTINUE,
    REPEAT,
    DEFER,
    WITH,
    AS,

    // keywords - error handling & pattern
    TRY,
    CATCH,
    FINALLY,
    THROW,
    RETHROW,
    ASSERT,
    MATCH,
    CASE,
    YIELD,
    ASYNC,
    AWAIT,
    SPAWN,
    ENUM,
    STRUCT,
    UNSAFE,
    EXTERN,

    // keywords - OOP
    CLASS,
    CONSTRUCTOR,
    INIT,
    PUBLIC,
    PRIVATE,
    PROTECTED,
    EXTENDS,
    THIS,
    SUPER,
    NEW,

    // operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    STAR_STAR,      // **

    EQ,             // ==
    NEQ,            // !=
    LT,
    GT,
    LE,
    GE,

    AND,            // &&
    OR,             // ||
    NOT,            // !

    BIT_AND,
    BIT_OR,
    BIT_XOR,
    SHL,            // <<
    SHR,            // >>

    ASSIGN,
    PLUS_EQ,
    MINUS_EQ,
    STAR_EQ,
    SLASH_EQ,
    PERCENT_EQ,

    // delimiters & punctuation
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    DOT,
    DOT_DOT,        // .. (range: 1..10)
    COLON,
    SEMICOLON,
    ARROW,          // =>
    PIPE,           // |> (pipeline: x |> f => f(x))
    ELLIPSIS,       // ...
    QUESTION,       // ? (ternary)
    QUESTION_DOT,   // ?. (optional chaining)
    COALESCE,       // ?? (null coalescing)
    COALESCE_EQ,    // ??= (assign if null)
    AT,             // @ (decorators)

    // comments (kept by lexer for documentation)
    COMMENT_LINE,
    COMMENT_BLOCK,

    // special
    NEWLINE,
    END_OF_FILE
};

// token structure with line/column for error reporting
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    // literal value storage
    std::variant<std::monostate, int64_t, double, std::string> value;

    Token(TokenType t, const std::string& lex, int ln, int col)
        : type(t), lexeme(lex), line(ln), column(col), value(std::monostate{}) {}
};

inline const char* tokenTypeToString(TokenType t) {
    switch (t) {
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NULL_LIT: return "NULL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::LET: return "LET";
        case TokenType::CONST: return "CONST";
        case TokenType::VAR: return "VAR";
        case TokenType::INT: return "INT";
        case TokenType::FLOAT_TYPE: return "FLOAT_TYPE";
        case TokenType::BOOL: return "BOOL";
        case TokenType::STRING_TYPE: return "STRING_TYPE";
        case TokenType::VOID: return "VOID";
        case TokenType::DEF: return "DEF";
        case TokenType::RETURN: return "RETURN";
        case TokenType::IF: return "IF";
        case TokenType::ELIF: return "ELIF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::FOR: return "FOR";
        case TokenType::IN: return "IN";
        case TokenType::RANGE: return "RANGE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::DO: return "DO";
        case TokenType::BREAK: return "BREAK";
        case TokenType::REPEAT: return "REPEAT";
        case TokenType::DEFER: return "DEFER";
        case TokenType::CLASS: return "CLASS";
        case TokenType::ENUM: return "ENUM";
        case TokenType::ASYNC: return "ASYNC";
        case TokenType::AWAIT: return "AWAIT";
        case TokenType::SPAWN: return "SPAWN";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::UNSAFE: return "UNSAFE";
        case TokenType::EXTERN: return "EXTERN";
        case TokenType::CONSTRUCTOR: return "CONSTRUCTOR";
        case TokenType::NEW: return "NEW";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::DOT_DOT: return "DOT_DOT";
        case TokenType::PIPE: return "PIPE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::AT: return "AT";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "?";
    }
}

} // namespace kern

#endif // kERN_TOKEN_HPP
