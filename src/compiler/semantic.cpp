#include "compiler/semantic.hpp"

#include "compiler/lexer.hpp"
#include "compiler/token.hpp"
#include "errors.hpp"
#include "vm/builtins.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#endif

#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace kern {

namespace {

static bool isDeclaredKeywordContext(TokenType t) {
    return t == TokenType::LET || t == TokenType::VAR ||
           t == TokenType::DEF || t == TokenType::CLASS || t == TokenType::STRUCT || t == TokenType::ENUM || t == TokenType::IMPORT ||
           t == TokenType::CATCH || t == TokenType::FOR || t == TokenType::WITH || t == TokenType::AS;
}

static std::string inferLiteralType(const std::string& expr) {
    std::string e = expr;
    while (!e.empty() && (e.back() == ';' || e.back() == ' ' || e.back() == '\t' || e.back() == '\r')) e.pop_back();
    if (e.empty()) return "";
    if (e == "true" || e == "false") return "bool";
    if (std::regex_match(e, std::regex(R"re(-?[0-9]+)re"))) return "int";
    if (std::regex_match(e, std::regex(R"re(-?[0-9]*\.[0-9]+)re"))) return "float";
    if (std::regex_match(e, std::regex(R"re("([^"\\]|\\.)*")re"))) return "string";
    return "";
}

static void addDiag(SemanticResult& r, SemanticSeverity sev, const std::string& file, int line, int col,
                    const std::string& code, const std::string& msg) {
    r.diagnostics.push_back({sev, file, line, col, code, msg});
    if (sev == SemanticSeverity::Error) r.hasError = true;
}

} // namespace

const char* semanticSeverityName(SemanticSeverity s) {
    switch (s) {
        case SemanticSeverity::Info: return "info";
        case SemanticSeverity::Warning: return "warning";
        default: return "error";
    }
}

SemanticResult analyzeSemanticSource(const std::string& source, const std::string& filePath, bool strictTypes) {
    SemanticResult out;

    std::unordered_set<std::string> declared;
    std::unordered_set<std::string> builtins;
    insertAllBuiltinNamesForAnalysis(builtins);
    builtins.insert("__import");
    builtins.insert("has_key");
    builtins.insert("clock");
    builtins.insert("sleep");
    builtins.insert("push");
    builtins.insert("keys");
    builtins.insert("len");
    builtins.insert("int");
    builtins.insert("float");
    builtins.insert("str");
    builtins.insert("del");
    builtins.insert("from");  // extern def ... from "dll"

    std::unordered_map<std::string, std::string> declaredTypes;
    std::unordered_set<std::string> unsafeRequiredNames = {
        "alloc","free","realloc","alloc_zeroed","alloc_aligned","alloc_tracked","free_tracked",
        "mem_copy","mem_set","mem_move","mem_cmp","mem_set_zero","mem_zero","mem_swap","mem_xor","mem_fill_pattern",
        "poke8","poke16","poke32","poke64","poke_float","poke_double",
        "volatile_store8","volatile_store16","volatile_store32","volatile_store64",
        "map_file","unmap_file","memory_protect","ffi_call"
    };
    std::unordered_set<std::string> externFns;
    std::regex declTypedRe(R"re(\b(?:let|var|const)\s+([A-Za-z_][A-Za-z0-9_]*)\s*:\s*([A-Za-z_][A-Za-z0-9_\*]*)\s*(?:=\s*(.+))?)re");
    std::regex assignRe(R"re(\b([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.+))re");
    std::regex defRe(R"re(^\s*(?:extern\s+)?def\s+[A-Za-z_][A-Za-z0-9_]*\s*\(([^)]*)\))re");
    std::regex lambdaRe(R"re(\blambda\s*\(([^)]*)\)\s*=>)re");

    // pre-scan function parameter names so token scan does not flag them.
    {
        std::istringstream pss(source);
        std::string ln;
        std::smatch m;
        while (std::getline(pss, ln)) {
            if (std::regex_search(ln, m, defRe) && m.size() >= 2) {
                std::stringstream ss(m[1].str());
                std::string p;
                while (std::getline(ss, p, ',')) {
                    p = std::regex_replace(p, std::regex(R"re(^\s+|\s+$)re"), "");
                    if (p.empty()) continue;
                    size_t c = p.find(':');
                    if (c != std::string::npos) p = p.substr(0, c);
                    size_t ws = p.find_last_of(" \t");
                    if (ws != std::string::npos) p = p.substr(ws + 1);
                    p = std::regex_replace(p, std::regex(R"re(^\s+|\s+$)re"), "");
                    if (!p.empty()) declared.insert(p);
                }
            }
            if (std::regex_search(ln, m, lambdaRe) && m.size() >= 2) {
                std::stringstream ss(m[1].str());
                std::string p;
                while (std::getline(ss, p, ',')) {
                    p = std::regex_replace(p, std::regex(R"re(^\s+|\s+$)re"), "");
                    if (p.empty()) continue;
                    size_t c = p.find(':');
                    if (c != std::string::npos) p = p.substr(0, c);
                    size_t ws = p.find_last_of(" \t");
                    if (ws != std::string::npos) p = p.substr(ws + 1);
                    p = std::regex_replace(p, std::regex(R"re(^\s+|\s+$)re"), "");
                    if (!p.empty()) declared.insert(p);
                }
            }
        }
    }

    try {
        Lexer lx(source);
        auto toks = lx.tokenize();
        int unsafeDepth = 0;
        bool awaitingUnsafeBlock = false;
        bool inExternDecl = false;
        int enumDepth = 0;
        bool awaitingEnumBlock = false;
        for (size_t i = 0; i < toks.size(); ++i) {
            const Token& t = toks[i];
            if (t.type == TokenType::UNSAFE) {
                awaitingUnsafeBlock = true;
                continue;
            }
            if (t.type == TokenType::EXTERN) {
                inExternDecl = true;
                if (i + 2 < toks.size() &&
                    (toks[i + 1].type == TokenType::DEF || toks[i + 1].type == TokenType::FUNCTION) &&
                    toks[i + 2].type == TokenType::IDENTIFIER) {
                    externFns.insert(toks[i + 2].lexeme);
                }
                continue;
            }
            if (t.type == TokenType::NEWLINE) {
                inExternDecl = false;
                continue;
            }
            if (t.type == TokenType::CATCH) {
                // catch (e) { ... } => declare e in semantic prepass to avoid false undeclared warnings.
                if (i + 2 < toks.size() && toks[i + 1].type == TokenType::LPAREN &&
                    toks[i + 2].type == TokenType::IDENTIFIER) {
                    declared.insert(toks[i + 2].lexeme);
                }
                continue;
            }
            if (t.type == TokenType::ENUM) {
                awaitingEnumBlock = true;
                continue;
            }
            if (t.type == TokenType::LBRACE) {
                if (awaitingUnsafeBlock) ++unsafeDepth;
                if (awaitingEnumBlock) ++enumDepth;
                awaitingUnsafeBlock = false;
                awaitingEnumBlock = false;
                continue;
            }
            if (t.type == TokenType::RBRACE) {
                if (unsafeDepth > 0) --unsafeDepth;
                if (enumDepth > 0) --enumDepth;
                awaitingUnsafeBlock = false;
                awaitingEnumBlock = false;
                continue;
            }
            if (t.type != TokenType::IDENTIFIER) continue;
            if (enumDepth > 0) {
                declared.insert(t.lexeme);
                continue;
            }
            if (i > 0 && (toks[i - 1].type == TokenType::DOT || toks[i - 1].type == TokenType::QUESTION_DOT)) continue;

            bool prevConst = false;
            if (i > 0) {
                std::string prevLex = toks[i - 1].lexeme;
                for (char& c : prevLex) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                prevConst = (prevLex == "const");
            }
            bool inferredDeclByAssign = (i + 1 < toks.size() && toks[i + 1].type == TokenType::ASSIGN);
            bool inferredDeclByTypeColon = (i + 1 < toks.size() && toks[i + 1].type == TokenType::COLON);
            bool isUnsafeRequired = unsafeRequiredNames.find(t.lexeme) != unsafeRequiredNames.end();
            if (isUnsafeRequired && unsafeDepth <= 0 && !inExternDecl) {
                addDiag(out, SemanticSeverity::Error, filePath, t.line, t.column,
                        "SAFE-UNSAFE-REQUIRED",
                        "Unsafe operation '" + t.lexeme + "' requires an unsafe block.");
            }
            // extern def Name(...) uses Name( in the declaration; that is not a call.
            // only actual calls to an extern function require unsafe { ... }.
            if (externFns.find(t.lexeme) != externFns.end() &&
                i + 1 < toks.size() && toks[i + 1].type == TokenType::LPAREN &&
                unsafeDepth <= 0 && !inExternDecl) {
                const bool isExternDeclHead =
                    (i >= 2 && toks[i - 2].type == TokenType::EXTERN &&
                     (toks[i - 1].type == TokenType::DEF || toks[i - 1].type == TokenType::FUNCTION));
                if (!isExternDeclHead) {
                    addDiag(out, SemanticSeverity::Error, filePath, t.line, t.column,
                            "SAFE-UNSAFE-REQUIRED",
                            "FFI call '" + t.lexeme + "' requires an unsafe block.");
                }
            }
            if (i > 0 && (isDeclaredKeywordContext(toks[i - 1].type) || prevConst)) {
                declared.insert(t.lexeme);
            } else if (inferredDeclByAssign && !declared.count(t.lexeme)) {
                declared.insert(t.lexeme);
            } else if (inferredDeclByTypeColon && !declared.count(t.lexeme)) {
                declared.insert(t.lexeme);
            } else if (!declared.count(t.lexeme) && !builtins.count(t.lexeme)) {
                addDiag(out, SemanticSeverity::Warning, filePath, t.line, t.column,
                        "SEM_UNDECLARED", "Possible undeclared symbol '" + t.lexeme + "'");
            }
        }
    } catch (const std::exception& e) {
        addDiag(out, SemanticSeverity::Error, filePath, 1, 1,
                "SEM-INTERNAL",
                std::string("Semantic analysis failed: ") + e.what());
    } catch (...) {
        addDiag(out, SemanticSeverity::Error, filePath, 1, 1,
                "SEM-INTERNAL",
                "Semantic analysis failed with an unknown exception.");
    }

    std::istringstream iss(source);
    std::string line;
    int lineNo = 0;
    while (std::getline(iss, line)) {
        ++lineNo;
        std::smatch m;
        if (std::regex_search(line, m, declTypedRe) && m.size() >= 3) {
            std::string name = m[1].str();
            std::string ty = m[2].str();
            declaredTypes[name] = ty;
            if (m.size() >= 4 && m[3].matched && strictTypes) {
                std::string rhsTy = inferLiteralType(m[3].str());
                if (!rhsTy.empty()) {
                    bool ok = (ty == rhsTy) || (ty == "double" && rhsTy == "float") || (ty == "float" && rhsTy == "int");
                    if (!ok) {
                        addDiag(out, SemanticSeverity::Error, filePath, lineNo, 1, "SEM_TYPE_MISMATCH",
                                "Type mismatch: variable '" + name + "' is '" + ty + "' but initializer is '" + rhsTy + "'");
                    }
                }
            }
            continue;
        }

        if (strictTypes && std::regex_search(line, m, assignRe) && m.size() >= 3) {
            std::string name = m[1].str();
            auto it = declaredTypes.find(name);
            if (it != declaredTypes.end()) {
                std::string rhsTy = inferLiteralType(m[2].str());
                if (!rhsTy.empty()) {
                    const std::string& ty = it->second;
                    bool ok = (ty == rhsTy) || (ty == "double" && rhsTy == "float") || (ty == "float" && rhsTy == "int");
                    if (!ok) {
                        addDiag(out, SemanticSeverity::Error, filePath, lineNo, 1, "SEM_ASSIGN_MISMATCH",
                                "Type mismatch on assignment to '" + name + "': expected '" + ty + "', got '" + rhsTy + "'");
                    }
                }
            }
        }
    }

    return out;
}

static std::string semanticDiagnosticDetail(const std::string& code, const std::string&) {
    if (code == "SEM_UNDECLARED")
        return "Heuristic scan only: the identifier was not seen in a let/def/import header and is not a known builtin.\n"
               "Dynamic imports, callbacks, and string-based dispatch can still be valid at runtime.\n"
               "If this is a false positive, declare the name earlier or ignore for generated glue code.";
    if (code == "SEM_TYPE_MISMATCH")
        return "Under strict typing (preview/experimental feature sets), annotated variables are checked against simple "
               "literal initializers (bool/int/float/string).\n"
               "Widen the annotation, change the literal, or use an untyped `let` if values are computed later.";
    if (code == "SEM_ASSIGN_MISMATCH")
        return "Strict mode compared a typed binding to a literal on the right-hand side of `=`.\n"
               "Update the literal, change the stored type, or relax strict checking in the project feature set.";
    if (code == "SAFE-UNSAFE-REQUIRED")
        return "This operation is treated as low-level/unsafe and is only allowed inside `unsafe { ... }`.\n"
               "Wrap the call in an unsafe block, or use a safe higher-level alternative.";
    return "See the primary message; this diagnostic comes from the semantic pre-pass over tokens and types.";
}

static std::string semanticDiagnosticHint(const std::string& code) {
    if (code == "SEM_UNDECLARED")
        return "Add let/def/import before first use, or confirm the symbol is provided by a dynamic/module path.";
    if (code == "SEM_TYPE_MISMATCH" || code == "SEM_ASSIGN_MISMATCH")
        return "Align declared types with literals, or remove strict typing for this binding.";
    if (code == "SAFE-UNSAFE-REQUIRED")
        return "Use `unsafe { ... }` for raw memory/FFI operations.";
    return "Cross-check declarations, imports, and feature-set strictness.";
}

bool applySemanticDiagnosticsToReporter(const std::string& source, const std::string& filePath, bool strictTypes) {
    SemanticResult sem = analyzeSemanticSource(source, filePath, strictTypes);
    for (const auto& d : sem.diagnostics) {
        if (d.severity == SemanticSeverity::Warning) {
            g_errorReporter.reportWarning(d.line, d.column, d.message, semanticDiagnosticHint(d.code), d.code,
                semanticDiagnosticDetail(d.code, d.message));
        } else if (d.severity == SemanticSeverity::Error) {
            g_errorReporter.reportCompileError(ErrorCategory::TypeError, d.line, d.column, d.message,
                semanticDiagnosticHint(d.code), d.code, semanticDiagnosticDetail(d.code, d.message));
        }
    }
    return sem.hasError;
}

} // namespace kern
