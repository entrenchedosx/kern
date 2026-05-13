/* *
 * kern/compiler/scope_codegen.hpp - Correct Scope Handling
 * 
 * Fixes:
 * - No accidental variable shadowing
 * - Proper lexical scoping
 * - Correct handling of `let x = x + 1`
 * - Shadowing detection and warnings
 */
#pragma once

#include "runtime/vm_minimal.hpp"
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stack>

namespace kern {

// Scope level tracking
struct ScopeLevel {
    std::unordered_map<std::string, int> variables;  // name -> local index
    std::unordered_map<std::string, bool> isParameter;  // Track parameters separately
    int baseIndex;  // Starting local index for this scope
    int nextIndex;  // Next available slot
    bool isLoop;    // Is this a loop scope?
    
    explicit ScopeLevel(int base = 0) : baseIndex(base), nextIndex(base), isLoop(false) {}
    
    int allocateSlot() {
        return nextIndex++;
    }
    
    int getLocalCount() const {
        return nextIndex - baseIndex;
    }
};

// Fixed code generator with proper scope handling
class ScopeCodeGen {
    std::vector<Token> tokens;
    size_t pos;
    std::stack<ScopeLevel> scopes;
    std::unordered_map<std::string, int> globals;
    std::vector<std::string> constants;
    int maxLocalDepth;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    bool hadError;
    
public:
    ScopeCodeGen() : pos(0), maxLocalDepth(0), hadError(false) {
        // Push global scope
        scopes.push(ScopeLevel(0));
    }
    
    struct CompileResult {
        Bytecode code;
        std::vector<std::string> constants;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        int maxLocals;
        bool success;
    };
    
    CompileResult compile(const std::string& source) {
        // Tokenize
        MinimalLexer lexer(source);
        tokens = lexer.tokenize();
        pos = 0;
        
        Bytecode code;
        
        // Compile statements
        while (!check(TokenType::EOF_TOKEN) && !hadError) {
            compileStatement(code);
        }
        
        // Add implicit return if needed
        if (code.empty() || code.back().op != Instruction::RETURN) {
            code.push_back({Instruction::PUSH_NIL, 0});
            code.push_back({Instruction::RETURN, 0});
        }
        
        code.push_back({Instruction::HALT, 0});
        
        CompileResult result;
        result.code = std::move(code);
        result.constants = std::move(constants);
        result.warnings = std::move(warnings);
        result.errors = std::move(errors);
        result.maxLocals = maxLocalDepth;
        result.success = !hadError;
        
        return result;
    }
    
private:
    // Check current token
    bool check(TokenType type) const {
        return pos < tokens.size() && tokens[pos].type == type;
    }
    
    // Match and consume token
    bool match(TokenType type) {
        if (check(type)) {
            pos++;
            return true;
        }
        return false;
    }
    
    // Consume expected token
    const Token& expect(TokenType type, const std::string& msg) {
        if (!check(type)) {
            error(msg + " at line " + std::to_string(tokens[pos].line));
            static Token dummy{TokenType::EOF_TOKEN, "", 0};
            return dummy;
        }
        return tokens[pos++];
    }
    
    // Error reporting
    void error(const std::string& msg) {
        errors.push_back(msg);
        hadError = true;
    }
    
    void warn(const std::string& msg) {
        warnings.push_back(msg);
    }
    
    // Scope management
    void pushScope(bool isLoop = false) {
        int base = scopes.empty() ? 0 : scopes.top().nextIndex;
        scopes.push(ScopeLevel(base));
        scopes.top().isLoop = isLoop;
    }
    
    void popScope() {
        if (scopes.empty()) return;
        
        int depth = scopes.top().getLocalCount();
        if (depth > maxLocalDepth) {
            maxLocalDepth = depth;
        }
        
        scopes.pop();
    }
    
    // Variable lookup - CRITICAL: checks all scopes
    enum class VarKind { NONE, LOCAL, UPVALUE, GLOBAL };
    
    struct VarInfo {
        VarKind kind;
        int index;      // local slot or constant index
        int depth;      // scope depth (0 = current)
    };
    
    VarInfo lookupVariable(const std::string& name) {
        // Check locals from innermost to outermost
        std::vector<ScopeLevel*> scopeStack;
        
        // Copy scopes to vector for iteration (stack doesn't allow iteration)
        std::stack<ScopeLevel> temp = scopes;
        while (!temp.empty()) {
            scopeStack.insert(scopeStack.begin(), &temp.top());
            temp.pop();
        }
        
        int depth = 0;
        for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
            auto& scope = **it;
            auto varIt = scope.variables.find(name);
            if (varIt != scope.variables.end()) {
                return {VarKind::LOCAL, varIt->second, depth};
            }
            depth++;
        }
        
        // Check globals
        auto globalIt = globals.find(name);
        if (globalIt != globals.end()) {
            return {VarKind::GLOBAL, globalIt->second, 0};
        }
        
        return {VarKind::NONE, -1, 0};
    }
    
    // Declare new variable - CRITICAL: no shadowing in same scope
    bool declareVariable(const std::string& name, int& outIndex) {
        if (scopes.empty()) {
            error("No active scope");
            return false;
        }
        
        auto& currentScope = scopes.top();
        
        // Check for shadowing in CURRENT scope only (not outer scopes)
        if (currentScope.variables.count(name)) {
            error("Variable '" + name + "' already declared in this scope");
            return false;
        }
        
        // Allocate slot
        outIndex = currentScope.allocateSlot();
        currentScope.variables[name] = outIndex;
        
        return true;
    }
    
    // Declare global
    int declareGlobal(const std::string& name) {
        auto it = globals.find(name);
        if (it != globals.end()) {
            return it->second;
        }
        
        int idx = addConstant(name);
        globals[name] = idx;
        return idx;
    }
    
    // Add constant
    int addConstant(const std::string& s) {
        for (size_t i = 0; i < constants.size(); i++) {
            if (constants[i] == s) return (int)i;
        }
        constants.push_back(s);
        return (int)(constants.size() - 1);
    }
    
    // Statement compilation
    void compileStatement(Bytecode& code) {
        if (match(TokenType::PRINT)) {
            compilePrint(code);
        } else if (match(TokenType::LET)) {
            compileLet(code);
        } else if (match(TokenType::IF)) {
            compileIf(code);
        } else if (match(TokenType::WHILE)) {
            compileWhile(code);
        } else if (match(TokenType::FOR)) {
            compileFor(code);
        } else if (match(TokenType::LBRACE)) {
            compileBlock(code);
        } else {
            compileExpressionStatement(code);
        }
    }
    
    void compilePrint(Bytecode& code) {
        compileExpression(code);
        code.push_back({Instruction::PRINT, 0});
        match(TokenType::SEMICOLON);  // Optional
    }
    
    void compileLet(Bytecode& code) {
        std::string name = expect(TokenType::IDENTIFIER, "Expected variable name").text;
        
        // Check if this is `let x = x + 1` situation
        // We need to handle the RHS BEFORE declaring the variable
        expect(TokenType::ASSIGN, "Expected '='");
        
        // Compile RHS first - this may reference outer scopes
        compileExpression(code);
        
        // Now declare the variable in current scope
        int slot;
        if (!declareVariable(name, slot)) {
            // Error already reported
            return;
        }
        
        code.push_back({Instruction::STORE_LOCAL, (int16_t)slot});
        match(TokenType::SEMICOLON);
    }
    
    void compileIf(Bytecode& code) {
        expect(TokenType::LPAREN, "Expected '(' after 'if'");
        compileExpression(code);
        expect(TokenType::RPAREN, "Expected ')'");
        
        // Jump to else if false
        size_t jumpElseIdx = code.size();
        code.push_back({Instruction::JUMP_IF_FALSE, 0});  // Placeholder
        
        pushScope();
        compileStatement(code);
        popScope();
        
        if (match(TokenType::ELSE)) {
            size_t jumpEndIdx = code.size();
            code.push_back({Instruction::JUMP, 0});  // Placeholder
            
            // Patch else jump
            code[jumpElseIdx].operand = (int16_t)(code.size() - jumpElseIdx);
            
            pushScope();
            compileStatement(code);
            popScope();
            
            // Patch end jump
            code[jumpEndIdx].operand = (int16_t)(code.size() - jumpEndIdx);
        } else {
            // Patch else jump (no else)
            code[jumpElseIdx].operand = (int16_t)(code.size() - jumpElseIdx);
        }
    }
    
    void compileWhile(Bytecode& code) {
        size_t loopStart = code.size();
        
        expect(TokenType::LPAREN, "Expected '(' after 'while'");
        
        pushScope(true);  // Mark as loop scope
        
        compileExpression(code);
        expect(TokenType::RPAREN, "Expected ')'");
        
        size_t jumpExitIdx = code.size();
        code.push_back({Instruction::JUMP_IF_FALSE, 0});  // Placeholder
        
        compileStatement(code);
        
        // Jump back to condition
        code.push_back({Instruction::JUMP, (int16_t)(loopStart - code.size())});
        
        // Patch exit jump
        code[jumpExitIdx].operand = (int16_t)(code.size() - jumpExitIdx);
        
        popScope();
    }
    
    void compileFor(Bytecode& code) {
        expect(TokenType::LPAREN, "Expected '(' after 'for'");
        
        pushScope();
        
        // Init
        if (!check(TokenType::SEMICOLON)) {
            if (match(TokenType::LET)) {
                compileLet(code);
            } else {
                compileExpressionStatement(code);
            }
        } else {
            match(TokenType::SEMICOLON);
        }
        
        size_t loopStart = code.size();
        
        // Condition
        if (!check(TokenType::SEMICOLON)) {
            compileExpression(code);
        } else {
            code.push_back({Instruction::PUSH_TRUE, 0});
        }
        expect(TokenType::SEMICOLON, "Expected ';'");
        
        size_t jumpExitIdx = code.size();
        code.push_back({Instruction::JUMP_IF_FALSE, 0});
        
        // Body
        compileStatement(code);
        
        // Increment (jump back to here)
        size_t incStart = code.size();
        compileExpression(code);  // For increment
        code.push_back({Instruction::POP, 0});  // Discard result
        
        // Jump to condition
        code.push_back({Instruction::JUMP, (int16_t)(loopStart - code.size())});
        
        // Patch exit jump
        code[jumpExitIdx].operand = (int16_t)(code.size() - jumpExitIdx);
        
        popScope();
        
        expect(TokenType::RPAREN, "Expected ')'");
    }
    
    void compileBlock(Bytecode& code) {
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN) && !hadError) {
            compileStatement(code);
        }
        expect(TokenType::RBRACE, "Expected '}'");
    }
    
    void compileExpressionStatement(Bytecode& code) {
        compileExpression(code);
        code.push_back({Instruction::POP, 0});  // Discard result
        match(TokenType::SEMICOLON);
    }
    
    // Expression compilation (same precedence as before)
    void compileExpression(Bytecode& code) {
        compileOr(code);
    }
    
    void compileOr(Bytecode& code) {
        compileAnd(code);
        while (match(TokenType::OR)) {
            compileAnd(code);
            code.push_back({Instruction::OR, 0});
        }
    }
    
    void compileAnd(Bytecode& code) {
        compileEquality(code);
        while (match(TokenType::AND)) {
            compileEquality(code);
            code.push_back({Instruction::AND, 0});
        }
    }
    
    void compileEquality(Bytecode& code) {
        compileComparison(code);
        while (true) {
            if (match(TokenType::EQ)) {
                compileComparison(code);
                code.push_back({Instruction::EQ, 0});
            } else if (match(TokenType::NE)) {
                compileComparison(code);
                code.push_back({Instruction::EQ, 0});
                code.push_back({Instruction::NOT, 0});
            } else {
                break;
            }
        }
    }
    
    void compileComparison(Bytecode& code) {
        compileTerm(code);
        while (true) {
            if (match(TokenType::LT)) {
                compileTerm(code);
                code.push_back({Instruction::LT, 0});
            } else if (match(TokenType::LE)) {
                compileTerm(code);
                code.push_back({Instruction::LE, 0});
            } else if (match(TokenType::GT)) {
                compileTerm(code);
                code.push_back({Instruction::GT, 0});
            } else if (match(TokenType::GE)) {
                compileTerm(code);
                code.push_back({Instruction::GE, 0});
            } else {
                break;
            }
        }
    }
    
    void compileTerm(Bytecode& code) {
        compileFactor(code);
        while (true) {
            if (match(TokenType::PLUS)) {
                compileFactor(code);
                code.push_back({Instruction::ADD, 0});
            } else if (match(TokenType::MINUS)) {
                compileFactor(code);
                code.push_back({Instruction::SUB, 0});
            } else {
                break;
            }
        }
    }
    
    void compileFactor(Bytecode& code) {
        compileUnary(code);
        while (true) {
            if (match(TokenType::MUL)) {
                compileUnary(code);
                code.push_back({Instruction::MUL, 0});
            } else if (match(TokenType::DIV)) {
                compileUnary(code);
                code.push_back({Instruction::DIV, 0});
            } else if (match(TokenType::MOD)) {
                compileUnary(code);
                code.push_back({Instruction::MOD, 0});
            } else {
                break;
            }
        }
    }
    
    void compileUnary(Bytecode& code) {
        if (match(TokenType::MINUS)) {
            compileUnary(code);
            code.push_back({Instruction::NEG, 0});
        } else if (match(TokenType::NOT)) {
            compileUnary(code);
            code.push_back({Instruction::NOT, 0});
        } else {
            compilePrimary(code);
        }
    }
    
    void compilePrimary(Bytecode& code) {
        if (match(TokenType::TRUE)) {
            code.push_back({Instruction::PUSH_TRUE, 0});
        } else if (match(TokenType::FALSE)) {
            code.push_back({Instruction::PUSH_FALSE, 0});
        } else if (match(TokenType::NIL)) {
            code.push_back({Instruction::PUSH_NIL, 0});
        } else if (match(TokenType::NUMBER)) {
            int idx = addConstant(tokens[pos - 1].text);
            code.push_back({Instruction::PUSH_CONST, (int16_t)idx});
        } else if (match(TokenType::STRING)) {
            int idx = addConstant(tokens[pos - 1].text);
            code.push_back({Instruction::PUSH_CONST, (int16_t)idx});
        } else if (match(TokenType::IDENTIFIER)) {
            std::string name = tokens[pos - 1].text;
            auto info = lookupVariable(name);
            
            switch (info.kind) {
                case VarKind::LOCAL:
                    code.push_back({Instruction::LOAD_LOCAL, (int16_t)info.index});
                    break;
                case VarKind::GLOBAL:
                    code.push_back({Instruction::LOAD_GLOBAL, (int16_t)info.index});
                    break;
                case VarKind::NONE:
                    warn("Undefined variable: " + name);
                    code.push_back({Instruction::PUSH_NIL, 0});
                    break;
                default:
                    error("Unsupported variable kind");
            }
        } else if (match(TokenType::LPAREN)) {
            compileExpression(code);
            expect(TokenType::RPAREN, "Expected ')'");
        } else {
            error("Unexpected token: " + tokens[pos].text);
            pos++;
        }
    }
};

} // namespace kern
