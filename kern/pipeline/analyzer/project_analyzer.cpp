#include "analyzer/project_analyzer.hpp"

#include "compiler/ast.hpp"
#include "compiler/import_aliases.hpp"
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "utils/kernconfig.hpp"
#include "compiler/builtin_names.hpp"
#include "errors/errors.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace kern {
namespace fs = std::filesystem;

namespace {

struct FileEditPlan {
    bool addImport = false;
    std::vector<std::string> importsToAdd;
    std::set<int> removeLines;
    std::vector<std::pair<int, std::string>> insertBeforeLine;
    std::vector<std::string> reasons;
};

struct ParsedFileInfo {
    std::string path;
    std::string source;
    std::vector<std::string> lines;
    std::vector<size_t> lineOffsets;
    std::vector<std::pair<std::string, int>> imports;
    std::vector<std::pair<std::string, int>> varDecls;
    std::unordered_set<std::string> declared;
    std::unordered_set<std::string> used;
    bool parseOk = true;
};

static std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

static std::vector<std::string> splitLines(const std::string& source) {
    std::vector<std::string> out;
    std::istringstream iss(source);
    std::string line;
    while (std::getline(iss, line)) out.push_back(line);
    if (!source.empty() && source.back() == '\n') out.push_back("");
    return out;
}

static std::vector<size_t> computeLineOffsets(const std::string& source) {
    std::vector<size_t> starts;
    starts.push_back(0);
    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') starts.push_back(i + 1);
    }
    return starts;
}

static int lineFromOffset(const std::vector<size_t>& starts, size_t offset) {
    auto it = std::upper_bound(starts.begin(), starts.end(), offset);
    if (it == starts.begin()) return 1;
    return static_cast<int>((it - starts.begin()));
}

static std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static std::string lowerCopy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string jsonEscapeLocal(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) out += ' ';
                else out += c;
                break;
        }
    }
    return out;
}

static void addIssue(AnalyzerReport& report, AnalyzerIssue issue) {
    report.issues.push_back(issue);
    if (issue.severity == IssueSeverity::Critical) ++report.criticalCount;
    else if (issue.severity == IssueSeverity::Warning) ++report.warningCount;
    else ++report.infoCount;
}

static std::vector<std::string> collectProjectSplFiles(const fs::path& root) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (it->is_directory()) {
            std::string dn = lowerCopy(it->path().filename().string());
            if (dn == ".git" || dn == ".kern-fix-backups" || dn == "build" || dn == "dist" || dn == "final") {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file()) continue;
        fs::path p = it->path();
        std::string ext = lowerCopy(p.extension().string());
        if (ext == ".kn") out.push_back(fs::weakly_canonical(p, ec).string());
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

static std::vector<std::string> collectSplConfigFiles(const fs::path& root) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::exists(root, ec)) return out;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (it->is_directory()) {
            std::string dn = lowerCopy(it->path().filename().string());
            if (dn == ".git" || dn == ".kern-fix-backups" || dn == "build" || dn == "dist" || dn == "final") {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file()) continue;
        if (lowerCopy(it->path().filename().string()) == "kernconfig.json") {
            out.push_back(fs::weakly_canonical(it->path(), ec).string());
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

static std::unordered_set<std::string> keywordLikeIdentifiers() {
    return {
        "if", "else", "for", "while", "return", "break", "continue", "import", "include",
        "let", "var", "const", "def", "class", "try", "catch", "finally", "throw", "rethrow",
        "true", "false", "null", "in", "match", "case", "new"
    };
}

static std::unordered_set<std::string> builtinNames() {
    std::unordered_set<std::string> out;
    insertAllBuiltinNamesForAnalysis(out);
    return out;
}

/* * matches codegen.cpp binding name for ImportStmt.*/
static std::string importBindingName(const ImportStmt* imp) {
    std::string bindingName = imp->hasAlias ? imp->alias : imp->moduleName;
    if (!imp->hasAlias && bindingName.size() >= 4 &&
        bindingName.compare(bindingName.size() - 4, 4, ".kn") == 0) {
        bindingName = bindingName.substr(0, bindingName.size() - 4);
    }
    return bindingName;
}

// match/case patterns: collect Identifier leaves (except _) as bindings.
static void addMatchPatternBindingIds(const Expr* e, ParsedFileInfo& file) {
    if (!e) return;
    if (const auto* id = dynamic_cast<const Identifier*>(e)) {
        if (id->name != "_") file.declared.insert(id->name);
        return;
    }
    if (const auto* b = dynamic_cast<const BinaryExpr*>(e)) {
        addMatchPatternBindingIds(b->left.get(), file);
        addMatchPatternBindingIds(b->right.get(), file);
        return;
    }
    if (const auto* u = dynamic_cast<const UnaryExpr*>(e)) {
        addMatchPatternBindingIds(u->operand.get(), file);
        return;
    }
    if (const auto* c = dynamic_cast<const CallExpr*>(e)) {
        addMatchPatternBindingIds(c->callee.get(), file);
        for (const auto& a : c->args) addMatchPatternBindingIds(a.expr.get(), file);
        return;
    }
    if (const auto* t = dynamic_cast<const TernaryExpr*>(e)) {
        addMatchPatternBindingIds(t->condition.get(), file);
        addMatchPatternBindingIds(t->thenExpr.get(), file);
        addMatchPatternBindingIds(t->elseExpr.get(), file);
        return;
    }
    if (const auto* ix = dynamic_cast<const IndexExpr*>(e)) {
        addMatchPatternBindingIds(ix->object.get(), file);
        addMatchPatternBindingIds(ix->index.get(), file);
        return;
    }
    if (const auto* oix = dynamic_cast<const OptionalIndexExpr*>(e)) {
        addMatchPatternBindingIds(oix->object.get(), file);
        addMatchPatternBindingIds(oix->index.get(), file);
        return;
    }
    if (const auto* me = dynamic_cast<const MemberExpr*>(e)) {
        addMatchPatternBindingIds(me->object.get(), file);
        return;
    }
    if (const auto* sl = dynamic_cast<const SliceExpr*>(e)) {
        addMatchPatternBindingIds(sl->object.get(), file);
        addMatchPatternBindingIds(sl->start.get(), file);
        addMatchPatternBindingIds(sl->end.get(), file);
        addMatchPatternBindingIds(sl->step.get(), file);
        return;
    }
    if (const auto* oc = dynamic_cast<const OptionalChainExpr*>(e)) {
        addMatchPatternBindingIds(oc->object.get(), file);
        return;
    }
    if (const auto* co = dynamic_cast<const CoalesceExpr*>(e)) {
        addMatchPatternBindingIds(co->left.get(), file);
        addMatchPatternBindingIds(co->right.get(), file);
        return;
    }
    if (const auto* r = dynamic_cast<const RangeExpr*>(e)) {
        addMatchPatternBindingIds(r->start.get(), file);
        addMatchPatternBindingIds(r->end.get(), file);
        addMatchPatternBindingIds(r->step.get(), file);
        return;
    }
}

static void collectAstDeclarationsFromExpr(const Expr* e, ParsedFileInfo& file);

static void collectAstDeclarationsFromStmt(const Stmt* s, ParsedFileInfo& file) {
    if (!s) return;
    if (const auto* v = dynamic_cast<const VarDeclStmt*>(s)) {
        file.declared.insert(v->name);
        // enum declarations lower to const map<int>; register member names as bindings.
        if (const auto* map = dynamic_cast<const MapLiteral*>(v->initializer.get())) {
            bool allIntVals = true;
            for (const auto& pr : map->entries) {
                if (!dynamic_cast<const IntLiteral*>(pr.second.get())) {
                    allIntVals = false;
                    break;
                }
            }
            if (allIntVals) {
                for (const auto& pr : map->entries) {
                    if (const auto* k = dynamic_cast<const StringLiteral*>(pr.first.get()))
                        file.declared.insert(k->value);
                }
            }
        }
        collectAstDeclarationsFromExpr(v->initializer.get(), file);
        return;
    }
    if (const auto* d = dynamic_cast<const DestructureStmt*>(s)) {
        for (const auto& n : d->names) file.declared.insert(n);
        collectAstDeclarationsFromExpr(d->initializer.get(), file);
        return;
    }
    if (const auto* fn = dynamic_cast<const FunctionDeclStmt*>(s)) {
        file.declared.insert(fn->name);
        for (const auto& p : fn->params) {
            file.declared.insert(p.name);
            collectAstDeclarationsFromExpr(p.defaultExpr.get(), file);
        }
        collectAstDeclarationsFromStmt(fn->body.get(), file);
        return;
    }
    if (const auto* c = dynamic_cast<const ClassDeclStmt*>(s)) {
        file.declared.insert(c->name);
        if (c->hasSuper && !c->superClass.empty()) file.declared.insert(c->superClass);
        for (const auto& m : c->members) file.declared.insert(m.second);
        for (const auto& m : c->methods) collectAstDeclarationsFromStmt(m.get(), file);
        for (const auto& st : c->constructorBody) collectAstDeclarationsFromStmt(st.get(), file);
        return;
    }
    if (const auto* imp = dynamic_cast<const ImportStmt*>(s)) {
        if (!imp->namedImports.empty()) {
            for (const auto& n : imp->namedImports) file.declared.insert(n);
        } else {
            file.declared.insert(importBindingName(imp));
        }
        return;
    }
    if (const auto* fi = dynamic_cast<const ForInStmt*>(s)) {
        file.declared.insert(fi->varName);
        if (!fi->valueVarName.empty()) file.declared.insert(fi->valueVarName);
        collectAstDeclarationsFromExpr(fi->iterable.get(), file);
        collectAstDeclarationsFromStmt(fi->body.get(), file);
        return;
    }
    if (const auto* fr = dynamic_cast<const ForRangeStmt*>(s)) {
        file.declared.insert(fr->varName);
        collectAstDeclarationsFromExpr(fr->start.get(), file);
        collectAstDeclarationsFromExpr(fr->end.get(), file);
        collectAstDeclarationsFromExpr(fr->step.get(), file);
        collectAstDeclarationsFromStmt(fr->body.get(), file);
        return;
    }
    if (const auto* fc = dynamic_cast<const ForCStyleStmt*>(s)) {
        collectAstDeclarationsFromStmt(fc->init.get(), file);
        collectAstDeclarationsFromExpr(fc->condition.get(), file);
        collectAstDeclarationsFromExpr(fc->update.get(), file);
        collectAstDeclarationsFromStmt(fc->body.get(), file);
        return;
    }
    if (const auto* w = dynamic_cast<const WhileStmt*>(s)) {
        collectAstDeclarationsFromExpr(w->condition.get(), file);
        collectAstDeclarationsFromStmt(w->body.get(), file);
        return;
    }
    if (const auto* r = dynamic_cast<const RepeatStmt*>(s)) {
        collectAstDeclarationsFromExpr(r->count.get(), file);
        collectAstDeclarationsFromStmt(r->body.get(), file);
        return;
    }
    if (const auto* rw = dynamic_cast<const RepeatWhileStmt*>(s)) {
        collectAstDeclarationsFromExpr(rw->condition.get(), file);
        collectAstDeclarationsFromStmt(rw->body.get(), file);
        return;
    }
    if (const auto* t = dynamic_cast<const TryStmt*>(s)) {
        collectAstDeclarationsFromStmt(t->tryBlock.get(), file);
        if (!t->catchVar.empty()) file.declared.insert(t->catchVar);
        collectAstDeclarationsFromStmt(t->catchBlock.get(), file);
        collectAstDeclarationsFromStmt(t->elseBlock.get(), file);
        collectAstDeclarationsFromStmt(t->finallyBlock.get(), file);
        return;
    }
    if (const auto* m = dynamic_cast<const MatchStmt*>(s)) {
        collectAstDeclarationsFromExpr(m->value.get(), file);
        for (const auto& c : m->cases) {
            addMatchPatternBindingIds(c.pattern.get(), file);
            collectAstDeclarationsFromExpr(c.guard.get(), file);
            collectAstDeclarationsFromStmt(c.body.get(), file);
        }
        return;
    }
    if (const auto* b = dynamic_cast<const BlockStmt*>(s)) {
        for (const auto& st : b->statements) collectAstDeclarationsFromStmt(st.get(), file);
        return;
    }
    if (const auto* i = dynamic_cast<const IfStmt*>(s)) {
        collectAstDeclarationsFromExpr(i->condition.get(), file);
        collectAstDeclarationsFromStmt(i->thenBranch.get(), file);
        collectAstDeclarationsFromStmt(i->elseBranch.get(), file);
        for (const auto& eb : i->elifBranches) {
            collectAstDeclarationsFromExpr(eb.first.get(), file);
            collectAstDeclarationsFromStmt(eb.second.get(), file);
        }
        return;
    }
    if (const auto* es = dynamic_cast<const ExprStmt*>(s)) {
        collectAstDeclarationsFromExpr(es->expr.get(), file);
        return;
    }
    if (const auto* ret = dynamic_cast<const ReturnStmt*>(s)) {
        for (const auto& v : ret->values) collectAstDeclarationsFromExpr(v.get(), file);
        return;
    }
    if (const auto* th = dynamic_cast<const ThrowStmt*>(s)) {
        collectAstDeclarationsFromExpr(th->value.get(), file);
        return;
    }
    if (const auto* as = dynamic_cast<const AssertStmt*>(s)) {
        collectAstDeclarationsFromExpr(as->condition.get(), file);
        collectAstDeclarationsFromExpr(as->message.get(), file);
        return;
    }
    if (const auto* df = dynamic_cast<const DeferStmt*>(s)) {
        collectAstDeclarationsFromExpr(df->expr.get(), file);
        return;
    }
    if (const auto* prog = dynamic_cast<const Program*>(s)) {
        for (const auto& st : prog->statements) collectAstDeclarationsFromStmt(st.get(), file);
        return;
    }
}

static void collectAstDeclarationsFromExpr(const Expr* e, ParsedFileInfo& file) {
    if (!e) return;
    if (const auto* lam = dynamic_cast<const LambdaExpr*>(e)) {
        for (const auto& p : lam->params) file.declared.insert(p);
        collectAstDeclarationsFromStmt(lam->body.get(), file);
        return;
    }
    if (const auto* ac = dynamic_cast<const ArrayComprehensionExpr*>(e)) {
        file.declared.insert(ac->varName);
        collectAstDeclarationsFromExpr(ac->iterExpr.get(), file);
        collectAstDeclarationsFromExpr(ac->bodyExpr.get(), file);
        collectAstDeclarationsFromExpr(ac->filterExpr.get(), file);
        return;
    }
    if (const auto* mc = dynamic_cast<const MapComprehensionExpr*>(e)) {
        file.declared.insert(mc->varName);
        collectAstDeclarationsFromExpr(mc->keyExpr.get(), file);
        collectAstDeclarationsFromExpr(mc->valExpr.get(), file);
        collectAstDeclarationsFromExpr(mc->iterExpr.get(), file);
        collectAstDeclarationsFromExpr(mc->filterExpr.get(), file);
        return;
    }
    if (const auto* b = dynamic_cast<const BinaryExpr*>(e)) {
        collectAstDeclarationsFromExpr(b->left.get(), file);
        collectAstDeclarationsFromExpr(b->right.get(), file);
        return;
    }
    if (const auto* u = dynamic_cast<const UnaryExpr*>(e)) {
        collectAstDeclarationsFromExpr(u->operand.get(), file);
        return;
    }
    if (const auto* c = dynamic_cast<const CallExpr*>(e)) {
        collectAstDeclarationsFromExpr(c->callee.get(), file);
        for (const auto& a : c->args) collectAstDeclarationsFromExpr(a.expr.get(), file);
        return;
    }
    if (const auto* ix = dynamic_cast<const IndexExpr*>(e)) {
        collectAstDeclarationsFromExpr(ix->object.get(), file);
        collectAstDeclarationsFromExpr(ix->index.get(), file);
        return;
    }
    if (const auto* oix = dynamic_cast<const OptionalIndexExpr*>(e)) {
        collectAstDeclarationsFromExpr(oix->object.get(), file);
        collectAstDeclarationsFromExpr(oix->index.get(), file);
        return;
    }
    if (const auto* me = dynamic_cast<const MemberExpr*>(e)) {
        collectAstDeclarationsFromExpr(me->object.get(), file);
        return;
    }
    if (const auto* as = dynamic_cast<const AssignExpr*>(e)) {
        collectAstDeclarationsFromExpr(as->target.get(), file);
        collectAstDeclarationsFromExpr(as->value.get(), file);
        return;
    }
    if (const auto* t = dynamic_cast<const TernaryExpr*>(e)) {
        collectAstDeclarationsFromExpr(t->condition.get(), file);
        collectAstDeclarationsFromExpr(t->thenExpr.get(), file);
        collectAstDeclarationsFromExpr(t->elseExpr.get(), file);
        return;
    }
    if (const auto* r = dynamic_cast<const RangeExpr*>(e)) {
        collectAstDeclarationsFromExpr(r->start.get(), file);
        collectAstDeclarationsFromExpr(r->end.get(), file);
        collectAstDeclarationsFromExpr(r->step.get(), file);
        return;
    }
    if (const auto* d = dynamic_cast<const DurationExpr*>(e)) {
        collectAstDeclarationsFromExpr(d->amount.get(), file);
        return;
    }
    if (const auto* co = dynamic_cast<const CoalesceExpr*>(e)) {
        collectAstDeclarationsFromExpr(co->left.get(), file);
        collectAstDeclarationsFromExpr(co->right.get(), file);
        return;
    }
    if (const auto* pl = dynamic_cast<const PipelineExpr*>(e)) {
        collectAstDeclarationsFromExpr(pl->left.get(), file);
        collectAstDeclarationsFromExpr(pl->right.get(), file);
        return;
    }
    if (const auto* sp = dynamic_cast<const SpreadExpr*>(e)) {
        collectAstDeclarationsFromExpr(sp->target.get(), file);
        return;
    }
    if (const auto* al = dynamic_cast<const ArrayLiteral*>(e)) {
        for (const auto& el : al->elements) collectAstDeclarationsFromExpr(el.get(), file);
        return;
    }
    if (const auto* ml = dynamic_cast<const MapLiteral*>(e)) {
        for (const auto& pr : ml->entries) {
            collectAstDeclarationsFromExpr(pr.first.get(), file);
            collectAstDeclarationsFromExpr(pr.second.get(), file);
        }
        return;
    }
    if (const auto* fs = dynamic_cast<const FStringExpr*>(e)) {
        for (const auto& part : fs->parts) {
            if (std::holds_alternative<ExprPtr>(part)) {
                const auto& ep = std::get<ExprPtr>(part);
                if (ep) collectAstDeclarationsFromExpr(ep.get(), file);
            }
        }
        return;
    }
    if (const auto* oc = dynamic_cast<const OptionalChainExpr*>(e)) {
        collectAstDeclarationsFromExpr(oc->object.get(), file);
        return;
    }
    if (const auto* sl = dynamic_cast<const SliceExpr*>(e)) {
        collectAstDeclarationsFromExpr(sl->object.get(), file);
        collectAstDeclarationsFromExpr(sl->start.get(), file);
        collectAstDeclarationsFromExpr(sl->end.get(), file);
        collectAstDeclarationsFromExpr(sl->step.get(), file);
        return;
    }
}

static fs::path tryResolveImport(const fs::path& importer, const std::string& raw, const fs::path& root);
static void mergeDeclaredFromIncludedFiles(const fs::path& includedSplPath, ParsedFileInfo& target,
    const fs::path& projectRoot, std::unordered_set<std::string>& visiting);

static void collectSyntaxAndSymbols(ParsedFileInfo& file, AnalyzerReport& report, const fs::path& projectRoot) {
    file.parseOk = true;
    std::regex importRe(R"re(\b(?:import|include)\s*(?:\(\s*)?"([^"]+)"(?:\s*\))?)re");
    for (std::sregex_iterator it(file.source.begin(), file.source.end(), importRe), end; it != end; ++it) {
        size_t pos = static_cast<size_t>((*it).position());
        int line = lineFromOffset(file.lineOffsets, pos);
        file.imports.push_back({(*it)[1].str(), line});
    }

    try {
        Lexer lexer(file.source);
        std::vector<Token> toks = lexer.tokenize();
        std::unordered_set<std::string> ignoreKeywords = keywordLikeIdentifiers();

        for (size_t i = 0; i < toks.size(); ++i) {
            const Token& t = toks[i];
            if (t.type != TokenType::IDENTIFIER) continue;
            bool declContext = false;
            if (i > 0) {
                TokenType p = toks[i - 1].type;
                bool prevIsConst = lowerCopy(toks[i - 1].lexeme) == "const";
                declContext = p == TokenType::LET || prevIsConst || p == TokenType::VAR ||
                              p == TokenType::DEF || p == TokenType::CLASS || p == TokenType::IMPORT ||
                              p == TokenType::CATCH || p == TokenType::FOR;
                if (p == TokenType::DOT || p == TokenType::QUESTION_DOT) continue;
            }
            if (declContext) {
                file.declared.insert(t.lexeme);
                file.varDecls.push_back({t.lexeme, t.line});
            } else {
                if (!ignoreKeywords.count(t.lexeme)) file.used.insert(t.lexeme);
            }
        }

        Parser parser(std::move(toks));
        std::unique_ptr<Program> program = parser.parse();

        // register bindings the token pass misses (nested params, lambdas, for-in, catch, match, imports, etc.).
        for (const auto& stmt : program->statements) collectAstDeclarationsFromStmt(stmt.get(), file);

        // include merges source; pull in declarations from resolved include chain.
        std::regex includeLineRe(R"re(\binclude\s*"([^"]+)")re");
        std::unordered_set<std::string> includeVisiting;
        for (std::sregex_iterator it(file.source.begin(), file.source.end(), includeLineRe), end; it != end; ++it) {
            const std::string raw = (*it)[1].str();
            fs::path resolved = tryResolveImport(fs::path(file.path), raw, projectRoot);
            if (resolved.empty()) continue;
            mergeDeclaredFromIncludedFiles(resolved, file, projectRoot, includeVisiting);
        }

        // control flow: missing return on typed non-void functions.
        std::function<bool(const Stmt*)> containsReturn = [&](const Stmt* s) -> bool {
            if (!s) return false;
            if (dynamic_cast<const ReturnStmt*>(s) != nullptr) return true;
            if (const auto* b = dynamic_cast<const BlockStmt*>(s)) {
                for (const auto& c : b->statements) if (containsReturn(c.get())) return true;
            } else if (const auto* i = dynamic_cast<const IfStmt*>(s)) {
                if (containsReturn(i->thenBranch.get()) || containsReturn(i->elseBranch.get())) return true;
                for (const auto& p : i->elifBranches) if (containsReturn(p.second.get())) return true;
            } else if (const auto* w = dynamic_cast<const WhileStmt*>(s)) {
                if (containsReturn(w->body.get())) return true;
            } else if (const auto* f = dynamic_cast<const ForRangeStmt*>(s)) {
                if (containsReturn(f->body.get())) return true;
            } else if (const auto* fi = dynamic_cast<const ForInStmt*>(s)) {
                if (containsReturn(fi->body.get())) return true;
            } else if (const auto* tr = dynamic_cast<const TryStmt*>(s)) {
                if (containsReturn(tr->tryBlock.get()) || containsReturn(tr->catchBlock.get()) ||
                    containsReturn(tr->elseBlock.get()) || containsReturn(tr->finallyBlock.get())) return true;
            }
            return false;
        };

        for (const auto& stmt : program->statements) {
            const auto* fn = dynamic_cast<const FunctionDeclStmt*>(stmt.get());
            if (!fn) continue;
            if (fn->hasReturnType && !fn->returnType.empty() && lowerCopy(fn->returnType) != "void") {
                if (!containsReturn(fn->body.get())) {
                    AnalyzerIssue issue;
                    issue.severity = IssueSeverity::Critical;
                    issue.file = file.path;
                    issue.line = fn->line > 0 ? fn->line : 1;
                    issue.column = fn->column > 0 ? fn->column : 1;
                    issue.type = "control_flow.missing_return";
                    issue.message = "Function '" + fn->name + "' has non-void return type but no return statement.";
                    issue.fix = "Insert 'return null' at end of function body.";
                    issue.autoFixable = true;
                    addIssue(report, std::move(issue));
                }
            }
        }
    } catch (const LexerError& e) {
        file.parseOk = false;
        AnalyzerIssue issue;
        issue.severity = IssueSeverity::Critical;
        issue.file = file.path;
        issue.line = e.line;
        issue.column = e.column;
        issue.type = "syntax.lexer";
        issue.message = e.what();
        issue.fix = lexerCompileErrorHint();
        addIssue(report, std::move(issue));
    } catch (const ParserError& e) {
        file.parseOk = false;
        AnalyzerIssue issue;
        issue.severity = IssueSeverity::Critical;
        issue.file = file.path;
        issue.line = e.line;
        issue.column = e.column;
        issue.type = "syntax.parser";
        issue.message = e.what();
        issue.fix = parserCompileErrorHint(e.what());
        addIssue(report, std::move(issue));
    }
}

static fs::path tryResolveImport(const fs::path& importer, const std::string& raw, const fs::path& root) {
    fs::path p(raw);
    if (p.extension().empty()) p += ".kn";
    std::vector<fs::path> probes = {importer.parent_path() / p, root / p};
    for (const auto& q : probes) {
        std::error_code ec;
        fs::path can = fs::weakly_canonical(q, ec);
        if (!ec && fs::exists(can, ec)) return can;
    }
    return {};
}

/* * canonical path string for cycle detection (include graphs).*/
static std::string canonicalPathKey(const fs::path& p) {
    std::error_code ec;
    fs::path c = fs::weakly_canonical(p, ec);
    if (ec) c = p;
    return c.generic_string();
}

/* *
 * include "x.kn" pastes source; merge top-level bindings from resolved files (transitively)
 * so semantic.possible_undefined_symbol does not false-positive on included defs.
 */
static void mergeDeclaredFromIncludedFiles(const fs::path& includedSplPath, ParsedFileInfo& target,
    const fs::path& projectRoot, std::unordered_set<std::string>& visiting) {
    const std::string key = canonicalPathKey(includedSplPath);
    if (visiting.count(key)) return;
    visiting.insert(key);

    std::string text = readTextFile(key);
    if (text.empty()) return;

    try {
        Lexer lexer(text);
        std::vector<Token> toks = lexer.tokenize();
        Parser parser(std::move(toks));
        std::unique_ptr<Program> program = parser.parse();
        for (const auto& st : program->statements) collectAstDeclarationsFromStmt(st.get(), target);

        std::regex includeRe(R"re(\binclude\s*"([^"]+)")re");
        for (std::sregex_iterator it(text.begin(), text.end(), includeRe), end; it != end; ++it) {
            const std::string raw = (*it)[1].str();
            fs::path next = tryResolveImport(fs::path(key), raw, projectRoot);
            if (next.empty()) continue;
            mergeDeclaredFromIncludedFiles(next, target, projectRoot, visiting);
        }
    } catch (const LexerError&) {
    } catch (const ParserError&) {
    }
}

static std::string relImportPath(const fs::path& fromFile, const fs::path& targetFile) {
    std::error_code ec;
    fs::path rel = fs::relative(targetFile, fromFile.parent_path(), ec);
    if (ec) rel = targetFile.filename();
    std::string s = rel.generic_string();
    if (s.empty()) s = targetFile.filename().generic_string();
    if (!s.empty() && s[0] != '.') s = "./" + s;
    return s;
}

static bool lineLooksLikeStandaloneDecl(const std::string& line, const std::string& name) {
    std::regex re("^\\s*(let|var|const)\\s+" + name + "\\b[^,]*$");
    return std::regex_search(line, re);
}

/* * true if the first if (...) on the line contains a bare = after masking ==, !=, <=, >=.*/
static bool conditionLineHasBareAssignment(const std::string& line) {
    std::string t = trim(line);
    if (t.rfind("if", 0) != 0) return false;
    size_t lparen = t.find('(');
    if (lparen == std::string::npos) return false;
    int depth = 0;
    size_t rparen = std::string::npos;
    for (size_t i = lparen; i < t.size(); ++i) {
        if (t[i] == '(') ++depth;
        else if (t[i] == ')') {
            --depth;
            if (depth == 0) {
                rparen = i;
                break;
            }
        }
    }
    if (rparen == std::string::npos || rparen <= lparen + 1) return false;
    std::string cond = t.substr(lparen + 1, rparen - lparen - 1);
    static const char* masks[] = {"==", "!=", "<=", ">=", "=>"};
    for (const char* m : masks) {
        std::string ms(m);
        size_t p = 0;
        while ((p = cond.find(ms, p)) != std::string::npos) {
            for (size_t k = 0; k < ms.size(); ++k) cond[p + k] = ' ';
            p += ms.size();
        }
    }
    return cond.find('=') != std::string::npos;
}

static int findFunctionClosingBraceLine(const std::vector<std::string>& lines, int startLine) {
    int depth = 0;
    bool started = false;
    for (int i = std::max(1, startLine); i <= static_cast<int>(lines.size()); ++i) {
        const std::string& ln = lines[static_cast<size_t>(i - 1)];
        for (char c : ln) {
            if (c == '{') {
                ++depth;
                started = true;
            } else if (c == '}') {
                --depth;
                if (started && depth == 0) return i;
            }
        }
    }
    return -1;
}

static std::string nowStamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(ms);
}

static bool ensureParentDir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    return !ec;
}

static bool backupFile(const std::string& root, const std::string& backupRoot, const std::string& file) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(file), fs::path(root), ec);
    if (ec) rel = fs::path(file).filename();
    fs::path out = fs::path(backupRoot) / rel;
    if (!ensureParentDir(out)) return false;
    fs::copy_file(fs::path(file), out, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

static std::string buildUpdatedText(const std::vector<std::string>& lines, const FileEditPlan& plan) {
    std::ostringstream out;
    for (int lineNo = 1; lineNo <= static_cast<int>(lines.size()); ++lineNo) {
        for (const auto& ins : plan.insertBeforeLine) {
            if (ins.first == lineNo) out << ins.second << "\n";
        }
        if (plan.removeLines.find(lineNo) != plan.removeLines.end()) continue;
        out << lines[static_cast<size_t>(lineNo - 1)];
        if (lineNo < static_cast<int>(lines.size())) out << "\n";
    }
    return out.str();
}

} // namespace

std::string severityToString(IssueSeverity s) {
    switch (s) {
        case IssueSeverity::Critical: return "CRITICAL";
        case IssueSeverity::Warning: return "WARNING";
        default: return "INFO";
    }
}

AnalyzerReport analyzeProjectAndMaybeFix(const AnalyzerOptions& options) {
    AnalyzerReport report;
    std::error_code ec;
    fs::path root = fs::weakly_canonical(fs::path(options.projectRoot.empty() ? "." : options.projectRoot), ec);
    if (ec) root = fs::absolute(fs::path(options.projectRoot.empty() ? "." : options.projectRoot));
    report.projectRoot = root.string();
    report.kernFiles = collectProjectSplFiles(root);
    report.configFiles = collectSplConfigFiles(root);

    std::unordered_map<std::string, ParsedFileInfo> files;
    for (const auto& path : report.kernFiles) {
        ParsedFileInfo pf;
        pf.path = path;
        pf.source = readTextFile(path);
        pf.lines = splitLines(pf.source);
        pf.lineOffsets = computeLineOffsets(pf.source);
        collectSyntaxAndSymbols(pf, report, root);
        files[path] = std::move(pf);
    }

    for (const auto& cfgPath : report.configFiles) {
        SplConfig cfg;
        std::string err;
        if (!loadSplConfig(cfgPath, cfg, err)) {
            AnalyzerIssue issue;
            issue.severity = IssueSeverity::Critical;
            issue.file = cfgPath;
            issue.line = 1;
            issue.column = 1;
            issue.type = "config.parse";
            issue.message = err;
            issue.fix = "Fix invalid kernconfig.json format.";
            addIssue(report, std::move(issue));
        }
    }

    // dependency analysis.
    std::unordered_map<std::string, std::vector<std::string>> depGraph;
    std::unordered_map<std::string, int> seenImportCount;
    for (const auto& kv : files) {
        const ParsedFileInfo& pf = kv.second;
        std::unordered_set<std::string> localDupCheck;
        for (const auto& imp : pf.imports) {
            std::string name = imp.first;
            if (localDupCheck.count(name)) {
                AnalyzerIssue issue;
                issue.severity = IssueSeverity::Warning;
                issue.file = pf.path;
                issue.line = imp.second;
                issue.column = 1;
                issue.type = "dependency.duplicate_import";
                issue.message = "Duplicate import '" + name + "'.";
                issue.fix = "Remove duplicate import line.";
                issue.autoFixable = true;
                addIssue(report, std::move(issue));
            } else {
                localDupCheck.insert(name);
            }
            fs::path depPath = tryResolveImport(fs::path(pf.path), name, root);
            if (depPath.empty() && isIntentionalMissingImportFixture(name)) {
                ++seenImportCount[name];
            } else if (depPath.empty() && isVirtualResolvedImport(name)) {
                // runtime builtin/stdlib — no project file required.
                ++seenImportCount[name];
            } else if (depPath.empty()) {
                AnalyzerIssue issue;
                issue.severity = IssueSeverity::Critical;
                issue.file = pf.path;
                issue.line = imp.second;
                issue.column = 1;
                issue.type = "dependency.missing_import";
                issue.message = "Missing import '" + name + "'.";
                issue.fix = "Fix import path or add the missing module.";
                addIssue(report, std::move(issue));
            } else {
                depGraph[pf.path].push_back(depPath.generic_string());
                ++seenImportCount[name];
            }
        }
    }

    // cycle detection.
    enum class Visit { None, Visiting, Done };
    std::unordered_map<std::string, Visit> vis;
    std::vector<std::string> stack;
    std::function<void(const std::string&)> dfs = [&](const std::string& node) {
        vis[node] = Visit::Visiting;
        stack.push_back(node);
        for (const auto& nxt : depGraph[node]) {
            if (vis[nxt] == Visit::Visiting) {
                AnalyzerIssue issue;
                issue.severity = IssueSeverity::Critical;
                issue.file = node;
                issue.line = 1;
                issue.column = 1;
                issue.type = "dependency.circular";
                issue.message = "Circular dependency detected involving '" + nxt + "'.";
                issue.fix = "Refactor imports to break cycle.";
                addIssue(report, std::move(issue));
                continue;
            }
            if (vis[nxt] == Visit::None) dfs(nxt);
        }
        stack.pop_back();
        vis[node] = Visit::Done;
    };
    for (const auto& kv : depGraph) if (vis[kv.first] == Visit::None) dfs(kv.first);

    std::unordered_set<std::string> builtins = builtinNames();
    std::unordered_set<std::string> projectStems;
    std::unordered_map<std::string, std::string> stemToFile;
    for (const auto& path : report.kernFiles) {
        std::string stem = fs::path(path).stem().string();
        projectStems.insert(stem);
        if (!stemToFile.count(stem)) stemToFile[stem] = path;
    }

    // semantic + control/perf/runtime heuristics.
    std::unordered_map<std::string, FileEditPlan> editPlans;
    for (auto& kv : files) {
        ParsedFileInfo& pf = kv.second;
        FileEditPlan plan;

        // undefined symbol heuristics.
        for (const auto& sym : pf.used) {
            if (pf.declared.count(sym) || builtins.count(sym)) continue;
            AnalyzerIssue issue;
            issue.severity = IssueSeverity::Warning;
            issue.file = pf.path;
            issue.line = 1;
            issue.column = 1;
            issue.type = "semantic.possible_undefined_symbol";
            issue.message = "Potential undefined symbol '" + sym + "'.";
            issue.fix = "Declare it or import a module that defines it.";
            issue.autoFixable = false;
            issue.uncertain = true;
            addIssue(report, std::move(issue));

            if (projectStems.count(sym) && stemToFile.count(sym)) {
                std::string candidate = relImportPath(fs::path(pf.path), fs::path(stemToFile[sym]));
                std::string stmt = "import \"" + candidate + "\"";
                bool already = false;
                for (const auto& imp : pf.imports) {
                    if (imp.first == candidate || imp.first == sym || imp.first.find(sym) != std::string::npos) {
                        already = true;
                        break;
                    }
                }
                if (!already) {
                    plan.importsToAdd.push_back(stmt);
                    AnalyzerIssue addImp;
                    addImp.severity = IssueSeverity::Warning;
                    addImp.file = pf.path;
                    addImp.line = 1;
                    addImp.column = 1;
                    addImp.type = "autofix.add_import";
                    addImp.message = "Can add missing import for symbol '" + sym + "'.";
                    addImp.fix = stmt;
                    addImp.autoFixable = true;
                    addIssue(report, std::move(addImp));
                }
            }
        }

        // duplicate imports removal.
        std::unordered_set<std::string> seenImport;
        for (const auto& imp : pf.imports) {
            if (seenImport.count(imp.first)) {
                plan.removeLines.insert(imp.second);
                plan.reasons.push_back("remove duplicate import");
            } else {
                seenImport.insert(imp.first);
            }
        }

        // unused variables (names starting with '_' are intentionally ignorable).
        for (const auto& vd : pf.varDecls) {
            if (!vd.first.empty() && vd.first[0] == '_') continue;
            if (!pf.used.count(vd.first)) {
                if (vd.second >= 1 && vd.second <= static_cast<int>(pf.lines.size()) &&
                    lineLooksLikeStandaloneDecl(pf.lines[static_cast<size_t>(vd.second - 1)], vd.first)) {
                    plan.removeLines.insert(vd.second);
                    AnalyzerIssue issue;
                    issue.severity = IssueSeverity::Warning;
                    issue.file = pf.path;
                    issue.line = vd.second;
                    issue.column = 1;
                    issue.type = "semantic.unused_variable";
                    issue.message = "Unused variable '" + vd.first + "'.";
                    issue.fix = "Remove declaration line.";
                    issue.autoFixable = true;
                    addIssue(report, std::move(issue));
                }
            }
        }

        // unreachable code and infinite loops (line heuristics).
        for (size_t i = 0; i < pf.lines.size(); ++i) {
            std::string t = trim(pf.lines[i]);
            if (t.rfind("while (true)", 0) == 0 || t.rfind("while(true)", 0) == 0) {
                bool hasBreak = false;
                int depth = 0;
                for (size_t j = i; j < pf.lines.size(); ++j) {
                    for (char c : pf.lines[j]) {
                        if (c == '{') ++depth;
                        if (c == '}') --depth;
                    }
                    if (pf.lines[j].find("break") != std::string::npos) hasBreak = true;
                    if (j > i && depth <= 0) break;
                }
                if (!hasBreak) {
                    AnalyzerIssue issue;
                    issue.severity = IssueSeverity::Warning;
                    issue.file = pf.path;
                    issue.line = static_cast<int>(i + 1);
                    issue.column = 1;
                    issue.type = "control_flow.possible_infinite_loop";
                    issue.message = "Possible infinite loop without break.";
                    issue.fix = "Add break condition.";
                    addIssue(report, std::move(issue));
                }
            }

            if (t.rfind("return", 0) == 0 || t.rfind("break", 0) == 0 || t.rfind("continue", 0) == 0 || t.rfind("throw", 0) == 0) {
                for (size_t j = i + 1; j < pf.lines.size(); ++j) {
                    std::string n = trim(pf.lines[j]);
                    if (n.empty() || n.rfind("//", 0) == 0) continue;
                    if (!n.empty() && n[0] != '}') {
                        AnalyzerIssue issue;
                        issue.severity = IssueSeverity::Info;
                        issue.file = pf.path;
                        issue.line = static_cast<int>(j + 1);
                        issue.column = 1;
                        issue.type = "control_flow.unreachable_code";
                        issue.message = "Potential unreachable code.";
                        issue.fix = "Review control-flow and remove unreachable lines.";
                        addIssue(report, std::move(issue));
                    }
                    break;
                }
            }

            if (conditionLineHasBareAssignment(pf.lines[i])) {
                AnalyzerIssue issue;
                issue.severity = IssueSeverity::Info;
                issue.file = pf.path;
                issue.line = static_cast<int>(i + 1);
                issue.column = 1;
                issue.type = "logic.assignment_in_condition";
                issue.message = "Assignment used inside condition; verify this is intentional.";
                issue.fix = "Use '==' for comparison or refactor.";
                issue.uncertain = true;
                addIssue(report, std::move(issue));
            }
        }

        // runtime analysis heuristic: null access.
        std::unordered_set<std::string> nullVars;
        std::regex assignNullRe(R"re(\b(let|var|const)?\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*null\b)re");
        for (size_t i = 0; i < pf.lines.size(); ++i) {
            std::smatch m;
            if (std::regex_search(pf.lines[i], m, assignNullRe) && m.size() >= 3) nullVars.insert(m[2].str());
            for (const auto& nv : nullVars) {
                if (pf.lines[i].find(nv + ".") != std::string::npos || pf.lines[i].find(nv + "[") != std::string::npos) {
                    AnalyzerIssue issue;
                    issue.severity = IssueSeverity::Warning;
                    issue.file = pf.path;
                    issue.line = static_cast<int>(i + 1);
                    issue.column = 1;
                    issue.type = "runtime.null_access";
                    issue.message = "Potential null access on '" + nv + "'.";
                    issue.fix = "Guard with null check before access.";
                    addIssue(report, std::move(issue));
                }
            }
        }

        // performance heuristics.
        int nestedLoopWarnings = 0;
        for (size_t i = 0; i + 1 < pf.lines.size(); ++i) {
            std::string a = trim(pf.lines[i]);
            std::string b = trim(pf.lines[i + 1]);
            bool loopA = a.rfind("for ", 0) == 0 || a.rfind("while ", 0) == 0;
            bool loopB = b.rfind("for ", 0) == 0 || b.rfind("while ", 0) == 0;
            if (loopA && loopB && nestedLoopWarnings < 3) {
                AnalyzerIssue issue;
                issue.severity = IssueSeverity::Info;
                issue.file = pf.path;
                issue.line = static_cast<int>(i + 1);
                issue.column = 1;
                issue.type = "perf.nested_loops";
                issue.message = "Nested loops may be slow on large inputs.";
                issue.fix = "Consider hoisting work, caching, or reducing complexity.";
                addIssue(report, std::move(issue));
                ++nestedLoopWarnings;
            }
        }

        // missing return autofix insertion for typed functions found above.
        for (const auto& issue : report.issues) {
            if (issue.file != pf.path || issue.type != "control_flow.missing_return" || !issue.autoFixable) continue;
            int closeLine = findFunctionClosingBraceLine(pf.lines, issue.line);
            if (closeLine > 0) {
                plan.insertBeforeLine.push_back({closeLine, "    return null"});
                plan.reasons.push_back("insert missing return");
            }
        }

        if (!plan.importsToAdd.empty()) {
            // place imports at top, after existing imports.
            int insertAt = 1;
            for (const auto& imp : pf.imports) insertAt = std::max(insertAt, imp.second + 1);
            std::sort(plan.importsToAdd.begin(), plan.importsToAdd.end());
            plan.importsToAdd.erase(std::unique(plan.importsToAdd.begin(), plan.importsToAdd.end()), plan.importsToAdd.end());
            for (const auto& stmt : plan.importsToAdd) {
                plan.insertBeforeLine.push_back({insertAt, stmt});
            }
            plan.reasons.push_back("add missing imports");
        }

        if (!plan.reasons.empty()) editPlans[pf.path] = std::move(plan);
    }

    if (options.applyFixes && !editPlans.empty()) {
        if (!options.dryRun) {
            fs::path backupRoot = fs::path(report.projectRoot) / ".kern-fix-backups" / nowStamp();
            std::error_code mkec;
            fs::create_directories(backupRoot, mkec);
            if (!mkec) report.backupRoot = backupRoot.string();
        }

        for (const auto& kv : editPlans) {
            const std::string& file = kv.first;
            const FileEditPlan& plan = kv.second;
            auto it = files.find(file);
            if (it == files.end()) continue;
            const ParsedFileInfo& pf = it->second;

            if (!options.dryRun && !report.backupRoot.empty()) {
                if (!backupFile(report.projectRoot, report.backupRoot, file)) {
                    AnalyzerIssue issue;
                    issue.severity = IssueSeverity::Critical;
                    issue.file = file;
                    issue.line = 1;
                    issue.column = 1;
                    issue.type = "fix.backup_failed";
                    issue.message = "Failed to create backup before applying fixes.";
                    issue.fix = "Check write permissions and disk space.";
                    addIssue(report, std::move(issue));
                    continue;
                }
            }

            std::string updated = buildUpdatedText(pf.lines, plan);
            if (!options.dryRun) {
                if (!writeTextFile(file, updated)) {
                    AnalyzerIssue issue;
                    issue.severity = IssueSeverity::Critical;
                    issue.file = file;
                    issue.line = 1;
                    issue.column = 1;
                    issue.type = "fix.write_failed";
                    issue.message = "Failed to write auto-fixed file.";
                    issue.fix = "Check file permissions.";
                    addIssue(report, std::move(issue));
                    continue;
                }
            }
            AnalyzerFixChange c;
            c.file = file;
            c.reason = plan.reasons.empty() ? "auto-fix applied" : plan.reasons.front();
            if (options.dryRun) report.pendingReviewChanges.push_back(std::move(c));
            else report.appliedChanges.push_back(std::move(c));
        }
    }

    return report;
}

bool undoFixesFromBackup(const std::string& backupPath, std::string& error) {
    std::error_code ec;
    fs::path backup = fs::path(backupPath);
    if (!backup.is_absolute()) backup = fs::current_path() / backup;
    backup = fs::weakly_canonical(backup, ec);
    if (ec || !fs::exists(backup)) {
        error = "backup path not found: " + backupPath;
        return false;
    }
    fs::path backupRoot = backup;
    fs::path projectRoot = backupRoot.parent_path().parent_path();
    if (projectRoot.empty() || !fs::exists(projectRoot)) {
        error = "could not infer project root from backup path";
        return false;
    }

    for (fs::recursive_directory_iterator it(backupRoot, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file()) continue;
        fs::path rel = fs::relative(it->path(), backupRoot, ec);
        if (ec) continue;
        fs::path dst = projectRoot / rel;
        fs::create_directories(dst.parent_path(), ec);
        if (ec) {
            error = "failed creating destination dirs for: " + dst.string();
            return false;
        }
        fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error = "failed restoring file: " + dst.string();
            return false;
        }
    }
    return true;
}

std::string analyzerReportToJson(const AnalyzerReport& report) {
    std::ostringstream out;
    out << "{";
    out << "\"projectRoot\":\"" << jsonEscapeLocal(report.projectRoot) << "\",";
    out << "\"critical\":" << report.criticalCount << ",";
    out << "\"warning\":" << report.warningCount << ",";
    out << "\"info\":" << report.infoCount << ",";
    out << "\"backupRoot\":\"" << jsonEscapeLocal(report.backupRoot) << "\",";
    out << "\"kernFiles\":[";
    for (size_t i = 0; i < report.kernFiles.size(); ++i) {
        if (i) out << ",";
        out << "\"" << jsonEscapeLocal(report.kernFiles[i]) << "\"";
    }
    out << "],";
    out << "\"configFiles\":[";
    for (size_t i = 0; i < report.configFiles.size(); ++i) {
        if (i) out << ",";
        out << "\"" << jsonEscapeLocal(report.configFiles[i]) << "\"";
    }
    out << "],";
    out << "\"issues\":[";
    for (size_t i = 0; i < report.issues.size(); ++i) {
        const auto& it = report.issues[i];
        if (i) out << ",";
        out << "{"
            << "\"severity\":\"" << severityToString(it.severity) << "\","
            << "\"file\":\"" << jsonEscapeLocal(it.file) << "\","
            << "\"line\":" << it.line << ","
            << "\"column\":" << it.column << ","
            << "\"type\":\"" << jsonEscapeLocal(it.type) << "\","
            << "\"message\":\"" << jsonEscapeLocal(it.message) << "\","
            << "\"fix\":\"" << jsonEscapeLocal(it.fix) << "\","
            << "\"autoFixable\":" << (it.autoFixable ? "true" : "false") << ","
            << "\"uncertain\":" << (it.uncertain ? "true" : "false")
            << "}";
    }
    out << "],";
    out << "\"appliedChanges\":[";
    for (size_t i = 0; i < report.appliedChanges.size(); ++i) {
        if (i) out << ",";
        out << "{"
            << "\"file\":\"" << jsonEscapeLocal(report.appliedChanges[i].file) << "\","
            << "\"reason\":\"" << jsonEscapeLocal(report.appliedChanges[i].reason) << "\""
            << "}";
    }
    out << "],";
    out << "\"pendingReviewChanges\":[";
    for (size_t i = 0; i < report.pendingReviewChanges.size(); ++i) {
        if (i) out << ",";
        out << "{"
            << "\"file\":\"" << jsonEscapeLocal(report.pendingReviewChanges[i].file) << "\","
            << "\"reason\":\"" << jsonEscapeLocal(report.pendingReviewChanges[i].reason) << "\""
            << "}";
    }
    out << "]";
    out << "}";
    return out.str();
}

} // namespace kern
