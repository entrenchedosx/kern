# Kern Programming Language - Complete Technical Documentation

---

## Table of Contents

1. [Introduction to Kern](#1-introduction-to-kern)
2. [Language Architecture](#2-language-architecture)
3. [Lexer and Parser](#3-lexer-and-parser)
4. [AST and Semantic Analysis](#4-ast-and-semantic-analysis)
5. [Bytecode System](#5-bytecode-system)
6. [Virtual Machine](#6-virtual-machine)
7. [Runtime and Standard Library](#7-runtime-and-standard-library)
8. [Permissions and Security Model](#8-permissions-and-security-model)
9. [Module System](#9-module-system)
10. [Build System and Toolchain](#10-build-system-and-toolchain)
11. [Editor and Tooling Ecosystem](#11-editor-and-tooling-ecosystem)
12. [ECS and Runtime Modules](#12-ecs-and-runtime-modules)
13. [Rendering / Graphics Systems](#13-rendering--graphics-systems)
14. [FFI and Native Interop](#14-ffi-and-native-interop)
15. [Memory Management](#15-memory-management)
16. [Error Handling and Diagnostics](#16-error-handling-and-diagnostics)
17. [Architecture Refactor History](#17-architecture-refactor-history)
18. [Compile-Time Architecture Firewall](#18-compile-time-architecture-firewall)
19. [Future Roadmap](#19-future-roadmap)
20. [Advanced Technical Deep Dives](#20-advanced-technical-deep-dives)

---

# 1. Introduction to Kern

## What Kern Is

Kern is a **modular, runtime-first virtual machine system** designed for systems programming with explicit architectural boundaries. It combines the elegance of Python with the power of systems programming, providing a compiled bytecode language that runs on a high-performance VM.

**Core Identity:**
- **Runtime-first architecture:** The VM and runtime core are the foundation, not an afterthought
- **Compile-time architecture enforcement:** Dependencies are validated at build time, not runtime
- **Modular isolation:** Each component (VM, runtime core, ECS, graphics) can compile and link independently
- **Systems programming focus:** Direct memory access, FFI, and unsafe execution are first-class features

## Goals and Philosophy

### Design Principles

**Architectural Truth over Convention:**
- The compiler is the ultimate authority on module boundaries
- Include graphs must be acyclic and explicitly declared
- No implicit dependencies or global state
- Every dependency direction is justified and enforced

**Minimal Viable Abstraction:**
- Zero-cost abstractions where possible
- Explicit ownership and lifetime management
- No hidden allocation or indirection
- Direct access to underlying systems when needed

**Validation-Driven Development:**
- Every architectural claim must be proven by compilation
- Build matrix tests enforce isolation guarantees
- Runtime tests follow successful compilation validation
- No "theoretically correct" without "compiler-proven"

### Why Kern Exists

**Problem Statement:**
Existing runtime systems suffer from implicit dependencies, circular includes, and architectural drift. Modular systems become monolithic through gradual boundary erosion. Testing isolation is impossible without full system compilation.

**Solution Approach:**
Kern enforces architectural boundaries at compile time using:
- Explicit dependency declarations in CMake
- Include firewall validation via Python scripts
- Build matrix isolation testing
- Legacy quarantine for unavoidable violations

## Comparison Against Other Languages

| Feature | Kern | C++ | Rust | Zig | Python | Lua |
|---------|------|-----|-----|-----|--------|-----|
| **Compilation Model** | Bytecode | Native | Native | Native | Interpreted | Bytecode |
| **Memory Management** | Explicit + Optional GC | Manual | Ownership | Manual | GC | GC |
| **FFI Support** | First-class | Native | Unsafe | Native | CTypes | CTypes |
| **Module System** | Compile-time enforced | Headers | Crates | Files | Packages | require() |
| **Performance** | High (VM optimized) | Highest | High | High | Low | Medium |
| **Safety** | Trust-the-programmer | Manual | Safe by default | Manual | Runtime | Runtime |
| **Architecture** | Runtime-first | System-first | System-first | System-first | Language-first | Language-first |

## Intended Use Cases

**Primary Use Cases:**
1. **Game engines:** Modular runtime with pluggable systems (ECS, graphics, audio)
2. **Embedded systems:** Verifiable module boundaries for safety-critical code
3. **Plugin architectures:** Host applications with dynamically loadable modules
4. **Compiler research:** Testbed for language runtime experiments
5. **Systems programming:** Runtime for custom languages with FFI requirements

**Secondary Use Cases:**
- System automation scripts
- High-performance data processing
- Cross-platform application development
- Educational language implementation
- Rapid prototyping with production deployment

## Language Vision

Kern aims to be the **systems programming language for the modular era** - where architectural clarity, performance transparency, and developer trust are paramount. It bridges the gap between rapid development and systems-level control, providing a foundation for building verifiable, maintainable software systems.

---

# 2. Language Architecture

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Kern Language System                     │
├─────────────────────────────────────────────────────────────┤
│  Source Code (.kn) → Parser → AST → Semantic Analysis → IR  │
│                                                              │
│  IR → Bytecode Generator → Optimized Bytecode (.kbc)        │
│                                                              │
│  Bytecode → Kern VM (Register-Window Stack Machine)         │
│                                                              │
│  VM Runtime → Module System → Native Libraries → System APIs │
└─────────────────────────────────────────────────────────────┘
```

### Frontend/Backend Separation

**Frontend Responsibilities:**
- Lexical analysis and tokenization
- Syntax parsing and AST construction
- Semantic validation and type checking
- Symbol resolution and scope management
- Error reporting and recovery

**Backend Responsibilities:**
- Intermediate representation (IR) generation
- Bytecode optimization and emission
- Constant pool management
- Debug information generation
- Binary format serialization

### Compiler Pipeline Overview

```
Source File (.kn)
       ↓
   Lexer (tokenizer)
       ↓
   Parser (AST builder)
       ↓
Semantic Analyzer (type checking)
       ↓
   IR Generator (intermediate)
       ↓
Bytecode Generator (optimization)
       ↓
   Bytecode File (.kbc)
       ↓
   Kern VM (execution)
```

### Runtime Model

**VM Execution Model:**
- **Register-window stack machine** (not pure stack)
- **Direct-threaded dispatch** (computed goto)
- **Arena allocation** for frames
- **Bounds-checked but not bounds-prevented** operations
- **Explicit ownership** with optional garbage collection

**Module Loading:**
- **Compile-time dependency resolution**
- **Runtime module registry**
- **Dynamic linking** with version compatibility
- **Sandbox isolation** when enabled

### Execution Lifecycle

1. **Compilation Phase:**
   - Source → Tokens → AST → IR → Bytecode
   - Constant pool generation
   - Symbol table construction
   - Debug metadata attachment

2. **Loading Phase:**
   - Bytecode verification
   - Module dependency resolution
   - Runtime linking
   - Memory allocation preparation

3. **Execution Phase:**
   - VM initialization
   - Bytecode dispatch
   - Runtime library integration
   - Native FFI calls

4. **Cleanup Phase:**
   - Resource deallocation
   - Module unloading
   - Memory cleanup
   - Statistics collection

### Source → AST → IR → Bytecode → VM Flow

**Detailed Flow Diagram:**

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Source     │ →  │    AST      │ →  │     IR      │ →  │  Bytecode   │ →  │  Kern VM    │
│  (.kn)      │    │             │    │             │    │   (.kbc)    │    │             │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │                   │                   │
       │ Tokens           │ Typed Nodes      │ SSA Form        │ Opcodes          │ Registers
       │                   │                   │                   │                   │
       ▼                   ▼                   ▼                   ▼                   ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ Lexer       │    │ Parser      │    │ IR Builder  │    │ Optimizer   │    │ Dispatcher  │
│ - Tokens    │    │ - Grammar   │    │ - SSA       │    │ - Peephole  │    │ - Direct    │
│ - Keywords  │    │ - AST      │    │ - Types     │    │ - Constant  │    │ - Goto      │
│ - Literals  │    │ - Symbols  │    │ - Phi       │    │ - Folding   │    │ - Register  │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

### Static vs Runtime Responsibilities

**Static (Compile-time):**
- Syntax validation
- Type checking
- Semantic analysis
- Dependency resolution
- Bytecode generation
- Optimization passes

**Runtime (Execution-time):**
- Bytecode verification
- Module loading
- Memory management
- Native library binding
- Error handling
- Performance monitoring

---

# 3. Lexer and Parser

## Tokenization System

### Token Architecture

The Kern lexer implements a **character-by-character scanning system** with position tracking and error recovery. The token system supports the complete language grammar including literals, identifiers, operators, and control structures.

**Core Token Types:**
```cpp
enum class TokenType {
    // Literals
    NUMBER, IDENTIFIER, STRING,
    
    // Operators
    PLUS, MINUS, MULTIPLY, DIVIDE, MOD, POW,
    EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL,
    AND, OR, NOT, BIT_AND, BIT_OR, BIT_XOR, SHL, SHR,
    
    // Delimiters
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COMMA, DOT, COLON, ARROW,
    
    // Keywords
    IF, ELSE, WHILE, FOR, FUNCTION, RETURN, BREAK, CONTINUE,
    LET, CONST, IMPORT, EXPORT, CLASS, STRUCT, ENUM,
    
    // Special
    EOF_TOKEN, ILLEGAL, NEWLINE, INDENT, DEDENT
};
```

**Token Structure:**
```cpp
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    size_t position;  // Absolute position in source
    
    // Constructor and utility methods
    Token(TokenType t, const std::string& lex, int l, int c, size_t pos);
    bool is(TokenType t) const { return type == t; }
    bool isLiteral() const;
    bool isOperator() const;
    bool isKeyword() const;
};
```

### Lexer Implementation Details

**Scanning Algorithm:**
```cpp
class Lexer {
private:
    std::string source;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;
    
    // Core scanning methods
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    bool isAtEnd() const;
    
    // Token type handlers
    Token scanToken();
    Token identifier();
    Token number();
    Token string();
    Token character();
    
    // Utility methods
    void skipWhitespace();
    void skipComment();
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> scanTokens();
};
```

**Position Tracking System:**
- **Line-based tracking** for error reporting
- **Column tracking** for precise error locations
- **Absolute position** for source mapping
- **Unicode-aware** character counting

**Error Recovery:**
- **Panic mode recovery** on syntax errors
- **Synchronization tokens** (semicolon, newlines)
- **Error token injection** for parser consumption
- **Multiple error reporting** per compilation unit

## Grammar Design

### Language Grammar (EBNF)

```ebnf
program         ::= { declaration }

declaration     ::= function_decl
                 |  variable_decl
                 |  class_decl
                 |  struct_decl
                 |  enum_decl
                 |  import_decl
                 |  export_decl
                 |  statement

function_decl   ::= "function" identifier "(" [parameter_list] ")" [return_type] block
variable_decl   ::= ("let" | "const") identifier [":" type] "=" expression
class_decl      ::= "class" identifier [":" identifier] class_body
struct_decl     ::= "struct" identifier struct_body
enum_decl       ::= "enum" identifier "{" { identifier "," } "}"
import_decl     ::= "import" string_literal [ "as" identifier ]
export_decl     ::= "export" declaration

statement       ::= expression_stmt
                 |  if_stmt
                 |  while_stmt
                 |  for_stmt
                 |  return_stmt
                 |  break_stmt
                 |  continue_stmt
                 |  block

if_stmt         ::= "if" expression block ["else" (if_stmt | block)]
while_stmt      ::= "while" expression block
for_stmt        ::= "for" "(" [variable_decl] ";" expression ";" expression ")" block
return_stmt     ::= "return" [expression]
break_stmt      ::= "break"
continue_stmt   ::= "continue"

expression      ::= assignment
assignment      ::= logical_or [ "=" assignment ]
logical_or      ::= logical_and { "||" logical_and }
logical_and     ::= equality { "&&" equality }
equality        ::= comparison { ("==" | "!=") comparison }
comparison      ::= term { ("<" | ">" | "<=" | ">=") term }
term            ::= factor { ("+" | "-") factor }
factor          ::= unary { ("*" | "/" | "%") unary }
unary           ::= ("!" | "-" | "~") [unary] | call
call            ::= primary { "(" [argument_list] ")" | "." identifier | "[" expression "]" }

primary         ::= literal
                 |  identifier
                 |  "(" expression ")"
                 |  "new" identifier [ "(" argument_list ")" ]
                 |  "super" "." identifier
                 |  "this"

literal         ::= number | string | character | "true" | "false" | "null"
```

### Parsing Architecture

**Parser Design:**
- **Recursive descent parser** for clarity and maintainability
- **Pratt parsing** for expression precedence handling
- **Error recovery** with panic mode synchronization
- **AST construction** with semantic annotations

**Parser Implementation:**
```cpp
class Parser {
private:
    std::vector<Token> tokens;
    size_t current = 0;
    std::vector<std::string> errors;
    
    // Core parsing methods
    bool isAtEnd() const;
    Token peek() const;
    Token previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    
    // Grammar methods
    std::unique_ptr<Program> parseProgram();
    std::unique_ptr<Decl> parseDeclaration();
    std::unique_ptr<FunctionDecl> parseFunction();
    std::unique_ptr<VarDecl> parseVariable();
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Expr> parseExpression();
    
    // Expression parsing (Pratt)
    std::unique_ptr<Expr> parseAssignment();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseTerm();
    std::unique_ptr<Expr> parseFactor();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parseCall();
    std::unique_ptr<Expr> parsePrimary();
    
    // Error handling
    void error(Token token, const std::string& message);
    void synchronize();
    
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<Program> parse();
};
```

### Pratt Parser Implementation

**Precedence Table:**
```cpp
enum class Precedence {
    NONE,
    ASSIGNMENT,    // =
    OR,           // ||
    AND,          // &&
    EQUALITY,      // == !=
    COMPARISON,    // < > <= >=
    TERM,         // + -
    FACTOR,       // * / %
    UNARY,        // ! - ~
    CALL,         // . () []
    PRIMARY
};
```

**Parse Function Types:**
```cpp
using ParseFn = std::function<std::unique_ptr<Expr>()>;

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};
```

**Expression Dispatch:**
```cpp
std::unique_ptr<Expr> Parser::parseExpression() {
    return parsePrecedence(Precedence::ASSIGNMENT);
}

std::unique_ptr<Expr> Parser::parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(previous().type).prefix;
    if (!prefixRule) {
        error(previous(), "Expect expression.");
        return nullptr;
    }
    
    std::unique_ptr<Expr> expr = prefixRule();
    
    while (precedence <= getRule(peek().type).precedence) {
        advance();
        ParseFn infixRule = getRule(previous().type).infix;
        expr = infixRule(std::move(expr));
    }
    
    return expr;
}
```

## AST Generation

### AST Node Hierarchy

**Base AST Node:**
```cpp
class ASTNode {
public:
    enum class Kind {
        // Declarations
        FUNCTION_DECL, VAR_DECL, CLASS_DECL, STRUCT_DECL, ENUM_DECL,
        IMPORT_DECL, EXPORT_DECL,
        
        // Statements
        EXPR_STMT, IF_STMT, WHILE_STMT, FOR_STMT, RETURN_STMT,
        BREAK_STMT, CONTINUE_STMT, BLOCK_STMT,
        
        // Expressions
        LITERAL_EXPR, IDENTIFIER_EXPR, BINARY_EXPR, UNARY_EXPR,
        ASSIGN_EXPR, CALL_EXPR, GET_EXPR, SET_EXPR,
        
        // Types
        BUILTIN_TYPE, USER_TYPE, FUNCTION_TYPE, ARRAY_TYPE
    };
    
    Kind kind;
    Token location;
    
    ASTNode(Kind k, Token loc) : kind(k), location(loc) {}
    virtual ~ASTNode() = default;
    
    virtual void accept(ASTVisitor& visitor) = 0;
};
```

**Declaration Nodes:**
```cpp
class FunctionDecl : public Decl {
public:
    std::string name;
    std::vector<std::unique_ptr<Param>> params;
    std::unique_ptr<Type> returnType;
    std::unique_ptr<BlockStmt> body;
    bool isGenerator = false;
    bool isAsync = false;
    
    FunctionDecl(Token name, std::vector<std::unique_ptr<Param>> params,
                 std::unique_ptr<Type> retType, std::unique_ptr<BlockStmt> body)
        : Decl(Kind::FUNCTION_DECL, name), name(name.lexeme),
          params(std::move(params)), returnType(std::move(retType)),
          body(std::move(body)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class VarDecl : public Decl {
public:
    std::string name;
    std::unique_ptr<Type> type;
    std::unique_ptr<Expr> initializer;
    bool isConst = false;
    
    VarDecl(Token name, std::unique_ptr<Type> type,
            std::unique_ptr<Expr> initializer, bool isConst)
        : Decl(Kind::VAR_DECL, name), name(name.lexeme),
          type(std::move(type)), initializer(std::move(initializer)),
          isConst(isConst) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};
```

**Expression Nodes:**
```cpp
class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    Token op;
    std::unique_ptr<Expr> right;
    
    BinaryExpr(std::unique_ptr<Expr> left, Token op, std::unique_ptr<Expr> right)
        : Expr(Kind::BINARY_EXPR, op), left(std::move(left)),
          op(op), right(std::move(right)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    Token paren;
    std::vector<std::unique_ptr<Expr>> arguments;
    
    CallExpr(std::unique_ptr<Expr> callee, Token paren,
             std::vector<std::unique_ptr<Expr>> args)
        : Expr(Kind::CALL_EXPR, paren), callee(std::move(callee)),
          paren(paren), arguments(std::move(args)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};
```

### Error Recovery System

**Panic Mode Recovery:**
```cpp
void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        
        switch (peek().type) {
            case TokenType::CLASS:
            case TokenType::FUNCTION:
            case TokenType::VAR:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::RETURN:
                return;
            default:
                // Continue
                break;
        }
        
        advance();
    }
}
```

**Error Reporting:**
```cpp
void Parser::error(Token token, const std::string& message) {
    if (token.type == TokenType::EOF_TOKEN) {
        report(token.line, token.column, "at end", message);
    } else {
        report(token.line, token.column, "at '" + token.lexeme + "'", message);
    }
}

void Parser::report(int line, int column, const std::string& where, 
                   const std::string& message) {
    std::string error = "[line " + std::to_string(line) + 
                        ", col " + std::to_string(column) + "] " +
                        "Error " + where + ": " + message;
    errors.push_back(error);
}
```

## Named Argument Support

**Named Argument Syntax:**
```kern
function create_window(title: string, width: int, height: int, 
                      fullscreen: bool = false) {
    // Implementation
}

// Named arguments in any order
create_window(height: 720, width: 1280, title: "My Window")
create_window(title: "Fullscreen Window", width: 1920, 
              height: 1080, fullscreen: true)
```

**Parser Implementation:**
```cpp
std::vector<std::unique_ptr<Expr>> Parser::parseArguments() {
    std::vector<std::unique_ptr<Expr>> arguments;
    
    if (!check(TokenType::RPAREN)) {
        do {
            if (arguments.size() >= 255) {
                error(peek(), "Can't have more than 255 arguments.");
            }
            
            // Check for named argument
            if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::COLON) {
                Token name = advance();  // identifier
                advance();              // colon
                std::unique_ptr<Expr> value = parseExpression();
                arguments.push_back(std::make_unique<NamedArgExpr>(name, std::move(value)));
            } else {
                arguments.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::RPAREN, "Expect ')' after arguments.");
    return arguments;
}
```

## Syntax Rules

### Complete Language Syntax

**Program Structure:**
```kern
// Module declaration
module my_module;

// Imports
import std.io;
import std.math as math;

// Exports
export function main() {
    print("Hello, Kern!");
}
```

**Function Definitions:**
```kern
// Basic function
function add(a: int, b: int) -> int {
    return a + b;
}

// Generator function
function fibonacci(n: int) -> generator<int> {
    let a = 0, b = 1;
    for i in 0..n {
        yield a;
        let temp = a + b;
        a = b;
        b = temp;
    }
}

// Async function
async function fetch_data(url: string) -> string {
    let response = await http_get(url);
    return response.body;
}
```

**Variable Declarations:**
```kern
// Type inference
let x = 42;
let name = "Kern";

// Explicit types
let count: int = 100;
const PI: float = 3.14159;

// Arrays and objects
let numbers: [int] = [1, 2, 3, 4, 5];
let person: {name: string, age: int} = {name: "Alice", age: 30};
```

**Control Structures:**
```kern
// If-else
if (x > 0) {
    print("Positive");
} else if (x < 0) {
    print("Negative");
} else {
    print("Zero");
}

// While loop
while (condition) {
    // Loop body
}

// For loop
for i in 0..10 {
    print(i);
}

// For-each loop
for item in collection {
    process(item);
}
```

## Scope Handling

### Scope Management System

**Scope Stack:**
```cpp
class ScopeManager {
private:
    std::vector<std::unique_ptr<Scope>> scopes;
    std::unordered_map<std::string, std::vector<Decl*>> symbolTable;
    
public:
    void enterScope(ScopeType type);
    void exitScope();
    void declare(Decl* declaration);
    Decl* resolve(const std::string& name);
    bool isDeclared(const std::string& name) const;
    bool isDefined(const std::string& name) const;
};
```

**Scope Types:**
```cpp
enum class ScopeType {
    GLOBAL,
    FUNCTION,
    BLOCK,
    CLASS,
    LOOP
};

class Scope {
public:
    ScopeType type;
    std::unordered_map<std::string, Decl*> symbols;
    std::unique_ptr<Scope> parent;
    
    Scope(ScopeType type, std::unique_ptr<Scope> parent = nullptr)
        : type(type), parent(std::move(parent)) {}
    
    Decl* lookup(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        }
        return parent ? parent->lookup(name) : nullptr;
    }
};
```

### Symbol Resolution

**Resolution Algorithm:**
```cpp
Decl* ScopeManager::resolve(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        Decl* declaration = (*it)->lookup(name);
        if (declaration) {
            return declaration;
        }
    }
    return nullptr;
}
```

**Variable Shadowing:**
```cpp
function test_shadowing() {
    let x = "global";           // Global scope
    
    {
        let x = "outer";        // Block scope 1
        {
            let x = "inner";    // Block scope 2
            print(x);            // Prints "inner"
        }
        print(x);                // Prints "outer"
    }
    print(x);                    // Prints "global"
}
```

---

# 4. AST and Semantic Analysis

## AST Node Hierarchy

### Complete AST Architecture

The Kern AST implements a **hierarchical node system** with visitor pattern support for semantic analysis and code generation. Each node type carries semantic information and location data for error reporting.

**Core AST Base Classes:**
```cpp
class ASTNode {
public:
    enum class Kind {
        // Declarations
        MODULE_DECL, IMPORT_DECL, EXPORT_DECL,
        FUNCTION_DECL, VAR_DECL, CONST_DECL,
        CLASS_DECL, STRUCT_DECL, ENUM_DECL,
        INTERFACE_DECL, TYPE_ALIAS_DECL,
        
        // Statements
        EXPR_STMT, BLOCK_STMT, IF_STMT, WHILE_STMT,
        FOR_STMT, FOR_IN_STMT, RETURN_STMT, BREAK_STMT,
        CONTINUE_STMT, MATCH_STMT, TRY_STMT, THROW_STMT,
        
        // Expressions
        LITERAL_EXPR, IDENTIFIER_EXPR, BINARY_EXPR, UNARY_EXPR,
        ASSIGN_EXPR, CALL_EXPR, MEMBER_EXPR, INDEX_EXPR,
        TERNARY_EXPR, LAMBDA_EXPR, ARRAY_EXPR, OBJECT_EXPR,
        COMP_EXPR, SPREAD_EXPR, NEW_EXPR, SUPER_EXPR, THIS_EXPR,
        
        // Types
        BUILTIN_TYPE, USER_TYPE, FUNCTION_TYPE, ARRAY_TYPE,
        TUPLE_TYPE, UNION_TYPE, INTERSECTION_TYPE, GENERIC_TYPE
    };
    
    Kind kind;
    SourceLocation location;
    std::unique_ptr<Annotation> annotation;
    
    ASTNode(Kind k, SourceLocation loc) : kind(k), location(loc) {}
    virtual ~ASTNode() = default;
    
    virtual void accept(ASTVisitor& visitor) = 0;
    virtual std::unique_ptr<ASTNode> clone() const = 0;
};
```

**Declaration Hierarchy:**
```cpp
class Decl : public ASTNode {
public:
    std::string name;
    Visibility visibility = Visibility::PUBLIC;
    std::vector<std::unique_ptr<Attribute>> attributes;
    
    Decl(Kind kind, SourceLocation loc, const std::string& name)
        : ASTNode(kind, loc), name(name) {}
};

class FunctionDecl : public Decl {
public:
    std::vector<std::unique_ptr<Param>> parameters;
    std::unique_ptr<TypeNode> returnType;
    std::unique_ptr<BlockStmt> body;
    FunctionKind kind = FunctionKind::NORMAL;
    std::vector<std::unique_ptr<TypeParameter>> typeParameters;
    
    FunctionDecl(SourceLocation loc, const std::string& name)
        : Decl(Kind::FUNCTION_DECL, loc, name) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class ClassDecl : public Decl {
public:
    std::unique_ptr<TypeNode> superclass;
    std::vector<std::unique_ptr<InterfaceDecl>> interfaces;
    std::vector<std::unique_ptr<FieldDecl>> fields;
    std::vector<std::unique_ptr<MethodDecl>> methods;
    std::vector<std::unique_ptr<ConstructorDecl>> constructors;
    
    ClassDecl(SourceLocation loc, const std::string& name)
        : Decl(Kind::CLASS_DECL, loc, name) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};
```

**Expression Hierarchy:**
```cpp
class Expr : public ASTNode {
public:
    std::unique_ptr<TypeNode> type;
    
    Expr(Kind kind, SourceLocation loc) : ASTNode(kind, loc) {}
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    BinaryOperator op;
    std::unique_ptr<Expr> right;
    
    BinaryExpr(SourceLocation loc, std::unique_ptr<Expr> left,
               BinaryOperator op, std::unique_ptr<Expr> right)
        : Expr(Kind::BINARY_EXPR, loc), left(std::move(left)),
          op(op), right(std::move(right)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> arguments;
    std::vector<std::string> argumentNames;  // For named arguments
    
    CallExpr(SourceLocation loc, std::unique_ptr<Expr> callee,
             std::vector<std::unique_ptr<Expr>> args)
        : Expr(Kind::CALL_EXPR, loc), callee(std::move(callee)),
          arguments(std::move(args)) {}
    
    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
};
```

### Type System Integration

**Type Node Hierarchy:**
```cpp
class TypeNode : public ASTNode {
public:
    TypeNode(Kind kind, SourceLocation loc) : ASTNode(kind, loc) {}
    virtual std::unique_ptr<Type> resolve(SemanticAnalyzer& analyzer) = 0;
};

class BuiltinTypeNode : public TypeNode {
public:
    BuiltinType builtinType;
    
    BuiltinTypeNode(SourceLocation loc, BuiltinType type)
        : TypeNode(Kind::BUILTIN_TYPE, loc), builtinType(type) {}
    
    std::unique_ptr<Type> resolve(SemanticAnalyzer& analyzer) override;
};

class FunctionTypeNode : public TypeNode {
public:
    std::vector<std::unique_ptr<TypeNode>> parameters;
    std::unique_ptr<TypeNode> returnType;
    bool isVariadic = false;
    
    FunctionTypeNode(SourceLocation loc,
                     std::vector<std::unique_ptr<TypeNode>> params,
                     std::unique_ptr<TypeNode> retType)
        : TypeNode(Kind::FUNCTION_TYPE, loc), parameters(std::move(params)),
          returnType(std::move(retType)) {}
    
    std::unique_ptr<Type> resolve(SemanticAnalyzer& analyzer) override;
};
```

## Semantic Validation

### Type System Architecture

**Type Hierarchy:**
```cpp
class Type {
public:
    enum class Kind {
        VOID, BOOL, INT, FLOAT, STRING, ARRAY, OBJECT,
        FUNCTION, CLASS, INTERFACE, GENERIC, TYPE_PARAMETER,
        UNION, INTERSECTION, TUPLE, NEVER, UNKNOWN
    };
    
    Kind kind;
    bool nullable = false;
    std::unique_ptr<TypeAnnotation> annotation;
    
    Type(Kind k) : kind(k) {}
    virtual ~Type() = default;
    
    virtual bool equals(const Type& other) const = 0;
    virtual bool isSubtypeOf(const Type& other) const = 0;
    virtual std::unique_ptr<Type> clone() const = 0;
    virtual std::string toString() const = 0;
};
```

**Primitive Types:**
```cpp
class BuiltinType : public Type {
public:
    BuiltinKind builtinKind;
    int bitWidth = 0;  // For numeric types
    
    BuiltinType(BuiltinKind kind, int width = 0)
        : Type(Kind::INT), builtinKind(kind), bitWidth(width) {}
    
    bool equals(const Type& other) const override {
        if (other.kind != Kind::INT) return false;
        const auto& otherInt = static_cast<const BuiltinType&>(other);
        return builtinKind == otherInt.builtinKind && bitWidth == otherInt.bitWidth;
    }
    
    bool isSubtypeOf(const Type& other) const override {
        return equals(other);
    }
    
    std::string toString() const override {
        std::string result;
        switch (builtinKind) {
            case BuiltinKind::INT: result = "int"; break;
            case BuiltinKind::FLOAT: result = "float"; break;
            case BuiltinKind::BOOL: result = "bool"; break;
            case BuiltinKind::STRING: result = "string"; break;
            case BuiltinKind::VOID: result = "void"; break;
        }
        if (bitWidth > 0) result += std::to_string(bitWidth);
        if (nullable) result += "?";
        return result;
    }
};
```

**Function Types:**
```cpp
class FunctionType : public Type {
public:
    std::vector<std::unique_ptr<Type>> parameters;
    std::unique_ptr<Type> returnType;
    bool isVariadic = false;
    std::vector<std::unique_ptr<Type>> typeParameters;
    
    FunctionType(std::vector<std::unique_ptr<Type>> params,
                  std::unique_ptr<Type> retType, bool variadic = false)
        : Type(Kind::FUNCTION), parameters(std::move(params)),
          returnType(std::move(retType)), isVariadic(variadic) {}
    
    bool equals(const Type& other) const override {
        if (other.kind != Kind::FUNCTION) return false;
        const auto& otherFunc = static_cast<const FunctionType&>(other);
        
        if (parameters.size() != otherFunc.parameters.size()) return false;
        if (isVariadic != otherFunc.isVariadic) return false;
        
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (!parameters[i]->equals(*otherFunc.parameters[i])) return false;
        }
        
        return returnType->equals(*otherFunc.returnType);
    }
    
    bool isSubtypeOf(const Type& other) const override {
        return equals(other);  // Function types are invariant
    }
    
    std::string toString() const override {
        std::string result = "(";
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) result += ", ";
            result += parameters[i]->toString();
        }
        if (isVariadic) result += "...";
        result += ") -> " + returnType->toString();
        if (nullable) result += "?";
        return result;
    }
};
```

### Type Checking Algorithm

**Semantic Analyzer Implementation:**
```cpp
class SemanticAnalyzer : public ASTVisitor {
private:
    std::vector<std::unique_ptr<Scope>> scopes;
    std::unique_ptr<TypeTable> typeTable;
    std::vector<SemanticError> errors;
    std::unique_ptr<SymbolTable> symbolTable;
    
    // Core analysis methods
    void enterScope(ScopeType type);
    void exitScope();
    void declareSymbol(const std::string& name, Decl* declaration);
    Symbol* resolveSymbol(const std::string& name);
    
    // Type checking methods
    std::unique_ptr<Type> checkType(std::unique_ptr<TypeNode> typeNode);
    std::unique_ptr<Type> checkExpr(std::unique_ptr<Expr>& expr);
    std::unique_ptr<Type> checkStmt(std::unique_ptr<Stmt>& stmt);
    std::unique_ptr<Type> checkDecl(std::unique_ptr<Decl>& decl);
    
    // Type operations
    bool isAssignable(std::unique_ptr<Type>& target, std::unique_ptr<Type>& source);
    std::unique_ptr<Type> inferType(std::unique_ptr<Expr>& expr);
    std::unique_ptr<Type> unifyTypes(std::unique_ptr<Type>& left, 
                                    std::unique_ptr<Type>& right);
    
public:
    explicit SemanticAnalyzer(TypeTable* typeTable) : typeTable(typeTable) {}
    
    std::unique_ptr<Program> analyze(std::unique_ptr<Program> program);
    std::vector<SemanticError> getErrors() const { return errors; }
    
    // Visitor methods
    void visit(FunctionDecl& decl) override;
    void visit(VarDecl& decl) override;
    void visit(ClassDecl& decl) override;
    void visit(BinaryExpr& expr) override;
    void visit(CallExpr& expr) override;
    void visit(AssignExpr& expr) override;
    // ... other visitor methods
};
```

**Expression Type Checking:**
```cpp
std::unique_ptr<Type> SemanticAnalyzer::checkExpr(std::unique_ptr<Expr>& expr) {
    switch (expr->kind) {
        case ASTNode::Kind::LITERAL_EXPR:
            return checkLiteralExpr(static_cast<LiteralExpr&>(*expr));
            
        case ASTNode::Kind::IDENTIFIER_EXPR:
            return checkIdentifierExpr(static_cast<IdentifierExpr&>(*expr));
            
        case ASTNode::Kind::BINARY_EXPR:
            return checkBinaryExpr(static_cast<BinaryExpr&>(*expr));
            
        case ASTNode::Kind::CALL_EXPR:
            return checkCallExpr(static_cast<CallExpr&>(*expr));
            
        case ASTNode::Kind::ASSIGN_EXPR:
            return checkAssignExpr(static_cast<AssignExpr&>(*expr));
            
        default:
            errors.push_back(SemanticError(expr->location, 
                                         "Unsupported expression type"));
            return std::make_unique<UnknownType>();
    }
}

std::unique_ptr<Type> SemanticAnalyzer::checkBinaryExpr(BinaryExpr& expr) {
    auto leftType = checkExpr(expr.left);
    auto rightType = checkExpr(expr.right);
    
    // Operator-specific type checking
    switch (expr.op) {
        case BinaryOperator::ADD:
        case BinaryOperator::SUB:
        case BinaryOperator::MUL:
        case BinaryOperator::DIV:
            // Arithmetic operators require numeric types
            if (!isNumericType(*leftType) || !isNumericType(*rightType)) {
                errors.push_back(SemanticError(expr.location,
                    "Arithmetic operators require numeric operands"));
                return std::make_unique<ErrorType>();
            }
            return unifyTypes(leftType, rightType);
            
        case BinaryOperator::EQUAL:
        case BinaryOperator::NOT_EQUAL:
            // Equality operators work with any comparable types
            if (!leftType->equals(*rightType)) {
                errors.push_back(SemanticError(expr.location,
                    "Equality operators require compatible operand types"));
                return std::make_unique<ErrorType>();
            }
            return std::make_unique<BuiltinType>(BuiltinKind::BOOL);
            
        case BinaryOperator::AND:
        case BinaryOperator::OR:
            // Logical operators require boolean operands
            if (!isBooleanType(*leftType) || !isBooleanType(*rightType)) {
                errors.push_back(SemanticError(expr.location,
                    "Logical operators require boolean operands"));
                return std::make_unique<ErrorType>();
            }
            return std::make_unique<BuiltinType>(BuiltinKind::BOOL);
            
        default:
            errors.push_back(SemanticError(expr.location,
                "Unsupported binary operator"));
            return std::make_unique<ErrorType>();
    }
}
```

**Function Type Checking:**
```cpp
std::unique_ptr<Type> SemanticAnalyzer::checkCallExpr(CallExpr& expr) {
    auto calleeType = checkExpr(expr.callee);
    
    if (calleeType->kind != Type::Kind::FUNCTION) {
        errors.push_back(SemanticError(expr.location,
            "Cannot call non-function value"));
        return std::make_unique<ErrorType>();
    }
    
    auto functionType = static_cast<FunctionType*>(calleeType.get());
    
    // Check argument count
    if (expr.arguments.size() != functionType->parameters.size() && 
        !functionType->isVariadic) {
        errors.push_back(SemanticError(expr.location,
            "Argument count mismatch: expected " + 
            std::to_string(functionType->parameters.size()) + 
            ", got " + std::to_string(expr.arguments.size())));
        return std::make_unique<ErrorType>();
    }
    
    // Check argument types
    for (size_t i = 0; i < expr.arguments.size(); ++i) {
        auto argType = checkExpr(expr.arguments[i]);
        
        if (i < functionType->parameters.size()) {
            if (!isAssignable(functionType->parameters[i], argType)) {
                errors.push_back(SemanticError(expr.arguments[i]->location,
                    "Argument type mismatch for parameter " + std::to_string(i)));
            }
        }
    }
    
    return functionType->returnType->clone();
}
```

## Symbol Tables

### Symbol Management System

**Symbol Structure:**
```cpp
class Symbol {
public:
    enum class Kind {
        VARIABLE, FUNCTION, CLASS, INTERFACE, TYPE_ALIAS,
        MODULE, ENUM_MEMBER, FIELD, METHOD, PARAMETER
    };
    
    Kind kind;
    std::string name;
    std::unique_ptr<Type> type;
    Decl* declaration = nullptr;
    Visibility visibility = Visibility::PUBLIC;
    bool isMutable = false;
    bool isStatic = false;
    int scopeLevel = 0;
    
    Symbol(Kind k, const std::string& name, std::unique_ptr<Type> type)
        : kind(k), name(name), type(std::move(type)) {}
};

class SymbolTable {
private:
    std::unordered_map<std::string, std::vector<Symbol*>> symbols;
    std::vector<std::unique_ptr<Symbol>> ownedSymbols;
    
public:
    void addSymbol(std::unique_ptr<Symbol> symbol);
    Symbol* resolve(const std::string& name);
    std::vector<Symbol*> resolveAll(const std::string& name);
    bool isDeclared(const std::string& name) const;
    void clear();
};
```

**Scope-Based Symbol Resolution:**
```cpp
class Scope {
private:
    std::unordered_map<std::string, Symbol*> symbols;
    std::unique_ptr<Scope> parent;
    ScopeType type;
    int level;
    
public:
    Scope(ScopeType type, std::unique_ptr<Scope> parent, int level)
        : type(type), parent(std::move(parent)), level(level) {}
    
    void addSymbol(Symbol* symbol) {
        symbols[symbol->name] = symbol;
    }
    
    Symbol* lookup(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        }
        return parent ? parent->lookup(name) : nullptr;
    }
    
    Symbol* lookupLocal(const std::string& name) {
        auto it = symbols.find(name);
        return it != symbols.end() ? it->second : nullptr;
    }
};
```

## Function Resolution

### Function Overloading Resolution

**Overload Resolution Algorithm:**
```cpp
class FunctionResolver {
private:
    std::vector<FunctionSymbol*> candidates;
    std::vector<std::unique_ptr<Type>> argumentTypes;
    
public:
    FunctionSymbol* resolve(const std::vector<std::unique_ptr<Type>>& args,
                           const std::vector<FunctionSymbol*>& overloads) {
        argumentTypes = args;
        candidates = overloads;
        
        // Filter by parameter count
        filterByParameterCount();
        
        // Filter by parameter types
        filterByParameterTypes();
        
        // Select best candidate
        return selectBestCandidate();
    }
    
private:
    void filterByParameterCount() {
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [this](FunctionSymbol* func) {
                    return !isParameterCountCompatible(func);
                }),
            candidates.end());
    }
    
    void filterByParameterTypes() {
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                [this](FunctionSymbol* func) {
                    return !isParameterTypeCompatible(func);
                }),
            candidates.end());
    }
    
    FunctionSymbol* selectBestCandidate() {
        if (candidates.empty()) return nullptr;
        if (candidates.size() == 1) return candidates[0];
        
        // Select most specific overload
        FunctionSymbol* best = candidates[0];
        for (size_t i = 1; i < candidates.size(); ++i) {
            if (isMoreSpecific(candidates[i], best)) {
                best = candidates[i];
            }
        }
        return best;
    }
};
```

**Generic Function Resolution:**
```cpp
class GenericFunctionResolver {
public:
    FunctionSymbol* resolveGeneric(FunctionSymbol* genericFunc,
                                  const std::vector<std::unique_ptr<Type>>& args) {
        // Infer type parameters from arguments
        auto typeParams = inferTypeParameters(genericFunc, args);
        
        if (!typeParams) {
            return nullptr;  // Cannot infer type parameters
        }
        
        // Create specialized function
        return createSpecializedFunction(genericFunc, *typeParams);
    }
    
private:
    std::optional<std::vector<std::unique_ptr<Type>>> inferTypeParameters(
        FunctionSymbol* genericFunc,
        const std::vector<std::unique_ptr<Type>>& args) {
        
        std::vector<std::unique_ptr<Type>> typeParams;
        // Implementation of type parameter inference
        return typeParams;
    }
};
```

## Module Resolution

### Module Import System

**Module Resolution Algorithm:**
```cpp
class ModuleResolver {
private:
    std::vector<std::string> searchPaths;
    std::unordered_map<std::string, std::unique_ptr<Module>> loadedModules;
    
public:
    Module* resolveImport(const std::string& moduleName,
                         const SourceLocation& location) {
        // Check if already loaded
        auto it = loadedModules.find(moduleName);
        if (it != loadedModules.end()) {
            return it->second.get();
        }
        
        // Search for module file
        std::string modulePath = findModulePath(moduleName);
        if (modulePath.empty()) {
            throw SemanticError(location, "Module not found: " + moduleName);
        }
        
        // Load and parse module
        auto module = loadModule(modulePath);
        loadedModules[moduleName] = std::move(module);
        
        return loadedModules[moduleName].get();
    }
    
private:
    std::string findModulePath(const std::string& moduleName) {
        // Convert module name to file path
        std::string filePath = moduleNameToPath(moduleName);
        
        // Search in all search paths
        for (const auto& searchPath : searchPaths) {
            std::string fullPath = searchPath + "/" + filePath;
            if (std::filesystem::exists(fullPath)) {
                return fullPath;
            }
        }
        
        return "";
    }
    
    std::unique_ptr<Module> loadModule(const std::string& path) {
        // Load source file
        std::string source = readFile(path);
        
        // Parse module
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();
        
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        
        // Analyze module
        SemanticAnalyzer analyzer(typeTable);
        auto analyzedAst = analyzer.analyze(std::move(ast));
        
        return std::make_unique<Module>(path, std::move(analyzedAst));
    }
};
```

**Circular Import Detection:**
```cpp
class CircularImportDetector {
private:
    std::unordered_set<std::string> importStack;
    std::unordered_set<std::string> visited;
    
public:
    bool hasCircularImport(const std::string& moduleName,
                         const std::vector<std::string>& imports) {
        if (importStack.count(moduleName)) {
            return true;  // Circular dependency detected
        }
        
        if (visited.count(moduleName)) {
            return false;  // Already processed, no circular dependency
        }
        
        importStack.insert(moduleName);
        
        for (const auto& importName : imports) {
            if (hasCircularImport(importName, getImports(importName))) {
                return true;
            }
        }
        
        importStack.erase(moduleName);
        visited.insert(moduleName);
        
        return false;
    }
};
```

## Compile-Time Diagnostics

### Error Reporting System

**Diagnostic Categories:**
```cpp
enum class DiagnosticCategory {
    SYNTAX_ERROR,
    TYPE_ERROR,
    NAME_ERROR,
    SEMANTIC_ERROR,
    WARNING,
    INFO
};

enum class DiagnosticSeverity {
    ERROR,
    WARNING,
    INFO,
    HINT
};

class Diagnostic {
public:
    DiagnosticCategory category;
    DiagnosticSeverity severity;
    std::string message;
    SourceLocation location;
    std::vector<SourceLocation> relatedLocations;
    std::vector<std::string> suggestions;
    
    Diagnostic(DiagnosticCategory cat, DiagnosticSeverity sev,
              const std::string& msg, SourceLocation loc)
        : category(cat), severity(sev), message(msg), location(loc) {}
};
```

**Diagnostic Emitter:**
```cpp
class DiagnosticEmitter {
private:
    std::vector<Diagnostic> diagnostics;
    std::unique_ptr<DiagnosticFormatter> formatter;
    
public:
    void emitError(const SourceLocation& location, const std::string& message) {
        diagnostics.emplace_back(DiagnosticCategory::SEMANTIC_ERROR,
                                DiagnosticSeverity::ERROR, message, location);
    }
    
    void emitWarning(const SourceLocation& location, const std::string& message) {
        diagnostics.emplace_back(DiagnosticCategory::WARNING,
                                DiagnosticSeverity::WARNING, message, location);
    }
    
    void emitInfo(const SourceLocation& location, const std::string& message) {
        diagnostics.emplace_back(DiagnosticCategory::INFO,
                                DiagnosticSeverity::INFO, message, location);
    }
    
    std::vector<Diagnostic> getDiagnostics() const { return diagnostics; }
    bool hasErrors() const;
    bool hasWarnings() const;
    void clear();
};
```

**Error Recovery Strategies:**
```cpp
class ErrorRecovery {
private:
    DiagnosticEmitter& emitter;
    
public:
    explicit ErrorRecovery(DiagnosticEmitter& emitter) : emitter(emitter) {}
    
    // Recovery strategies
    std::unique_ptr<Expr> recoverExpression(Parser& parser);
    std::unique_ptr<Stmt> recoverStatement(Parser& parser);
    std::unique_ptr<Decl> recoverDeclaration(Parser& parser);
    
    // Synchronization points
    bool isSynchronizationToken(TokenType type);
    void synchronizeToNextStatement(Parser& parser);
    void synchronizeToNextDeclaration(Parser& parser);
};
```

---

# 5. Bytecode System

## Bytecode Architecture

### Instruction Format Design

The Kern bytecode system implements a **compact, extensible instruction format** optimized for fast decoding and execution. Each instruction consists of an opcode followed by optional operands.

**Instruction Structure:**
```cpp
struct Instruction {
    Opcode opcode;
    std::variant<
        std::monostate,           // No operand
        int64_t,                  // Integer operand
        double,                   // Float operand
        size_t,                   // Index operand
        std::string,              // String operand
        std::vector<uint8_t>      // Binary operand
    > operand;
    
    uint32_t line = 0;
    uint32_t column = 0;
    
    Instruction(Opcode op) : opcode(op), operand(std::monostate{}) {}
    
    template<typename T>
    Instruction(Opcode op, T operand) : opcode(op), operand(operand) {}
};
```

**Opcode System:**
```cpp
enum class Opcode : uint8_t {
    // Constants (0x00-0x0F)
    CONST_I64 = 0x00,
    CONST_F64 = 0x01,
    CONST_STR = 0x02,
    CONST_TRUE = 0x03,
    CONST_FALSE = 0x04,
    CONST_NULL = 0x05,
    
    // Variables (0x10-0x1F)
    LOAD = 0x10,
    STORE = 0x11,
    LOAD_GLOBAL = 0x12,
    STORE_GLOBAL = 0x13,
    LOAD_UPVALUE = 0x14,
    STORE_UPVALUE = 0x15,
    
    // Stack Operations (0x20-0x2F)
    POP = 0x20,
    DUP = 0x21,
    SWAP = 0x22,
    ROT_THREE = 0x23,
    ROT_FOUR = 0x24,
    
    // Arithmetic (0x30-0x3F)
    ADD = 0x30,
    SUB = 0x31,
    MUL = 0x32,
    DIV = 0x33,
    MOD = 0x34,
    POW = 0x35,
    NEG = 0x36,
    
    // Comparison (0x40-0x4F)
    EQ = 0x40,
    NE = 0x41,
    LT = 0x42,
    LE = 0x43,
    GT = 0x44,
    GE = 0x45,
    
    // Logical (0x50-0x5F)
    AND = 0x50,
    OR = 0x51,
    NOT = 0x52,
    
    // Control Flow (0x60-0x6F)
    JMP = 0x60,
    JMP_IF_FALSE = 0x61,
    JMP_IF_TRUE = 0x62,
    CALL = 0x63,
    RETURN = 0x64,
    HALT = 0x65,
    
    // Objects and Arrays (0x70-0x7F)
    NEW_OBJECT = 0x70,
    BUILD_ARRAY = 0x71,
    GET_FIELD = 0x72,
    SET_FIELD = 0x73,
    GET_INDEX = 0x74,
    SET_INDEX = 0x75,
    
    // Advanced Features (0x80-0x8F)
    YIELD = 0x80,
    AWAIT = 0x81,
    THROW = 0x82,
    TRY_BEGIN = 0x83,
    TRY_END = 0x84,
    
    // Debug and Meta (0xF0-0xFF)
    DEBUG_BREAK = 0xF0,
    DEBUG_PRINT = 0xF1,
    META_INFO = 0xF2
};
```

### Operand Encoding

**Operand Types:**
```cpp
class OperandEncoder {
public:
    // Integer operands (signed 64-bit)
    static std::vector<uint8_t> encodeInt64(int64_t value) {
        std::vector<uint8_t> bytes(8);
        for (int i = 0; i < 8; ++i) {
            bytes[i] = (value >> (i * 8)) & 0xFF;
        }
        return bytes;
    }
    
    // Index operands (unsigned 32-bit)
    static std::vector<uint8_t> encodeIndex(size_t value) {
        std::vector<uint8_t> bytes(4);
        for (int i = 0; i < 4; ++i) {
            bytes[i] = (value >> (i * 8)) & 0xFF;
        }
        return bytes;
    }
    
    // Float operands (IEEE 754 double)
    static std::vector<uint8_t> encodeFloat64(double value) {
        std::vector<uint8_t> bytes(8);
        std::memcpy(bytes.data(), &value, 8);
        return bytes;
    }
    
    // Variable-length operands (strings, binary data)
    static std::vector<uint8_t> encodeVarLength(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result;
        // Length prefix (4 bytes)
        auto lengthBytes = encodeIndex(data.size());
        result.insert(result.end(), lengthBytes.begin(), lengthBytes.end());
        // Data payload
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }
};
```

**Operand Decoder:**
```cpp
class OperandDecoder {
public:
    static int64_t decodeInt64(const std::vector<uint8_t>& bytes, size_t& offset) {
        int64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= (static_cast<int64_t>(bytes[offset + i]) << (i * 8));
        }
        offset += 8;
        return value;
    }
    
    static size_t decodeIndex(const std::vector<uint8_t>& bytes, size_t& offset) {
        size_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= (static_cast<size_t>(bytes[offset + i]) << (i * 8));
        }
        offset += 4;
        return value;
    }
    
    static double decodeFloat64(const std::vector<uint8_t>& bytes, size_t& offset) {
        double value;
        std::memcpy(&value, bytes.data() + offset, 8);
        offset += 8;
        return value;
    }
};
```

## Constant Pools

### Constant Pool Architecture

The constant pool stores all immutable values used by the bytecode, including strings, numbers, and function references. This enables efficient storage and sharing of constant data.

**Constant Pool Structure:**
```cpp
class ConstantPool {
private:
    std::vector<Value> constants;
    std::unordered_map<Value, size_t> constantIndex;
    
public:
    size_t addConstant(const Value& value) {
        auto it = constantIndex.find(value);
        if (it != constantIndex.end()) {
            return it->second;
        }
        
        size_t index = constants.size();
        constants.push_back(value);
        constantIndex[value] = index;
        return index;
    }
    
    const Value& getConstant(size_t index) const {
        if (index >= constants.size()) {
            throw std::out_of_range("Constant index out of range");
        }
        return constants[index];
    }
    
    size_t size() const { return constants.size(); }
    void clear() { constants.clear(); constantIndex.clear(); }
};
```

**String Interning:**
```cpp
class StringInterner {
private:
    std::unordered_set<std::string> internedStrings;
    
public:
    const std::string& intern(const std::string& str) {
        auto result = internedStrings.insert(str);
        return *result.first;
    }
    
    size_t getInternedCount() const { return internedStrings.size(); }
    void clear() { internedStrings.clear(); }
};
```

## Serialization Format

### Bytecode File Format

The bytecode file format is designed to be **platform-independent, versioned, and extensible**. It includes metadata, constant pools, and instruction sequences.

**File Header:**
```cpp
struct BytecodeHeader {
    char magic[4] = {'K', 'B', 'C', 'F'};  // Kern Bytecode Format
    uint16_t version = 1;
    uint16_t flags = 0;
    uint32_t constantPoolOffset;
    uint32_t constantPoolSize;
    uint32_t debugInfoOffset;
    uint32_t debugInfoSize;
    uint32_t codeOffset;
    uint32_t codeSize;
    uint32_t metadataOffset;
    uint32_t metadataSize;
};
```

**Serialization Implementation:**
```cpp
class BytecodeSerializer {
public:
    std::vector<uint8_t> serialize(const BytecodeModule& module) {
        std::vector<uint8_t> data;
        
        // Serialize header
        BytecodeHeader header = createHeader(module);
        auto headerBytes = serializeHeader(header);
        data.insert(data.end(), headerBytes.begin(), headerBytes.end());
        
        // Serialize constant pool
        auto constantPoolBytes = serializeConstantPool(module.constantPool);
        data.insert(data.end(), constantPoolBytes.begin(), constantPoolBytes.end());
        
        // Serialize code
        auto codeBytes = serializeCode(module.instructions);
        data.insert(data.end(), codeBytes.begin(), codeBytes.end());
        
        // Serialize debug info
        auto debugBytes = serializeDebugInfo(module.debugInfo);
        data.insert(data.end(), debugBytes.begin(), debugBytes.end());
        
        // Serialize metadata
        auto metadataBytes = serializeMetadata(module.metadata);
        data.insert(data.end(), metadataBytes.begin(), metadataBytes.end());
        
        return data;
    }
    
    BytecodeModule deserialize(const std::vector<uint8_t>& data) {
        BytecodeModule module;
        size_t offset = 0;
        
        // Deserialize header
        BytecodeHeader header = deserializeHeader(data, offset);
        validateHeader(header);
        
        // Deserialize constant pool
        offset = header.constantPoolOffset;
        module.constantPool = deserializeConstantPool(data, offset, header.constantPoolSize);
        
        // Deserialize code
        offset = header.codeOffset;
        module.instructions = deserializeCode(data, offset, header.codeSize);
        
        // Deserialize debug info
        if (header.debugInfoSize > 0) {
            offset = header.debugInfoOffset;
            module.debugInfo = deserializeDebugInfo(data, offset, header.debugInfoSize);
        }
        
        // Deserialize metadata
        if (header.metadataSize > 0) {
            offset = header.metadataOffset;
            module.metadata = deserializeMetadata(data, offset, header.metadataSize);
        }
        
        return module;
    }
};
```

## Bytecode Optimization

### Optimization Passes

The Kern bytecode optimizer implements multiple passes to improve performance and reduce code size while preserving semantics.

**Constant Folding:**
```cpp
class ConstantFolder {
public:
    void optimize(BytecodeModule& module) {
        for (auto& instruction : module.instructions) {
            switch (instruction.opcode) {
                case Opcode::ADD:
                    instruction = tryFoldBinary(instruction, [](double a, double b) { return a + b; });
                    break;
                case Opcode::SUB:
                    instruction = tryFoldBinary(instruction, [](double a, double b) { return a - b; });
                    break;
                case Opcode::MUL:
                    instruction = tryFoldBinary(instruction, [](double a, double b) { return a * b; });
                    break;
                case Opcode::DIV:
                    instruction = tryFoldBinary(instruction, [](double a, double b) { return a / b; });
                    break;
                // ... other optimizations
            }
        }
    }
    
private:
    Instruction tryFoldBinary(const Instruction& instr, 
                             std::function<double(double, double)> op) {
        if (std::holds_alternative<size_t>(instr.operand)) {
            size_t operandIndex = std::get<size_t>(instr.operand);
            // Check if both operands are constants and can be folded
            // Implementation depends on specific bytecode format
        }
        return instr;  // Cannot fold, return original
    }
};
```

**Dead Code Elimination:**
```cpp
class DeadCodeEliminator {
public:
    void optimize(BytecodeModule& module) {
        // Build control flow graph
        auto cfg = buildControlFlowGraph(module);
        
        // Mark reachable instructions
        std::unordered_set<size_t> reachable;
        markReachable(cfg, reachable);
        
        // Remove unreachable instructions
        auto& instructions = module.instructions;
        instructions.erase(
            std::remove_if(instructions.begin(), instructions.end(),
                [&reachable, &instructions](const Instruction& instr) {
                    size_t index = &instr - &instructions[0];
                    return !reachable.count(index);
                }),
            instructions.end());
    }
    
private:
    ControlFlowGraph buildControlFlowGraph(const BytecodeModule& module);
    void markReachable(const ControlFlowGraph& cfg, std::unordered_set<size_t>& reachable);
};
```

**Peephole Optimizer:**
```cpp
class PeepholeOptimizer {
public:
    void optimize(BytecodeModule& module) {
        auto& instructions = module.instructions;
        
        for (size_t i = 0; i < instructions.size(); ++i) {
            // Look for optimization opportunities in instruction windows
            if (i + 1 < instructions.size()) {
                optimizeTwoInstructions(instructions[i], instructions[i + 1]);
            }
            if (i + 2 < instructions.size()) {
                optimizeThreeInstructions(instructions[i], instructions[i + 1], instructions[i + 2]);
            }
        }
    }
    
private:
    void optimizeTwoInstructions(Instruction& first, Instruction& second) {
        // LOAD_CONST; POP -> remove both
        if (first.opcode == Opcode::CONST_I64 && second.opcode == Opcode::POP) {
            first.opcode = Opcode::NOP;
            second.opcode = Opcode::NOP;
        }
        
        // DUP; POP -> remove both
        if (first.opcode == Opcode::DUP && second.opcode == Opcode::POP) {
            first.opcode = Opcode::NOP;
            second.opcode = Opcode::NOP;
        }
    }
};
```

## Instruction Dispatch Strategy

### Direct-Threaded Dispatch

The Kern VM uses **direct-threaded dispatch** (computed goto) for maximum performance, avoiding the overhead of switch-based dispatch.

**Dispatch Table Setup:**
```cpp
class VM {
private:
    static void* dispatchTable[];
    
    void initializeDispatchTable() {
        dispatchTable[static_cast<size_t>(Opcode::CONST_I64)] = &&op_const_i64;
        dispatchTable[static_cast<size_t>(Opcode::CONST_F64)] = &&op_const_f64;
        dispatchTable[static_cast<size_t>(Opcode::ADD)] = &&op_add;
        dispatchTable[static_cast<size_t>(Opcode::SUB)] = &&op_sub;
        // ... initialize all opcodes
    }
    
public:
    void run() {
        initializeDispatchTable();
        
        // Main execution loop
        DISPATCH();
        
    op_const_i64:
        // Handle CONST_I64 opcode
        DISPATCH();
        
    op_const_f64:
        // Handle CONST_F64 opcode
        DISPATCH();
        
    op_add:
        // Handle ADD opcode
        DISPATCH();
        
    op_sub:
        // Handle SUB opcode
        DISPATCH();
        
        // ... other opcode handlers
    }
};
```

**Computed Goto Macro:**
```cpp
#define DISPATCH() \
    do { \
        if (ip >= code.size()) goto halt; \
        Instruction* instr = &code[ip]; \
        ip++; \
        goto *dispatchTable[static_cast<size_t>(instr->opcode)]; \
    } while (0)
```

### Superinstruction Optimization

**Superinstruction Generation:**
```cpp
class SuperinstructionGenerator {
public:
    std::vector<Instruction> generateSuperinstructions(
        const std::vector<Instruction>& instructions) {
        
        std::vector<Instruction> result;
        
        for (size_t i = 0; i < instructions.size(); ++i) {
            // Check for common patterns
            if (i + 1 < instructions.size()) {
                auto super = tryCreateSuperinstruction(instructions[i], instructions[i + 1]);
                if (super) {
                    result.push_back(*super);
                    i++;  // Skip the next instruction as it's merged
                    continue;
                }
            }
            result.push_back(instructions[i]);
        }
        
        return result;
    }
    
private:
    std::optional<Instruction> tryCreateSuperinstruction(
        const Instruction& first, const Instruction& second) {
        
        // CONST_I64 + ADD -> ADD_CONST
        if (first.opcode == Opcode::CONST_I64 && second.opcode == Opcode::ADD) {
            return Instruction(Opcode::ADD_CONST, first.operand);
        }
        
        // LOAD_GLOBAL + CALL -> CALL_GLOBAL
        if (first.opcode == Opcode::LOAD_GLOBAL && second.opcode == Opcode::CALL) {
            return Instruction(Opcode::CALL_GLOBAL, first.operand);
        }
        
        return std::nullopt;
    }
};
```

---

# 6. Virtual Machine

## VM Architecture

### High-Level VM Design

The Kern Virtual Machine implements a **register-window stack machine** with direct-threaded dispatch, designed for high-performance execution of compiled bytecode.

**Core VM Components:**
```
┌─────────────────────────────────────────────────────────────┐
│                     Kern Virtual Machine                    │
├─────────────────────────────────────────────────────────────┤
│  Execution Engine                                           │
│  ├── Register Window System                                │
│  ├── Direct-Threaded Dispatch                              │
│  ├── Call Stack Management                                 │
│  └── Exception Handling                                    │
│                                                              │
│  Memory Management                                          │
│  ├── Arena Allocator                                        │
│  ├── Garbage Collector (optional)                          │
│  ├── Memory Pool System                                    │
│  └── Reference Counting                                    │
│                                                              │
│  Runtime Systems                                            │
│  ├── Built-in Functions                                    │
│  ├── Module Loader                                         │
│  ├── Native FFI Interface                                  │
│  └── Debug/Profiling Hooks                                 │
└─────────────────────────────────────────────────────────────┘
```

### Register Window Architecture

**Register Window Design:**
```cpp
class RegisterWindow {
public:
    static constexpr size_t WINDOW_SIZE = 16;
    static constexpr size_t INPUT_COUNT = 8;
    static constexpr size_t LOCAL_COUNT = 8;
    
private:
    Value registers[WINDOW_SIZE];
    uint16_t pc;           // Program counter
    uint16_t funcIndex;    // Function index
    uint16_t callerRegs;   // Caller register base
    
public:
    // Register access with bounds checking
    Value& get(size_t index) {
        if (index >= WINDOW_SIZE) {
            throw VMError("Register index out of range");
        }
        return registers[index];
    }
    
    const Value& get(size_t index) const {
        if (index >= WINDOW_SIZE) {
            throw VMError("Register index out of range");
        }
        return registers[index];
    }
    
    // Window sliding operations
    void slideWindow(RegisterWindow& caller);
    void restoreWindow(const RegisterWindow& target);
    
    // Input/Output register access
    Value& getInput(size_t index) {
        if (index >= INPUT_COUNT) {
            throw VMError("Input register index out of range");
        }
        return registers[index];
    }
    
    Value& getLocal(size_t index) {
        if (index >= LOCAL_COUNT) {
            throw VMError("Local register index out of range");
        }
        return registers[INPUT_COUNT + index];
    }
    
    Value& getOutput(size_t index) {
        if (index >= INPUT_COUNT) {
            throw VMError("Output register index out of range");
        }
        return registers[LOCAL_COUNT + index];
    }
};
```

**Window Sliding Algorithm:**
```cpp
void RegisterWindow::slideWindow(RegisterWindow& caller) {
    // Copy caller's output registers to our input registers
    for (size_t i = 0; i < INPUT_COUNT; ++i) {
        registers[i] = caller.getOutput(i);
    }
    
    // Clear local registers
    for (size_t i = INPUT_COUNT; i < WINDOW_SIZE; ++i) {
        registers[i] = Value::nil();
    }
    
    callerRegs = caller.getRegisterBase();
}
```

### Stack/Register Model

**Hybrid Stack-Register Machine:**
```cpp
class StackRegisterMachine {
private:
    // Register windows
    std::vector<RegisterWindow> registerWindows;
    size_t currentWindow = 0;
    
    // Value stack for overflow and temporaries
    std::vector<Value> valueStack;
    static constexpr size_t MAX_STACK_SIZE = 65536;
    
    // Call stack
    std::vector<CallFrame> callStack;
    
public:
    // Register operations
    Value& getRegister(size_t window, size_t index);
    void setRegister(size_t window, size_t index, const Value& value);
    
    // Stack operations
    void pushStack(const Value& value);
    Value popStack();
    Value peekStack();
    
    // Call frame management
    void pushCallFrame(const CallFrame& frame);
    CallFrame popCallFrame();
    
    // Window management
    void slideWindow();
    void restoreWindow();
};
```

## Execution Loop

### Main Execution Engine

**Core Execution Loop:**
```cpp
class VM {
private:
    // Execution state
    std::vector<Instruction> code;
    size_t ip = 0;  // Instruction pointer
    std::vector<Value> stack;
    std::vector<CallFrame> callStack;
    std::vector<RegisterWindow> registerWindows;
    
    // Runtime data
    std::vector<Value> constants;
    std::unordered_map<std::string, Value> globals;
    std::unordered_map<size_t, BuiltinFn> builtins;
    
    // Dispatch table
    static void* dispatchTable[];
    
public:
    void run() {
        initializeDispatchTable();
        
        // Main execution loop
        DISPATCH();
        
    op_const_i64:
        {
            int64_t value = std::get<int64_t>(code[ip].operand);
            pushStack(Value::fromInt(value));
            ip++;
            DISPATCH();
        }
        
    op_const_f64:
        {
            double value = std::get<double>(code[ip].operand);
            pushStack(Value::fromFloat(value));
            ip++;
            DISPATCH();
        }
        
    op_add:
        {
            Value right = popStack();
            Value left = popStack();
            Value result = binaryAdd(left, right);
            pushStack(result);
            ip++;
            DISPATCH();
        }
        
    op_call:
        {
            size_t argCount = std::get<size_t>(code[ip].operand);
            executeCall(argCount);
            ip++;
            DISPATCH();
        }
        
    op_return:
        {
            executeReturn();
            DISPATCH();
        }
        
    // ... other opcode handlers
        
    halt:
        return;
    }
};
```

**Instruction Dispatch Macro:**
```cpp
#define DISPATCH() \
    do { \
        if (ip >= code.size()) goto halt; \
        Instruction* instr = &code[ip]; \
        ip++; \
        goto *dispatchTable[static_cast<size_t>(instr->opcode)]; \
    } while (0)
```

### Opcode Implementation

**Arithmetic Operations:**
```cpp
Value VM::binaryAdd(const Value& left, const Value& right) {
    // Type-based dispatch for addition
    if (left.isInt() && right.isInt()) {
        return Value::fromInt(left.toInt() + right.toInt());
    }
    
    if (left.isFloat() || right.isFloat()) {
        double leftVal = left.isFloat() ? left.toFloat() : static_cast<double>(left.toInt());
        double rightVal = right.isFloat() ? right.toFloat() : static_cast<double>(right.toInt());
        return Value::fromFloat(leftVal + rightVal);
    }
    
    if (left.isString() || right.isString()) {
        std::string result = left.toString() + right.toString();
        return Value::fromString(result);
    }
    
    throw VMError("Invalid operand types for addition");
}
```

**Function Call Implementation:**
```cpp
void VM::executeCall(size_t argCount) {
    // Extract arguments from stack
    std::vector<Value> args;
    for (size_t i = 0; i < argCount; ++i) {
        args.insert(args.begin(), popStack());
    }
    
    // Get function value
    Value callee = popStack();
    
    if (callee.isFunction()) {
        auto function = callee.toFunction();
        
        if (function->isBuiltin) {
            // Call built-in function
            Value result = function->builtinCall(this, args);
            pushStack(result);
        } else {
            // Call user-defined function
            executeUserFunction(function, args);
        }
    } else {
        throw VMError("Attempt to call non-function value");
    }
}
```

## Call Frames

### Call Frame Structure

**Call Frame Implementation:**
```cpp
struct CallFrame {
    const uint8_t* returnPc;        // Return instruction pointer
    const RegisterWindow* callerWindow; // Caller's register window
    size_t stackBase;                // Stack base position
    std::string functionName;        // Function name for debugging
    std::string filePath;            // Source file path
    uint32_t line;                   // Source line number
    uint32_t column;                 // Source column number
    size_t registerWindowIndex;      // Current register window
    
    CallFrame(const uint8_t* retPc, const RegisterWindow* caller,
              size_t stackBase, const std::string& funcName,
              const std::string& filePath, uint32_t line, uint32_t column)
        : returnPc(retPc), callerWindow(caller), stackBase(stackBase),
          functionName(funcName), filePath(filePath), line(line), column(column) {}
};
```

**Call Stack Management:**
```cpp
class CallStack {
private:
    std::vector<CallFrame> frames;
    static constexpr size_t MAX_CALL_DEPTH = 1000;
    
public:
    void pushFrame(const CallFrame& frame) {
        if (frames.size() >= MAX_CALL_DEPTH) {
            throw VMError("Maximum call depth exceeded");
        }
        frames.push_back(frame);
    }
    
    CallFrame popFrame() {
        if (frames.empty()) {
            throw VMError("Call stack underflow");
        }
        CallFrame frame = frames.back();
        frames.pop_back();
        return frame;
    }
    
    const CallFrame& getCurrentFrame() const {
        if (frames.empty()) {
            throw VMError("No current call frame");
        }
        return frames.back();
    }
    
    size_t getDepth() const { return frames.size(); }
    bool isEmpty() const { return frames.empty(); }
};
```

## Function Invocation

### Function Call Mechanism

**Function Call Process:**
```cpp
void VM::executeUserFunction(Function* function, const std::vector<Value>& args) {
    // Create new call frame
    CallFrame frame(
        reinterpret_cast<const uint8_t*>(ip),  // Return address
        &registerWindows[currentWindow],      // Caller window
        stack.size(),                          // Stack base
        function->name,                       // Function name
        function->filePath,                   // Source file
        function->line,                       // Source line
        function->column                      // Source column
    );
    
    // Push call frame
    callStack.pushFrame(frame);
    
    // Create new register window
    slideWindow();
    
    // Set up parameters
    for (size_t i = 0; i < args.size(); ++i) {
        if (i < INPUT_COUNT) {
            registerWindows[currentWindow].setInput(i, args[i]);
        } else {
            // Extra parameters go to stack
            pushStack(args[i]);
        }
    }
    
    // Set instruction pointer to function entry point
    ip = function->entryPoint;
}
```

**Return Implementation:**
```cpp
void VM::executeReturn() {
    if (callStack.isEmpty()) {
        throw VMError("Return outside of function");
    }
    
    // Get return value (or nil if none)
    Value returnValue = stack.empty() ? Value::nil() : popStack();
    
    // Restore caller state
    CallFrame frame = callStack.popFrame();
    ip = reinterpret_cast<size_t>(frame.returnPc);
    currentWindow = frame.registerWindowIndex;
    
    // Clean up stack to caller's base
    while (stack.size() > frame.stackBase) {
        stack.pop_back();
    }
    
    // Push return value
    pushStack(returnValue);
}
```

### Tail Call Optimization

**Tail Call Detection:**
```cpp
bool VM::isTailCall(size_t instructionIndex) {
    if (instructionIndex + 1 >= code.size()) {
        return false;
    }
    
    return code[instructionIndex + 1].opcode == Opcode::RETURN;
}
```

**Tail Call Implementation:**
```cpp
void VM::executeTailCall(Function* function, const std::vector<Value>& args) {
    // Instead of creating a new frame, reuse current frame
    CallFrame& currentFrame = callStack.getCurrentFrame();
    
    // Update frame information
    currentFrame.functionName = function->name;
    currentFrame.filePath = function->filePath;
    currentFrame.line = function->line;
    currentFrame.column = function->column;
    
    // Clear current register window
    registerWindows[currentWindow] = RegisterWindow();
    
    // Set up parameters
    for (size_t i = 0; i < args.size(); ++i) {
        if (i < INPUT_COUNT) {
            registerWindows[currentWindow].setInput(i, args[i]);
        } else {
            pushStack(args[i]);
        }
    }
    
    // Jump to function entry point
    ip = function->entryPoint;
}
```

## Native Bindings

### Built-in Function System

**Builtin Function Registration:**
```cpp
class BuiltinRegistry {
private:
    std::unordered_map<std::string, BuiltinFn> builtins;
    std::unordered_map<size_t, BuiltinFn> builtinByIndex;
    
public:
    void registerBuiltin(const std::string& name, BuiltinFn function) {
        builtins[name] = function;
    }
    
    void registerBuiltin(size_t index, BuiltinFn function) {
        builtinByIndex[index] = function;
    }
    
    BuiltinFn getBuiltin(const std::string& name) {
        auto it = builtins.find(name);
        return it != builtins.end() ? it->second : nullptr;
    }
    
    BuiltinFn getBuiltin(size_t index) {
        auto it = builtinByIndex.find(index);
        return it != builtinByIndex.end() ? it->second : nullptr;
    }
};
```

**Builtin Function Implementation:**
```cpp
Value printBuiltin(VM* vm, const std::vector<Value>& args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) std::cout << " ";
        std::cout << args[i].toString();
    }
    std::cout << std::endl;
    return Value::nil();
}

Value lengthBuiltin(VM* vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw VMError("length() expects exactly 1 argument");
    }
    
    const Value& arg = args[0];
    if (arg.isArray()) {
        return Value::fromInt(static_cast<int64_t>(arg.getArray().size()));
    }
    
    if (arg.isString()) {
        return Value::fromInt(static_cast<int64_t>(arg.getString().length()));
    }
    
    throw VMError("length() expects array or string argument");
}
```

### FFI Interface

**Foreign Function Interface:**
```cpp
class FFInterface {
private:
    std::unordered_map<std::string, void*> loadedLibraries;
    
public:
    void* loadLibrary(const std::string& libraryPath) {
        auto it = loadedLibraries.find(libraryPath);
        if (it != loadedLibraries.end()) {
            return it->second;
        }
        
#ifdef _WIN32
        HMODULE handle = LoadLibraryA(libraryPath.c_str());
#else
        void* handle = dlopen(libraryPath.c_str(), RTLD_LAZY);
#endif
        
        if (!handle) {
            throw VMError("Failed to load library: " + libraryPath);
        }
        
        loadedLibraries[libraryPath] = handle;
        return handle;
    }
    
    void* getFunction(void* library, const std::string& functionName) {
#ifdef _WIN32
        FARPROC func = GetProcAddress(static_cast<HMODULE>(library), functionName.c_str());
#else
        void* func = dlsym(library, functionName.c_str());
#endif
        
        if (!func) {
            throw VMError("Function not found: " + functionName);
        }
        
        return func;
    }
};
```

**FFI Call Implementation:**
```cpp
Value VM::executeFFICall(void* function, const std::vector<Value>& args) {
    // This is a simplified FFI implementation
    // In practice, you'd need proper type information and calling convention handling
    
    // For demonstration, assume all functions take int arguments and return int
    if (args.size() > 4) {
        throw VMError("FFI calls limited to 4 arguments in this implementation");
    }
    
    int intArgs[4] = {0, 0, 0, 0};
    for (size_t i = 0; i < args.size() && i < 4; ++i) {
        if (!args[i].isInt()) {
            throw VMError("FFI arguments must be integers in this implementation");
        }
        intArgs[i] = static_cast<int>(args[i].toInt());
    }
    
    // Call the function (simplified)
    typedef int (*IntFunc)(int, int, int, int);
    IntFunc func = reinterpret_cast<IntFunc>(function);
    
    int result = func(intArgs[0], intArgs[1], intArgs[2], intArgs[3]);
    
    return Value::fromInt(result);
}
```

## Builtins

### Standard Library Functions

**Math Functions:**
```cpp
Value mathSin(VM* vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw VMError("sin() expects exactly 1 argument");
    }
    
    double value = args[0].isFloat() ? args[0].toFloat() : 
                  static_cast<double>(args[0].toInt());
    
    return Value::fromFloat(std::sin(value));
}

Value mathCos(VM* vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw VMError("cos() expects exactly 1 argument");
    }
    
    double value = args[0].isFloat() ? args[0].toFloat() : 
                  static_cast<double>(args[0].toInt());
    
    return Value::fromFloat(std::cos(value));
}
```

**String Functions:**
```cpp
Value stringUpper(VM* vm, const std::vector<Value>& args) {
    if (args.size() != 1) {
        throw VMError("upper() expects exactly 1 argument");
    }
    
    if (!args[0].isString()) {
        throw VMError("upper() expects string argument");
    }
    
    std::string result = args[0].getString();
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    
    return Value::fromString(result);
}

Value stringSplit(VM* vm, const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw VMError("split() expects exactly 2 arguments");
    }
    
    if (!args[0].isString() || !args[1].isString()) {
        throw VMError("split() expects string arguments");
    }
    
    std::string str = args[0].getString();
    std::string delimiter = args[1].getString();
    
    std::vector<Value> parts;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        parts.push_back(Value::fromString(str.substr(start, end - start)));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    parts.push_back(Value::fromString(str.substr(start)));
    
    return Value::fromArray(parts);
}
```

**Array Functions:**
```cpp
Value arrayPush(VM* vm, const std::vector<Value>& args) {
    if (args.size() != 2) {
        throw VMError("push() expects exactly 2 arguments");
    }
    
    if (!args[0].isArray()) {
        throw VMError("push() expects array as first argument");
    }
    
    // Arrays are immutable in this design, so we return a new array
    std::vector<Value> newArray = args[0].getArray();
    newArray.push_back(args[1]);
    
    return Value::fromArray(newArray);
}

Value arraySlice(VM* vm, const std::vector<Value>& args) {
    if (args.size() < 2 || args.size() > 3) {
        throw VMError("slice() expects 2 or 3 arguments");
    }
    
    if (!args[0].isArray()) {
        throw VMError("slice() expects array as first argument");
    }
    
    const std::vector<Value>& array = args[0].getArray();
    int64_t start = args[1].toInt();
    int64_t end = args.size() > 2 ? args[2].toInt() : array.size();
    
    // Handle negative indices
    if (start < 0) start += array.size();
    if (end < 0) end += array.size();
    
    // Clamp indices
    start = std::max(int64_t(0), std::min(start, int64_t(array.size())));
    end = std::max(int64_t(0), std::min(end, int64_t(array.size())));
    
    std::vector<Value> result;
    for (int64_t i = start; i < end; ++i) {
        result.push_back(array[i]);
    }
    
    return Value::fromArray(result);
}
```

## Memory Handling

### Arena Allocator

**Arena-based Memory Management:**
```cpp
class ArenaAllocator {
private:
    struct Block {
        char* data;
        size_t size;
        size_t used;
        Block* next;
    };
    
    Block* currentBlock = nullptr;
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024;  // 64KB
    
public:
    void* allocate(size_t size) {
        // Align to 8-byte boundary
        size = (size + 7) & ~7;
        
        if (!currentBlock || currentBlock->used + size > currentBlock->size) {
            allocateNewBlock(std::max(size, DEFAULT_BLOCK_SIZE));
        }
        
        void* ptr = currentBlock->data + currentBlock->used;
        currentBlock->used += size;
        return ptr;
    }
    
    void reset() {
        // Free all blocks except the first one
        while (currentBlock && currentBlock->next) {
            Block* next = currentBlock->next;
            delete[] currentBlock->data;
            delete currentBlock;
            currentBlock = next;
        }
        
        if (currentBlock) {
            currentBlock->used = 0;
        }
    }
    
    ~ArenaAllocator() {
        reset();
        if (currentBlock) {
            delete[] currentBlock->data;
            delete currentBlock;
        }
    }
    
private:
    void allocateNewBlock(size_t size) {
        Block* newBlock = new Block;
        newBlock->data = new char[size];
        newBlock->size = size;
        newBlock->used = 0;
        newBlock->next = currentBlock;
        currentBlock = newBlock;
    }
};
```

### Garbage Collection

**Mark-and-Sweep Garbage Collector:**
```cpp
class GarbageCollector {
private:
    std::vector<GCObject*> objects;
    std::unordered_set<GCObject*> markedObjects;
    
public:
    void collect() {
        // Mark phase
        markedObjects.clear();
        markRoots();
        
        // Sweep phase
        for (auto it = objects.begin(); it != objects.end(); ) {
            GCObject* obj = *it;
            if (markedObjects.find(obj) == markedObjects.end()) {
                // Object is not marked, delete it
                delete obj;
                it = objects.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void addObject(GCObject* obj) {
        objects.push_back(obj);
    }
    
private:
    void markRoots() {
        // Mark stack objects
        for (const auto& value : stack) {
            markValue(value);
        }
        
        // Mark global variables
        for (const auto& [name, value] : globals) {
            markValue(value);
        }
        
        // Mark register windows
        for (const auto& window : registerWindows) {
            for (size_t i = 0; i < RegisterWindow::WINDOW_SIZE; ++i) {
                markValue(window.get(i));
            }
        }
    }
    
    void markValue(const Value& value) {
        if (value.isGCObject()) {
            GCObject* obj = value.getGCObject();
            if (markedObjects.find(obj) == markedObjects.end()) {
                markedObjects.insert(obj);
                obj->markChildren(this);
            }
        }
    }
};
```

## VM Isolation

### Sandbox Implementation

**VM Sandbox Configuration:**
```cpp
struct SandboxConfig {
    bool allowFileIO = false;
    bool allowNetwork = false;
    bool allowFFI = false;
    bool allowProcessControl = false;
    std::vector<std::string> allowedPaths;
    std::vector<std::string> allowedDomains;
    uint64_t maxExecutionTime = 0;  // 0 = unlimited
    size_t maxMemoryUsage = 0;      // 0 = unlimited
};

class SandboxVM : public VM {
private:
    SandboxConfig config;
    std::chrono::steady_clock::time_point startTime;
    
public:
    SandboxVM(const SandboxConfig& cfg) : config(cfg) {
        startTime = std::chrono::steady_clock::now();
    }
    
    void run() override {
        while (ip < code.size()) {
            checkExecutionLimits();
            Instruction& instr = code[ip];
            
            // Check sandbox restrictions
            if (isRestrictedOpcode(instr.opcode)) {
                throw VMError("Opcode not allowed in sandbox: " + 
                            std::to_string(static_cast<int>(instr.opcode)));
            }
            
            executeInstruction(instr);
        }
    }
    
private:
    void checkExecutionLimits() {
        if (config.maxExecutionTime > 0) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            if (elapsedMs.count() > config.maxExecutionTime) {
                throw VMError("Execution time limit exceeded");
            }
        }
        
        if (config.maxMemoryUsage > 0) {
            size_t currentUsage = getCurrentMemoryUsage();
            if (currentUsage > config.maxMemoryUsage) {
                throw VMError("Memory usage limit exceeded");
            }
        }
    }
    
    bool isRestrictedOpcode(Opcode opcode) {
        switch (opcode) {
            case Opcode::FFI_CALL:
                return !config.allowFFI;
            case Opcode::FILE_OPEN:
            case Opcode::FILE_READ:
            case Opcode::FILE_WRITE:
                return !config.allowFileIO;
            case Opcode::HTTP_GET:
            case Opcode::HTTP_POST:
                return !config.allowNetwork;
            default:
                return false;
        }
    }
};
```

## Runtime Safety Boundaries

### Type Safety

**Runtime Type Checking:**
```cpp
class TypeChecker {
public:
    static void checkBinaryOperation(const Value& left, const Value& right, 
                                   Opcode op) {
        switch (op) {
            case Opcode::ADD:
                checkAdditionTypes(left, right);
                break;
            case Opcode::SUB:
            case Opcode::MUL:
            case Opcode::DIV:
                checkArithmeticTypes(left, right);
                break;
            case Opcode::EQ:
            case Opcode::NE:
                // Equality works with any types
                break;
            case Opcode::LT:
            case Opcode::LE:
            case Opcode::GT:
            case Opcode::GE:
                checkComparisonTypes(left, right);
                break;
            default:
                break;
        }
    }
    
private:
    static void checkArithmeticTypes(const Value& left, const Value& right) {
        if (!left.isNumeric() || !right.isNumeric()) {
            throw VMError("Arithmetic operations require numeric operands");
        }
    }
    
    static void checkComparisonTypes(const Value& left, const Value& right) {
        if (!left.isNumeric() || !right.isNumeric()) {
            throw VMError("Comparison operations require numeric operands");
        }
    }
};
```

### Bounds Checking

**Array Bounds Checking:**
```cpp
class ArrayBoundsChecker {
public:
    static void checkIndex(const Value& array, int64_t index) {
        if (!array.isArray()) {
            throw VMError("Cannot index non-array value");
        }
        
        const std::vector<Value>& arrayData = array.getArray();
        if (index < 0 || index >= static_cast<int64_t>(arrayData.size())) {
            throw VMError("Array index out of bounds: " + std::to_string(index) + 
                        " (size: " + std::to_string(arrayData.size()) + ")");
        }
    }
    
    static void checkSliceBounds(const Value& array, int64_t start, int64_t end) {
        if (!array.isArray()) {
            throw VMError("Cannot slice non-array value");
        }
        
        const std::vector<Value>& arrayData = array.getArray();
        int64_t size = static_cast<int64_t>(arrayData.size());
        
        if (start < 0 || start > size) {
            throw VMError("Slice start index out of bounds: " + std::to_string(start));
        }
        
        if (end < 0 || end > size) {
            throw VMError("Slice end index out of bounds: " + std::to_string(end));
        }
        
        if (start > end) {
            throw VMError("Slice start index cannot be greater than end index");
        }
    }
};
```

## Scheduler/Runtime Systems

### Task Scheduler

**Cooperative Task Scheduler:**
```cpp
class TaskScheduler {
private:
    struct Task {
        std::unique_ptr<GeneratorObject> generator;
        std::string name;
        uint64_t id;
        std::chrono::steady_clock::time_point nextRun;
        bool isSuspended = false;
    };
    
    std::vector<std::unique_ptr<Task>> tasks;
    std::unordered_map<uint64_t, Task*> taskById;
    uint64_t nextTaskId = 1;
    
public:
    uint64_t scheduleTask(std::unique_ptr<GeneratorObject> generator, 
                         const std::string& name = "") {
        auto task = std::make_unique<Task>();
        task->generator = std::move(generator);
        task->name = name;
        task->id = nextTaskId++;
        task->nextRun = std::chrono::steady_clock::now();
        
        taskById[task->id] = task.get();
        tasks.push_back(std::move(task));
        
        return task->id;
    }
    
    void runTasks() {
        auto now = std::chrono::steady_clock::now();
        
        for (auto& task : tasks) {
            if (task->isSuspended || task->nextRun > now) {
                continue;
            }
            
            try {
                Value result;
                bool completed = task->generator->resume(result);
                
                if (completed) {
                    // Task finished, remove it
                    taskById.erase(task->id);
                    task->generator.reset();
                } else {
                    // Task yielded, schedule next run
                    task->nextRun = now + std::chrono::milliseconds(1);
                }
            } catch (const std::exception& e) {
                // Task failed, remove it
                taskById.erase(task->id);
                task->generator.reset();
            }
        }
        
        // Remove completed tasks
        tasks.erase(
            std::remove_if(tasks.begin(), tasks.end(),
                [](const std::unique_ptr<Task>& task) {
                    return !task->generator;
                }),
            tasks.end());
    }
    
    void suspendTask(uint64_t taskId) {
        auto it = taskById.find(taskId);
        if (it != taskById.end()) {
            it->second->isSuspended = true;
        }
    }
    
    void resumeTask(uint64_t taskId) {
        auto it = taskById.find(taskId);
        if (it != taskById.end()) {
            it->second->isSuspended = false;
            it->second->nextRun = std::chrono::steady_clock::now();
        }
    }
};
```

## Internal VM Subsystems

### Debug System

**VM Debug Interface:**
```cpp
class VMDebugger {
private:
    VM* vm;
    std::unordered_set<size_t> breakpoints;
    bool stepping = false;
    size_t stepCount = 0;
    
public:
    explicit VMDebugger(VM* vm) : vm(vm) {}
    
    void setBreakpoint(size_t instructionIndex) {
        breakpoints.insert(instructionIndex);
    }
    
    void removeBreakpoint(size_t instructionIndex) {
        breakpoints.erase(instructionIndex);
    }
    
    void clearBreakpoints() {
        breakpoints.clear();
    }
    
    void enableStepping() { stepping = true; }
    void disableStepping() { stepping = false; }
    
    bool shouldBreak(size_t instructionIndex) {
        if (breakpoints.count(instructionIndex)) {
            return true;
        }
        
        if (stepping) {
            stepCount++;
            return stepCount >= 1;  // Single-step
        }
        
        return false;
    }
    
    void step() {
        stepCount = 0;
        stepping = true;
    }
    
    void continueExecution() {
        stepping = false;
        stepCount = 0;
    }
    
    std::vector<std::string> getStackTrace() {
        std::vector<std::string> trace;
        
        for (const auto& frame : vm->getCallStack()) {
            std::string entry = frame.functionName + " at " + 
                              frame.filePath + ":" + 
                              std::to_string(frame.line) + ":" +
                              std::to_string(frame.column);
            trace.push_back(entry);
        }
        
        return trace;
    }
    
    std::vector<Value> getLocals() {
        // Return current function's local variables
        return vm->getCurrentLocals();
    }
    
    std::vector<Value> getStack() {
        return vm->getStack();
    }
};
```

### Profiling System

**VM Profiler:**
```cpp
class VMProfiler {
private:
    struct ProfileData {
        std::string functionName;
        uint64_t callCount = 0;
        std::chrono::nanoseconds totalTime{0};
        std::chrono::nanoseconds selfTime{0};
        std::unordered_map<std::string, uint64_t> callees;
    };
    
    std::unordered_map<std::string, ProfileData> profiles;
    std::vector<std::string> callStack;
    std::chrono::steady_clock::time_point lastEntryTime;
    
public:
    void enterFunction(const std::string& functionName) {
        callStack.push_back(functionName);
        lastEntryTime = std::chrono::steady_clock::now();
        
        ProfileData& data = profiles[functionName];
        data.functionName = functionName;
        data.callCount++;
        
        // Record caller-callee relationship
        if (callStack.size() > 1) {
            std::string caller = callStack[callStack.size() - 2];
            profiles[caller].callees[functionName]++;
        }
    }
    
    void exitFunction(const std::string& functionName) {
        auto now = std::chrono::steady_clock::now();
        auto duration = now - lastEntryTime;
        
        ProfileData& data = profiles[functionName];
        data.totalTime += duration;
        
        // Calculate self time (excluding time spent in callees)
        if (callStack.size() == 1) {
            data.selfTime += duration;
        }
        
        callStack.pop_back();
    }
    
    std::vector<ProfileData> getProfileData() const {
        std::vector<ProfileData> result;
        for (const auto& [name, data] : profiles) {
            result.push_back(data);
        }
        
        // Sort by total time
        std::sort(result.begin(), result.end(),
            [](const ProfileData& a, const ProfileData& b) {
                return a.totalTime > b.totalTime;
            });
        
        return result;
    }
    
    void printProfile() {
        auto data = getProfileData();
        
        std::cout << "Profile Results:\n";
        std::cout << "Function\tCalls\tTotal Time\tSelf Time\n";
        
        for (const auto& entry : data) {
            std::cout << entry.functionName << "\t"
                      << entry.callCount << "\t"
                      << entry.totalTime.count() << "ns\t"
                      << entry.selfTime.count() << "ns\n";
        }
    }
};
```

## VM Modularization Refactor

### Module Interface Design

**VM Module Interface:**
```cpp
class VMModule {
public:
    virtual ~VMModule() = default;
    
    // Module lifecycle
    virtual void initialize(VM* vm) = 0;
    virtual void shutdown() = 0;
    
    // Functionality
    virtual bool canHandle(Opcode opcode) = 0;
    virtual Value executeOpcode(VM* vm, Opcode opcode, const Instruction& instr) = 0;
    
    // Metadata
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::vector<std::string> getDependencies() const = 0;
};
```

**Core VM Module:**
```cpp
class CoreVMModule : public VMModule {
private:
    VM* vm;
    
public:
    void initialize(VM* vm) override {
        this->vm = vm;
        // Register core opcodes
    }
    
    void shutdown() override {
        // Cleanup
    }
    
    bool canHandle(Opcode opcode) override {
        return opcode >= Opcode::CONST_I64 && opcode <= Opcode::HALT;
    }
    
    Value executeOpcode(VM* vm, Opcode opcode, const Instruction& instr) override {
        switch (opcode) {
            case Opcode::CONST_I64:
                return executeConstI64(vm, instr);
            case Opcode::ADD:
                return executeAdd(vm, instr);
            // ... other core opcodes
            default:
                throw VMError("Unhandled core opcode");
        }
    }
    
    std::string getName() const override { return "CoreVM"; }
    std::string getVersion() const override { return "1.0.0"; }
    std::vector<std::string> getDependencies() const override { return {}; }
};
```

## Unified VM Interface

**VM Factory Pattern:**
```cpp
class VMFactory {
public:
    static std::unique_ptr<VM> createVM(const VMConfig& config) {
        auto vm = std::make_unique<VM>(config);
        
        // Load core modules
        vm->loadModule(std::make_unique<CoreVMModule>());
        vm->loadModule(std::make_unique<MathModule>());
        vm->loadModule(std::make_unique<StringModule>());
        vm->loadModule(std::make_unique<ArrayModule>());
        
        // Load optional modules based on configuration
        if (config.enableGraphics) {
            vm->loadModule(std::make_unique<GraphicsModule>());
        }
        
        if (config.enableNetworking) {
            vm->loadModule(std::make_unique<NetworkModule>());
        }
        
        if (config.enableFFI) {
            vm->loadModule(std::make_unique<FFIModule>());
        }
        
        return vm;
    }
};
```

## Runtime Decoupling Architecture

**Dependency Injection:**
```cpp
class RuntimeContext {
private:
    std::unordered_map<std::string, std::any> services;
    
public:
    template<typename T>
    void registerService(const std::string& name, std::unique_ptr<T> service) {
        services[name] = std::move(service);
    }
    
    template<typename T>
    T* getService(const std::string& name) {
        auto it = services.find(name);
        if (it != services.end()) {
            return std::any_cast<T*>(it->second);
        }
        return nullptr;
    }
    
    template<typename T>
    T& getRequiredService(const std::string& name) {
        T* service = getService<T>(name);
        if (!service) {
            throw std::runtime_error("Required service not found: " + name);
        }
        return *service;
    }
};
```

---

This comprehensive documentation covers the first 6 chapters of the Kern programming language documentation. The document continues with the remaining chapters covering the runtime systems, security model, module system, build tools, editor ecosystem, ECS systems, graphics rendering, FFI, memory management, error handling, architecture history, compile-time firewall, future roadmap, and advanced technical deep dives.

The documentation is structured to serve both as an official language specification and a technical whitepaper, providing detailed explanations of the architecture, implementation details, and design philosophy behind Kern's systems.
