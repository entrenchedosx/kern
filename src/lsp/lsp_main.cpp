#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include "compiler/semantic.hpp"
#include "compiler/ast.hpp"
#include "platform/env_compat.hpp"
#include "errors.hpp"
#include "vm/builtins.hpp"
#include "diagnostics/source_span.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#ifdef FLOAT
#undef FLOAT
#endif
#ifdef AND
#undef AND
#endif
#ifdef OR
#undef OR
#endif
#ifdef NOT
#undef NOT
#endif

namespace kern {

namespace {

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Object, Array };
    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::map<std::string, JsonValue> objectValue;
    std::vector<JsonValue> arrayValue;

    static JsonValue null() { return JsonValue(); }
    static JsonValue boolean(bool b) {
        JsonValue v;
        v.type = Type::Bool;
        v.boolValue = b;
        return v;
    }
    static JsonValue number(double n) {
        JsonValue v;
        v.type = Type::Number;
        v.numberValue = n;
        return v;
    }
    static JsonValue string(std::string s) {
        JsonValue v;
        v.type = Type::String;
        v.stringValue = std::move(s);
        return v;
    }
    static JsonValue object(std::map<std::string, JsonValue> o = {}) {
        JsonValue v;
        v.type = Type::Object;
        v.objectValue = std::move(o);
        return v;
    }
    static JsonValue array(std::vector<JsonValue> a = {}) {
        JsonValue v;
        v.type = Type::Array;
        v.arrayValue = std::move(a);
        return v;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}

    JsonValue parse() {
        skipWs();
        JsonValue v = parseValue();
        skipWs();
        if (pos_ != text_.size()) throw std::runtime_error("Unexpected trailing JSON content");
        return v;
    }

private:
    const std::string& text_;
    size_t pos_ = 0;

    void skipWs() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++pos_;
            else break;
        }
    }

    bool consume(char ch) {
        skipWs();
        if (pos_ < text_.size() && text_[pos_] == ch) {
            ++pos_;
            return true;
        }
        return false;
    }

    JsonValue parseValue() {
        skipWs();
        if (pos_ >= text_.size()) throw std::runtime_error("Unexpected end of JSON");
        char c = text_[pos_];
        if (c == '"') return JsonValue::string(parseString());
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't') return parseLiteral("true", JsonValue::boolean(true));
        if (c == 'f') return parseLiteral("false", JsonValue::boolean(false));
        if (c == 'n') return parseLiteral("null", JsonValue::null());
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();
        throw std::runtime_error("Invalid JSON token");
    }

    JsonValue parseLiteral(const char* lit, JsonValue out) {
        size_t n = std::strlen(lit);
        if (text_.compare(pos_, n, lit) != 0) throw std::runtime_error("Invalid JSON literal");
        pos_ += n;
        return out;
    }

    std::string parseString() {
        if (!consume('"')) throw std::runtime_error("Expected opening quote");
        std::string out;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= text_.size()) break;
                char esc = text_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        // keep unicode escapes as-is (minimal parser; editor transport still works).
                        out += "\\u";
                        for (int i = 0; i < 4 && pos_ < text_.size(); ++i) out.push_back(text_[pos_++]);
                        break;
                    }
                    default:
                        out.push_back(esc);
                        break;
                }
                continue;
            }
            out.push_back(c);
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    JsonValue parseObject() {
        if (!consume('{')) throw std::runtime_error("Expected object");
        std::map<std::string, JsonValue> out;
        skipWs();
        if (consume('}')) return JsonValue::object(std::move(out));
        while (true) {
            std::string key = parseString();
            if (!consume(':')) throw std::runtime_error("Expected ':' after object key");
            out[key] = parseValue();
            if (consume('}')) break;
            if (!consume(',')) throw std::runtime_error("Expected ',' between object fields");
        }
        return JsonValue::object(std::move(out));
    }

    JsonValue parseArray() {
        if (!consume('[')) throw std::runtime_error("Expected array");
        std::vector<JsonValue> out;
        skipWs();
        if (consume(']')) return JsonValue::array(std::move(out));
        while (true) {
            out.push_back(parseValue());
            if (consume(']')) break;
            if (!consume(',')) throw std::runtime_error("Expected ',' between array items");
        }
        return JsonValue::array(std::move(out));
    }

    JsonValue parseNumber() {
        skipWs();
        size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        }
        double n = 0.0;
        try {
            n = std::stod(text_.substr(start, pos_ - start));
        } catch (...) {
            throw std::runtime_error("Invalid JSON number");
        }
        if (!std::isfinite(n)) n = 0;
        return JsonValue::number(n);
    }
};

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

static std::string jsonStringify(const JsonValue& v) {
    switch (v.type) {
        case JsonValue::Type::Null:
            return "null";
        case JsonValue::Type::Bool:
            return v.boolValue ? "true" : "false";
        case JsonValue::Type::Number: {
            double n = v.numberValue;
            if (!std::isfinite(n)) n = 0;
            std::ostringstream out;
            out << n;
            return out.str();
        }
        case JsonValue::Type::String:
            return "\"" + jsonEscape(v.stringValue) + "\"";
        case JsonValue::Type::Array: {
            std::string out = "[";
            for (size_t i = 0; i < v.arrayValue.size(); ++i) {
                if (i) out += ",";
                out += jsonStringify(v.arrayValue[i]);
            }
            out += "]";
            return out;
        }
        case JsonValue::Type::Object: {
            std::string out = "{";
            bool first = true;
            for (const auto& kv : v.objectValue) {
                if (!first) out += ",";
                first = false;
                out += "\"" + jsonEscape(kv.first) + "\":" + jsonStringify(kv.second);
            }
            out += "}";
            return out;
        }
        default:
            return "null";
    }
}

static const JsonValue* objGet(const JsonValue& obj, const std::string& key) {
    if (obj.type != JsonValue::Type::Object) return nullptr;
    auto it = obj.objectValue.find(key);
    if (it == obj.objectValue.end()) return nullptr;
    return &it->second;
}

static std::string asString(const JsonValue* v, const std::string& def = "") {
    if (!v || v->type != JsonValue::Type::String) return def;
    return v->stringValue;
}

static int asInt(const JsonValue* v, int def = 0) {
    if (!v) return def;
    if (v->type == JsonValue::Type::Number) return static_cast<int>(v->numberValue);
    return def;
}

static std::string decodeUriComponent(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return -1;
            };
            int h1 = hex(in[i + 1]);
            int h2 = hex(in[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                out.push_back(static_cast<char>((h1 << 4) | h2));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

static std::string uriToPath(const std::string& uri) {
    const std::string prefix = "file://";
    if (uri.rfind(prefix, 0) != 0) return uri;
    std::string path = decodeUriComponent(uri.substr(prefix.size()));
    if (!path.empty() && path[0] == '/' && path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':')
        path.erase(path.begin());
    return path;
}

static std::string lspDetectRepoRoot(const std::string& filePath) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(filePath).has_parent_path() ? fs::path(filePath).parent_path() : fs::current_path();
    for (int i = 0; i < 16 && !dir.empty(); ++i) {
        if (fs::exists(dir / "lib" / "kern")) return dir.generic_string();
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = std::move(parent);
    }
    return {};
}

static std::string pathToFileUri(const std::string& nativePath) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path ap = fs::weakly_canonical(fs::path(nativePath), ec);
    if (ec) ap = fs::path(nativePath);
    std::string g = ap.generic_string();
#ifdef _WIN32
    std::string uri = "file:///";
    for (char c : g) {
        if (c == '\\')
            uri += '/';
        else
            uri += c;
    }
    return uri;
#else
    return std::string("file://") + g;
#endif
}

static std::string lspNormalizeImportKey(std::string s) {
    for (char& c : s) {
        if (c == '\\') c = '/';
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    if (s.size() >= 4 && s.compare(s.size() - 4, 4, ".kn") == 0) s.resize(s.size() - 4);
    return s;
}

static bool lspIsStdlibNameQuick(const std::string& p) {
    if (p == "std" || p == "std.v1") return true;
    if (p.rfind("std.", 0) == 0) return true;
    static const std::unordered_set<std::string> k{
        "math",       "string",     "json",       "random",     "sys",          "io",         "net",
        "web",        "data",       "array",      "env",        "map",          "types",      "debug",
        "log",        "time",       "memory",     "util",       "profiling",    "path",       "errors",
        "iter",       "collections","fs",         "regex",      "csv",          "b64",        "logging",
        "hash",       "uuid",       "os",         "copy",       "datetime",     "secrets",    "itools",
        "cli",        "encoding",   "run",        "interop",    "concurrency",  "observability",
        "security",   "automation", "binary",     "websec",     "netops",       "datatools",  "runtime_controls"};
    return k.find(p) != k.end();
}

static bool lspImportIsVirtual(const std::string& rawImport) {
    const std::string p = lspNormalizeImportKey(rawImport);
    if (p.empty()) return false;
    if (lspIsStdlibNameQuick(p)) return true;
    if (p == "game" || p == "g2d" || p == "g3d" || p == "2dgraphics") return true;
    if (p == "process" || p == "input" || p == "vision" || p == "render") return true;
    static const char kPrefix[] = "kern::";
    if (p.size() > sizeof(kPrefix) - 1 && p.compare(0, sizeof(kPrefix) - 1, kPrefix) == 0) {
        const std::string rest = p.substr(sizeof(kPrefix) - 1);
        if (rest == "process" || rest == "input" || rest == "vision" || rest == "render") return true;
        if (lspIsStdlibNameQuick(rest)) return true;
    }
    return p.find("__does_not_exist__") != std::string::npos;
}

static std::string lspResolveImportOnDisk(const std::string& importerFile, const std::string& rawImport,
                                          const std::string& projectRoot, const std::vector<std::string>& includePaths) {
    if (lspImportIsVirtual(rawImport)) return {};
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path importer = fs::weakly_canonical(fs::path(importerFile), ec);
    if (ec) importer = fs::path(importerFile);
    fs::path r(rawImport);
    if (r.extension().empty()) r += ".kn";
    std::vector<fs::path> probes;
    if (r.is_absolute()) probes.push_back(r);
    probes.push_back(importer.parent_path() / r);
    if (!projectRoot.empty()) probes.push_back(fs::path(projectRoot) / r);
    for (const auto& inc : includePaths) {
        fs::path incPath(inc);
        if (!incPath.is_absolute() && !projectRoot.empty()) incPath = fs::path(projectRoot) / incPath;
        probes.push_back(incPath / r);
    }
    for (auto& p : probes) {
        std::error_code ec2;
        fs::path can = fs::weakly_canonical(p, ec2);
        if (!ec2 && fs::exists(can)) return can.generic_string();
    }
    return {};
}

static std::string importBindingNameForLsp(const ImportStmt* imp) {
    if (!imp) return {};
    std::string bindingName = imp->hasAlias ? imp->alias : imp->moduleName;
    if (!imp->hasAlias && bindingName.size() >= 4 && bindingName.compare(bindingName.size() - 4, 4, ".kn") == 0)
        bindingName = bindingName.substr(0, bindingName.size() - 4);
    return bindingName;
}

static std::string readTextFilePath(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct LspTransport {
    bool readMessage(std::string& outJson) {
        outJson.clear();
        std::string line;
        int contentLength = -1;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            const std::string k = "Content-Length:";
            if (line.rfind(k, 0) == 0) {
                std::string n = line.substr(k.size());
                contentLength = std::atoi(n.c_str());
            }
        }
        if (contentLength < 0) return false;
        outJson.resize(static_cast<size_t>(contentLength));
        std::cin.read(&outJson[0], contentLength);
        return static_cast<int>(std::cin.gcount()) == contentLength;
    }

    void send(const JsonValue& msg) {
        std::string payload = jsonStringify(msg);
        std::cout << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
        std::cout.flush();
    }
};

static SourceSpan lspPosToSpan(const JsonValue* pos) {
    int line = asInt(pos ? objGet(*pos, "line") : nullptr, 0);
    int ch = asInt(pos ? objGet(*pos, "character") : nullptr, 0);
    return normalizeSourceSpan(line + 1, ch + 1, line + 1, ch + 1);
}

static JsonValue spanToLspRange(const SourceSpan& s) {
    SourceSpan n = normalizeSourceSpan(s.line, s.column, s.lineEnd, s.columnEnd);
    return JsonValue::object({
        {"start", JsonValue::object({
            {"line", JsonValue::number(std::max(0, n.line - 1))},
            {"character", JsonValue::number(std::max(0, n.column - 1))}
        })},
        {"end", JsonValue::object({
            {"line", JsonValue::number(std::max(0, n.lineEnd - 1))},
            {"character", JsonValue::number(std::max(0, n.columnEnd))}
        })}
    });
}

struct DiagnosticItem {
    SourceSpan span;
    int severity = 1;  // 1 error, 2 Warning, 3 Info
    std::string message;
    std::string code;
};

static DiagnosticItem mkDiag(SourceSpan span, int severity, std::string msg, std::string code) {
    DiagnosticItem d;
    d.span = normalizeSourceSpan(span.line, span.column, span.lineEnd, span.columnEnd);
    d.severity = severity;
    d.message = std::move(msg);
    d.code = std::move(code);
    return d;
}

static std::vector<DiagnosticItem> collectDiagnostics(const std::string& source, const std::string& filePath) {
    std::vector<DiagnosticItem> out;
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        CodeGenerator gen;
        (void)gen.generate(std::move(program));
        SemanticResult sem = analyzeSemanticSource(source, filePath, false);
        for (const auto& d : sem.diagnostics) {
            int sev = 3;
            if (d.severity == SemanticSeverity::Error) sev = 1;
            else if (d.severity == SemanticSeverity::Warning) sev = 2;
            out.push_back(mkDiag(normalizeSourceSpan(d.line, d.column, d.line, d.column), sev, d.message, d.code));
        }
    } catch (const LexerError& e) {
        out.push_back(mkDiag(normalizeSourceSpan(e.line, e.column, e.line, e.column), 1, e.what(), "LEX-TOKENIZE"));
    } catch (const ParserError& e) {
        out.push_back(mkDiag(normalizeSourceSpan(e.line, e.column, e.line, e.column), 1, e.what(), "PARSE-SYNTAX"));
    } catch (const std::exception& e) {
        out.push_back(mkDiag(normalizeSourceSpan(1, 1, 1, 1), 1, std::string("compile failed: ") + e.what(), "LSP-COMPILE"));
    }
    return out;
}

struct IndexedSymbol {
    std::string name;
    std::string kind;
    SourceSpan decl;
    int scopeStartLine = 1;
    int scopeEndLine = 1;
    int depth = 0;
    std::string detail;
};

static int maxStmtLine(const Stmt* s) {
    if (!s) return 1;
    int m = s->line > 0 ? s->line : 1;
    if (const auto* p = dynamic_cast<const Program*>(s)) {
        for (const auto& st : p->statements) m = std::max(m, maxStmtLine(st.get()));
    } else if (const auto* b = dynamic_cast<const BlockStmt*>(s)) {
        for (const auto& st : b->statements) m = std::max(m, maxStmtLine(st.get()));
    } else if (const auto* u = dynamic_cast<const UnsafeBlockStmt*>(s)) {
        m = std::max(m, maxStmtLine(u->body.get()));
    } else if (const auto* i = dynamic_cast<const IfStmt*>(s)) {
        m = std::max(m, maxStmtLine(i->thenBranch.get()));
        m = std::max(m, maxStmtLine(i->elseBranch.get()));
        for (const auto& e : i->elifBranches) m = std::max(m, maxStmtLine(e.second.get()));
    } else if (const auto* forRangeStmt = dynamic_cast<const ForRangeStmt*>(s)) {
        m = std::max(m, maxStmtLine(forRangeStmt->body.get()));
    } else if (const auto* forInStmt = dynamic_cast<const ForInStmt*>(s)) {
        m = std::max(m, maxStmtLine(forInStmt->body.get()));
    } else if (const auto* forCStyleStmt = dynamic_cast<const ForCStyleStmt*>(s)) {
        m = std::max(m, maxStmtLine(forCStyleStmt->init.get()));
        m = std::max(m, maxStmtLine(forCStyleStmt->body.get()));
    } else if (const auto* w = dynamic_cast<const WhileStmt*>(s)) {
        m = std::max(m, maxStmtLine(w->body.get()));
    } else if (const auto* repeatStmt = dynamic_cast<const RepeatStmt*>(s)) {
        m = std::max(m, maxStmtLine(repeatStmt->body.get()));
    } else if (const auto* repeatWhileStmt = dynamic_cast<const RepeatWhileStmt*>(s)) {
        m = std::max(m, maxStmtLine(repeatWhileStmt->body.get()));
    } else if (const auto* t = dynamic_cast<const TryStmt*>(s)) {
        m = std::max(m, maxStmtLine(t->tryBlock.get()));
        m = std::max(m, maxStmtLine(t->catchBlock.get()));
        m = std::max(m, maxStmtLine(t->elseBlock.get()));
        m = std::max(m, maxStmtLine(t->finallyBlock.get()));
    } else if (const auto* functionDecl = dynamic_cast<const FunctionDeclStmt*>(s)) {
        m = std::max(m, maxStmtLine(functionDecl->body.get()));
    } else if (const auto* c = dynamic_cast<const ClassDeclStmt*>(s)) {
        for (const auto& st : c->methods) m = std::max(m, maxStmtLine(st.get()));
        for (const auto& st : c->constructorBody) m = std::max(m, maxStmtLine(st.get()));
    }
    return m;
}

static void addSymbol(std::vector<IndexedSymbol>& out, const std::string& name, const std::string& kind,
                      int line, int col, int scopeStart, int scopeEnd, int depth, const std::string& detail) {
    if (name.empty()) return;
    IndexedSymbol s;
    s.name = name;
    s.kind = kind;
    s.decl = normalizeSourceSpan(line, col > 0 ? col : 1, line, col > 0 ? col : 1);
    s.scopeStartLine = std::max(1, scopeStart);
    s.scopeEndLine = std::max(s.scopeStartLine, scopeEnd);
    s.depth = depth;
    s.detail = detail;
    out.push_back(std::move(s));
}

static void indexStmt(const Stmt* s, int scopeStart, int scopeEnd, int depth, std::vector<IndexedSymbol>& out);

static void indexStmt(const Stmt* s, int scopeStart, int scopeEnd, int depth, std::vector<IndexedSymbol>& out) {
    if (!s) return;
    if (const auto* p = dynamic_cast<const Program*>(s)) {
        for (const auto& st : p->statements) indexStmt(st.get(), scopeStart, scopeEnd, depth, out);
        return;
    }
    if (const auto* b = dynamic_cast<const BlockStmt*>(s)) {
        int bStart = std::max(1, b->line);
        int bEnd = maxStmtLine(b);
        for (const auto& st : b->statements) indexStmt(st.get(), bStart, bEnd, depth + 1, out);
        return;
    }
    if (const auto* v = dynamic_cast<const VarDeclStmt*>(s)) {
        addSymbol(out, v->name, "variable", v->line, v->column, scopeStart, scopeEnd, depth, v->hasType ? v->typeName : "var");
    } else if (const auto* d = dynamic_cast<const DestructureStmt*>(s)) {
        for (const auto& n : d->names)
            addSymbol(out, n, "variable", d->line, d->column, scopeStart, scopeEnd, depth, "var");
    } else if (const auto* f = dynamic_cast<const FunctionDeclStmt*>(s)) {
        addSymbol(out, f->name, "function", f->line, f->column, scopeStart, scopeEnd, depth, "def");
        int fnStart = f->line;
        int fnEnd = maxStmtLine(f);
        for (const auto& p : f->params)
            addSymbol(out, p.name, "parameter", f->line, f->column, fnStart, fnEnd, depth + 1, p.typeName);
        indexStmt(f->body.get(), fnStart, fnEnd, depth + 1, out);
    } else if (const auto* c = dynamic_cast<const ClassDeclStmt*>(s)) {
        addSymbol(out, c->name, "class", c->line, c->column, scopeStart, scopeEnd, depth, "class");
        int clStart = c->line;
        int clEnd = maxStmtLine(c);
        for (const auto& m : c->methods) indexStmt(m.get(), clStart, clEnd, depth + 1, out);
        for (const auto& ctor : c->constructorBody) indexStmt(ctor.get(), clStart, clEnd, depth + 1, out);
    } else if (const auto* sdecl = dynamic_cast<const StructDeclStmt*>(s)) {
        addSymbol(out, sdecl->name, "struct", sdecl->line, sdecl->column, scopeStart, scopeEnd, depth, "struct");
    } else if (const auto* ff = dynamic_cast<const FfiDeclStmt*>(s)) {
        addSymbol(out, ff->name, "function", ff->line, ff->column, scopeStart, scopeEnd, depth, "extern def");
    } else if (const auto* fr = dynamic_cast<const ForRangeStmt*>(s)) {
        int bodyEnd = maxStmtLine(fr->body.get());
        addSymbol(out, fr->varName, "variable", fr->line, fr->column, fr->line, bodyEnd, depth + 1, "for-range");
        indexStmt(fr->body.get(), fr->line, bodyEnd, depth + 1, out);
    } else if (const auto* fi = dynamic_cast<const ForInStmt*>(s)) {
        int bodyEnd = maxStmtLine(fi->body.get());
        addSymbol(out, fi->varName, "variable", fi->line, fi->column, fi->line, bodyEnd, depth + 1, "for-in");
        if (!fi->valueVarName.empty())
            addSymbol(out, fi->valueVarName, "variable", fi->line, fi->column, fi->line, bodyEnd, depth + 1, "for-in-value");
        indexStmt(fi->body.get(), fi->line, bodyEnd, depth + 1, out);
    } else if (const auto* fc = dynamic_cast<const ForCStyleStmt*>(s)) {
        int bodyEnd = maxStmtLine(fc->body.get());
        indexStmt(fc->init.get(), fc->line, bodyEnd, depth + 1, out);
        indexStmt(fc->body.get(), fc->line, bodyEnd, depth + 1, out);
    } else if (const auto* w = dynamic_cast<const WhileStmt*>(s)) {
        indexStmt(w->body.get(), w->line, maxStmtLine(w->body.get()), depth + 1, out);
    } else if (const auto* r = dynamic_cast<const RepeatStmt*>(s)) {
        indexStmt(r->body.get(), r->line, maxStmtLine(r->body.get()), depth + 1, out);
    } else if (const auto* rw = dynamic_cast<const RepeatWhileStmt*>(s)) {
        indexStmt(rw->body.get(), rw->line, maxStmtLine(rw->body.get()), depth + 1, out);
    } else if (const auto* t = dynamic_cast<const TryStmt*>(s)) {
        int tryEnd = maxStmtLine(t);
        indexStmt(t->tryBlock.get(), t->line, tryEnd, depth + 1, out);
        if (!t->catchVar.empty())
            addSymbol(out, t->catchVar, "variable", t->line, t->column, t->line, tryEnd, depth + 1, "catch");
        indexStmt(t->catchBlock.get(), t->line, tryEnd, depth + 1, out);
        indexStmt(t->elseBlock.get(), t->line, tryEnd, depth + 1, out);
        indexStmt(t->finallyBlock.get(), t->line, tryEnd, depth + 1, out);
    } else if (const auto* i = dynamic_cast<const IfStmt*>(s)) {
        indexStmt(i->thenBranch.get(), i->line, maxStmtLine(i->thenBranch.get()), depth + 1, out);
        for (const auto& el : i->elifBranches) indexStmt(el.second.get(), i->line, maxStmtLine(el.second.get()), depth + 1, out);
        indexStmt(i->elseBranch.get(), i->line, maxStmtLine(i->elseBranch.get()), depth + 1, out);
    } else if (const auto* u = dynamic_cast<const UnsafeBlockStmt*>(s)) {
        indexStmt(u->body.get(), u->line, maxStmtLine(u->body.get()), depth + 1, out);
    }
}

static std::vector<IndexedSymbol> buildSymbolIndex(const std::string& source) {
    std::vector<IndexedSymbol> out;
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        int endLine = std::max(1, maxStmtLine(program.get()));
        indexStmt(program.get(), 1, endLine, 0, out);
    } catch (...) {
        // leave empty on parse errors; completion still falls back to keywords/builtins.
    }
    return out;
}

static bool findTopLevelDefinitionSpan(const std::string& source, const std::string& name, SourceSpan& outSpan) {
    std::vector<IndexedSymbol> all = buildSymbolIndex(source);
    const IndexedSymbol* best = nullptr;
    for (const auto& s : all) {
        if (s.name != name || s.depth != 0) continue;
        if (!best || s.decl.line < best->decl.line) best = &s;
    }
    if (!best) return false;
    outSpan = best->decl;
    return true;
}

static int lspSymbolKindFromIndexed(const std::string& kind) {
    if (kind == "function") return 12;  // Function
    if (kind == "class") return 5;    // Class
    if (kind == "struct") return 23;  // Struct
    return 13;                        // Variable (also parameter)
}

static JsonValue indexedSymbolToDocumentSymbol(const IndexedSymbol& s) {
    std::map<std::string, JsonValue> fields;
    fields["name"] = JsonValue::string(s.name);
    fields["kind"] = JsonValue::number(static_cast<double>(lspSymbolKindFromIndexed(s.kind)));
    fields["range"] = spanToLspRange(s.decl);
    fields["selectionRange"] = spanToLspRange(s.decl);
    if (!s.detail.empty()) fields["detail"] = JsonValue::string(s.detail);
    fields["children"] = JsonValue::array({});
    return JsonValue::object(std::move(fields));
}

static std::vector<std::string> kKeywords = {
    "let","var","const","def","class","struct","enum","extern","unsafe","return","if","elif","else",
    "for","while","repeat","in","match","case","try","catch","finally","throw","rethrow","break","continue","import"
};

static std::unordered_set<std::string> builtinNameSet() {
    std::unordered_set<std::string> names;
    insertAllBuiltinNamesForAnalysis(names);
#ifdef KERN_BUILD_GAME
    for (const auto& n : getGameBuiltinNames()) names.insert(n);
#endif
    return names;
}

static std::string builtinPermissionHint(const std::string& name) {
    static const std::unordered_map<std::string, std::string> hints = {
        {"read_file", "permission: filesystem.read"},
        {"write_file", "permission: filesystem.write"},
        {"append_file", "permission: filesystem.write"},
        {"delete_file", "permission: filesystem.write"},
        {"copy_file", "permission: filesystem.write"},
        {"move_file", "permission: filesystem.write"},
        {"exec", "permission: system.exec"},
        {"exec_args", "permission: system.exec"},
        {"exec_capture", "permission: system.exec"},
        {"spawn", "permission: process.control"},
        {"wait_process", "permission: process.control"},
        {"kill_process", "permission: process.control"},
        {"http_get", "permission: network.http"},
        {"http_post", "permission: network.http"},
        {"http_request", "permission: network.http"},
        {"tcp_connect", "permission: network.tcp"},
        {"tcp_listen", "permission: network.tcp"},
        {"udp_open", "permission: network.udp"},
        {"ffi_call", "unsafe + ffi enabled"},
        {"ffi_call_typed", "unsafe + ffi enabled"},
        {"alloc", "unsafe context"},
        {"free", "unsafe context"}};
    auto it = hints.find(name);
    return it == hints.end() ? std::string() : it->second;
}

static std::vector<Token> safeTokenize(const std::string& source) {
    try {
        Lexer lx(source);
        return lx.tokenize();
    } catch (...) {
        return {};
    }
}

static const Token* findIdentifierAt(const std::vector<Token>& tokens, int line1, int col1) {
    for (const auto& t : tokens) {
        if (t.type != TokenType::IDENTIFIER) continue;
        int start = t.column;
        int end = t.column + static_cast<int>(t.lexeme.size()) - 1;
        if (t.line == line1 && col1 >= start && col1 <= end) return &t;
    }
    return nullptr;
}

static std::string lineAt(const std::string& source, int line1) {
    std::istringstream in(source);
    std::string line;
    int n = 1;
    while (std::getline(in, line)) {
        if (n == line1) return line;
        ++n;
    }
    return "";
}

static std::string wordPrefixAt(const std::string& source, int line1, int col1) {
    std::string ln = lineAt(source, line1);
    if (ln.empty()) return "";
    int i = std::max(0, std::min(static_cast<int>(ln.size()), col1 - 1));
    int s = i;
    while (s > 0) {
        char c = ln[static_cast<size_t>(s - 1)];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') --s;
        else break;
    }
    return ln.substr(static_cast<size_t>(s), static_cast<size_t>(i - s));
}

static bool startsWithCaseInsensitive(const std::string& s, const std::string& pfx) {
    if (pfx.size() > s.size()) return false;
    for (size_t i = 0; i < pfx.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(pfx[i])))
            return false;
    }
    return true;
}

static bool semanticTokenSkippable(TokenType t) {
    return t == TokenType::COMMENT_LINE || t == TokenType::COMMENT_BLOCK || t == TokenType::NEWLINE ||
           t == TokenType::END_OF_FILE;
}

/** LSP legend indices: keyword, function, variable, string, number, operator */
static unsigned semanticTokenTypeIndex(TokenType t) {
    if (t == TokenType::IDENTIFIER) return 2;
    if (t == TokenType::STRING) return 3;
    if (t == TokenType::INTEGER || t == TokenType::FLOAT || t == TokenType::TRUE || t == TokenType::FALSE ||
        t == TokenType::NULL_LIT)
        return 4;
    if (t == TokenType::PLUS || t == TokenType::MINUS || t == TokenType::STAR || t == TokenType::SLASH ||
        t == TokenType::PERCENT || t == TokenType::STAR_STAR || t == TokenType::EQ || t == TokenType::NEQ ||
        t == TokenType::LT || t == TokenType::GT || t == TokenType::LE || t == TokenType::GE || t == TokenType::AND ||
        t == TokenType::OR || t == TokenType::NOT || t == TokenType::BIT_AND || t == TokenType::BIT_OR ||
        t == TokenType::BIT_XOR || t == TokenType::SHL || t == TokenType::SHR || t == TokenType::ASSIGN ||
        t == TokenType::PLUS_EQ || t == TokenType::MINUS_EQ || t == TokenType::STAR_EQ || t == TokenType::SLASH_EQ ||
        t == TokenType::PERCENT_EQ || t == TokenType::LPAREN || t == TokenType::RPAREN || t == TokenType::LBRACE ||
        t == TokenType::RBRACE || t == TokenType::LBRACKET || t == TokenType::RBRACKET || t == TokenType::COMMA ||
        t == TokenType::DOT || t == TokenType::DOT_DOT || t == TokenType::COLON || t == TokenType::SEMICOLON ||
        t == TokenType::ARROW || t == TokenType::PIPE || t == TokenType::ELLIPSIS || t == TokenType::QUESTION ||
        t == TokenType::QUESTION_DOT || t == TokenType::COALESCE || t == TokenType::COALESCE_EQ || t == TokenType::AT)
        return 5;
    return 0;
}

static JsonValue buildSemanticTokensData(const std::string& source) {
    std::vector<Token> toks;
    try {
        Lexer lexer(source);
        toks = lexer.tokenize();
    } catch (...) {
        return JsonValue::object({{"data", JsonValue::array({})}});
    }
    std::vector<const Token*> ordered;
    ordered.reserve(toks.size());
    for (const auto& t : toks) {
        if (semanticTokenSkippable(t.type)) continue;
        ordered.push_back(&t);
    }
    std::sort(ordered.begin(), ordered.end(), [](const Token* a, const Token* b) {
        if (a->line != b->line) return a->line < b->line;
        return a->column < b->column;
    });
    std::vector<JsonValue> data;
    int prevL0 = -1;
    int prevStart = 0;
    int prevLen = 0;
    for (const Token* tp : ordered) {
        const int l0 = tp->line - 1;
        const int c0 = tp->column - 1;
        const int len = static_cast<int>(tp->lexeme.size());
        if (len <= 0) continue;
        uint32_t deltaLine = 0;
        uint32_t deltaStart = 0;
        if (prevL0 < 0) {
            deltaLine = static_cast<uint32_t>(std::max(0, l0));
            deltaStart = static_cast<uint32_t>(std::max(0, c0));
        } else if (l0 > prevL0) {
            deltaLine = static_cast<uint32_t>(l0 - prevL0);
            deltaStart = static_cast<uint32_t>(std::max(0, c0));
        } else {
            deltaLine = 0;
            const int afterPrev = prevStart + prevLen;
            deltaStart = static_cast<uint32_t>(std::max(0, c0 - afterPrev));
        }
        const unsigned tt = semanticTokenTypeIndex(tp->type);
        data.push_back(JsonValue::number(static_cast<double>(deltaLine)));
        data.push_back(JsonValue::number(static_cast<double>(deltaStart)));
        data.push_back(JsonValue::number(static_cast<double>(len)));
        data.push_back(JsonValue::number(static_cast<double>(tt)));
        data.push_back(JsonValue::number(0.0));
        prevL0 = l0;
        prevStart = c0;
        prevLen = len;
    }
    return JsonValue::object({{"data", JsonValue::array(std::move(data))}});
}

struct DocumentState {
    std::string uri;
    std::string path;
    std::string text;
    int version = 0;
};

static void collectTopLevelImportExprBindings(const Program* prog, std::unordered_map<std::string, std::string>& out) {
    if (!prog) return;
    for (const auto& st : prog->statements) {
        const auto* v = dynamic_cast<const VarDeclStmt*>(st.get());
        if (!v || !v->initializer) continue;
        const auto* call = dynamic_cast<const CallExpr*>(v->initializer.get());
        if (!call || !call->callee) continue;
        const auto* calleeId = dynamic_cast<const Identifier*>(call->callee.get());
        if (!calleeId || calleeId->name != "__import") continue;
        if (call->args.empty() || call->args[0].spread || !call->args[0].expr) continue;
        const auto* lit = dynamic_cast<const StringLiteral*>(call->args[0].expr.get());
        if (!lit) continue;
        out[v->name] = lit->value;
    }
}

static std::string loadKnTextForResolvedPath(const std::unordered_map<std::string, DocumentState>& docs,
                                             const std::string& resolvedPath) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path want = fs::weakly_canonical(fs::path(resolvedPath), ec);
    if (ec) want = fs::path(resolvedPath);
    const std::string wantGen = want.generic_string();
    for (const auto& kv : docs) {
        std::error_code ec2;
        fs::path p = fs::weakly_canonical(fs::path(uriToPath(kv.first)), ec2);
        if (!ec2 && p.generic_string() == wantGen) return kv.second.text;
    }
    return readTextFilePath(resolvedPath);
}

class KernLspServer {
public:
    void run() {
        while (true) {
            std::string payload;
            if (!transport_.readMessage(payload)) break;
            JsonValue msg;
            try {
                msg = JsonParser(payload).parse();
            } catch (...) {
                continue;
            }
            handleMessage(msg);
            if (exitNow_) break;
        }
    }

private:
    LspTransport transport_;
    std::unordered_map<std::string, DocumentState> docs_;
    bool shutdownRequested_ = false;
    bool exitNow_ = false;

    void sendResponse(const JsonValue& id, const JsonValue& result) {
        transport_.send(JsonValue::object({
            {"jsonrpc", JsonValue::string("2.0")},
            {"id", id},
            {"result", result}
        }));
    }

    void sendError(const JsonValue& id, int code, const std::string& message) {
        transport_.send(JsonValue::object({
            {"jsonrpc", JsonValue::string("2.0")},
            {"id", id},
            {"error", JsonValue::object({
                {"code", JsonValue::number(code)},
                {"message", JsonValue::string(message)}
            })}
        }));
    }

    void sendNotification(const std::string& method, JsonValue params) {
        transport_.send(JsonValue::object({
            {"jsonrpc", JsonValue::string("2.0")},
            {"method", JsonValue::string(method)},
            {"params", std::move(params)}
        }));
    }

    void publishDiagnosticsFor(const DocumentState& doc) {
        std::vector<DiagnosticItem> list = collectDiagnostics(doc.text, doc.path.empty() ? doc.uri : doc.path);
        std::vector<JsonValue> arr;
        arr.reserve(list.size());
        for (const auto& d : list) {
            arr.push_back(JsonValue::object({
                {"range", spanToLspRange(d.span)},
                {"severity", JsonValue::number(d.severity)},
                {"source", JsonValue::string("kern")},
                {"code", JsonValue::string(d.code)},
                {"message", JsonValue::string(d.message)}
            }));
        }
        sendNotification("textDocument/publishDiagnostics", JsonValue::object({
            {"uri", JsonValue::string(doc.uri)},
            {"diagnostics", JsonValue::array(std::move(arr))}
        }));
    }

    std::vector<IndexedSymbol> visibleSymbolsFor(const DocumentState& doc, int line, int column) {
        (void)column;
        std::vector<IndexedSymbol> all = buildSymbolIndex(doc.text);
        std::vector<IndexedSymbol> visible;
        visible.reserve(all.size());
        for (const auto& s : all) {
            if (s.decl.line > line) continue;
            if (line < s.scopeStartLine || line > s.scopeEndLine) continue;
            visible.push_back(s);
        }
        return visible;
    }

    JsonValue handleCompletion(const DocumentState& doc, int line, int column) {
        std::vector<IndexedSymbol> visible = visibleSymbolsFor(doc, line, column);
        std::unordered_set<std::string> builtins = builtinNameSet();
        std::string pfx = wordPrefixAt(doc.text, line, column);

        struct Candidate {
            std::string label;
            int kind = 6;
            std::string detail;
            int score = 100;
        };
        std::unordered_map<std::string, Candidate> cands;

        for (const auto& s : visible) {
            if (!startsWithCaseInsensitive(s.name, pfx)) continue;
            Candidate c;
            c.label = s.name;
            c.kind = (s.kind == "function") ? 3 : (s.kind == "class" || s.kind == "struct" || s.kind == "enum") ? 7 : 6;
            c.detail = s.detail.empty() ? s.kind : s.detail;
            c.score = 10 - std::min(9, s.depth);
            auto it = cands.find(c.label);
            if (it == cands.end() || c.score < it->second.score) cands[c.label] = c;
        }

        for (const auto& kw : kKeywords) {
            if (!startsWithCaseInsensitive(kw, pfx)) continue;
            if (cands.find(kw) != cands.end()) continue;
            cands[kw] = Candidate{kw, 14, "keyword", 30};
        }
        for (const auto& b : builtins) {
            if (!startsWithCaseInsensitive(b, pfx)) continue;
            if (cands.find(b) != cands.end()) continue;
            cands[b] = Candidate{b, 3, "builtin", 40};
        }

        std::vector<Candidate> ordered;
        ordered.reserve(cands.size());
        for (auto& kv : cands) ordered.push_back(std::move(kv.second));
        std::sort(ordered.begin(), ordered.end(), [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score < b.score;
            return a.label < b.label;
        });

        std::vector<JsonValue> items;
        items.reserve(ordered.size());
        for (const auto& c : ordered) {
            items.push_back(JsonValue::object({
                {"label", JsonValue::string(c.label)},
                {"kind", JsonValue::number(c.kind)},
                {"detail", JsonValue::string(c.detail)},
                {"sortText", JsonValue::string((c.score < 10 ? "0" : "1") + c.label)}
            }));
        }
        return JsonValue::array(std::move(items));
    }

    /** Same-file first; then `import` / `from … import` targets on disk (open buffer if already in LSP). */
    JsonValue handleDefinition(const DocumentState& doc, int line, int column) {
        std::vector<Token> toks = safeTokenize(doc.text);
        const Token* id = findIdentifierAt(toks, line, column);
        if (!id) return JsonValue::null();
        std::vector<IndexedSymbol> visible = visibleSymbolsFor(doc, line, column);
        IndexedSymbol* best = nullptr;
        for (auto& s : visible) {
            if (s.name != id->lexeme) continue;
            if (!best) best = &s;
            else if (s.depth > best->depth || (s.depth == best->depth && s.decl.line >= best->decl.line)) best = &s;
        }
        if (best)
            return JsonValue::object({
                {"uri", JsonValue::string(doc.uri)},
                {"range", spanToLspRange(best->decl)}
            });

        std::unique_ptr<Program> program;
        try {
            Lexer lexer(doc.text);
            std::vector<Token> tokens = lexer.tokenize();
            Parser parser(std::move(tokens));
            program = parser.parse();
        } catch (...) {
            return JsonValue::null();
        }
        const std::string& idName = id->lexeme;
        std::string importerPath = !doc.path.empty() ? doc.path : uriToPath(doc.uri);
        if (importerPath.empty()) return JsonValue::null();

        std::vector<std::string> inc;
        if (const char* lib = kernGetEnv("KERN_LIB")) inc.emplace_back(lib);
        const std::string root = lspDetectRepoRoot(importerPath);

        for (const auto& st : program->statements) {
            const auto* imp = dynamic_cast<const ImportStmt*>(st.get());
            if (!imp) continue;

            if (!imp->namedImports.empty()) {
                bool wants = false;
                for (const auto& n : imp->namedImports) {
                    if (n == idName) {
                        wants = true;
                        break;
                    }
                }
                if (!wants) continue;
                const std::string modPath = lspResolveImportOnDisk(importerPath, imp->moduleName, root, inc);
                if (modPath.empty()) continue;
                const std::string modText = loadKnTextForResolvedPath(docs_, modPath);
                if (modText.empty()) continue;
                SourceSpan span;
                if (!findTopLevelDefinitionSpan(modText, idName, span)) continue;
                return JsonValue::object({
                    {"uri", JsonValue::string(pathToFileUri(modPath))},
                    {"range", spanToLspRange(span)}
                });
            }

            if (importBindingNameForLsp(imp) == idName) {
                const std::string modPath = lspResolveImportOnDisk(importerPath, imp->moduleName, root, inc);
                if (modPath.empty()) continue;
                SourceSpan entry = normalizeSourceSpan(1, 1, 1, 1);
                return JsonValue::object({
                    {"uri", JsonValue::string(pathToFileUri(modPath))},
                    {"range", spanToLspRange(entry)}
                });
            }
        }

        std::unordered_map<std::string, std::string> importExprBindings;
        collectTopLevelImportExprBindings(program.get(), importExprBindings);
        const auto itBind = importExprBindings.find(idName);
        if (itBind != importExprBindings.end()) {
            const std::string modPath = lspResolveImportOnDisk(importerPath, itBind->second, root, inc);
            if (!modPath.empty()) {
                const SourceSpan entry = normalizeSourceSpan(1, 1, 1, 1);
                return JsonValue::object({
                    {"uri", JsonValue::string(pathToFileUri(modPath))},
                    {"range", spanToLspRange(entry)}
                });
            }
        }
        return JsonValue::null();
    }

    JsonValue handleHover(const DocumentState& doc, int line, int column) {
        std::vector<Token> toks = safeTokenize(doc.text);
        const Token* id = findIdentifierAt(toks, line, column);
        if (!id) return JsonValue::null();

        std::vector<IndexedSymbol> visible = visibleSymbolsFor(doc, line, column);
        IndexedSymbol* best = nullptr;
        for (auto& s : visible) {
            if (s.name != id->lexeme) continue;
            if (!best) best = &s;
            else if (s.depth > best->depth || (s.depth == best->depth && s.decl.line >= best->decl.line)) best = &s;
        }

        std::string text;
        if (best) {
            text = best->name + " — " + (best->detail.empty() ? best->kind : best->detail);
        } else {
            std::unordered_set<std::string> builtins = builtinNameSet();
            if (builtins.find(id->lexeme) != builtins.end()) {
                text = id->lexeme + " — builtin";
                std::string hint = builtinPermissionHint(id->lexeme);
                if (!hint.empty()) text += "\n" + hint;
            }
            else text = id->lexeme;
        }

        return JsonValue::object({
            {"contents", JsonValue::object({
                {"kind", JsonValue::string("markdown")},
                {"value", JsonValue::string(text)}
            })}
        });
    }

    JsonValue handleDocumentSymbol(const DocumentState& doc) {
        std::vector<IndexedSymbol> all = buildSymbolIndex(doc.text);
        std::vector<JsonValue> arr;
        arr.reserve(all.size());
        for (const auto& s : all) arr.push_back(indexedSymbolToDocumentSymbol(s));
        return JsonValue::array(std::move(arr));
    }

    JsonValue handleWorkspaceSymbol(const std::string& query) {
        std::string qlow = query;
        std::transform(qlow.begin(), qlow.end(), qlow.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::vector<JsonValue> arr;
        for (const auto& kv : docs_) {
            const DocumentState& doc = kv.second;
            std::vector<IndexedSymbol> all = buildSymbolIndex(doc.text);
            for (const auto& s : all) {
                if (!qlow.empty()) {
                    std::string nlow = s.name;
                    std::transform(nlow.begin(), nlow.end(), nlow.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (nlow.find(qlow) == std::string::npos) continue;
                }
                arr.push_back(JsonValue::object({
                    {"name", JsonValue::string(s.name)},
                    {"kind", JsonValue::number(static_cast<double>(lspSymbolKindFromIndexed(s.kind)))},
                    {"location", JsonValue::object({
                        {"uri", JsonValue::string(doc.uri)},
                        {"range", spanToLspRange(s.decl)}
                    })}
                }));
            }
        }
        return JsonValue::array(std::move(arr));
    }

    void handleRequest(const JsonValue& req) {
        const JsonValue* id = objGet(req, "id");
        if (!id) return;
        std::string method = asString(objGet(req, "method"));
        const JsonValue* params = objGet(req, "params");

        if (method == "initialize") {
            JsonValue result = JsonValue::object({
                {"capabilities", JsonValue::object({
                    {"textDocumentSync", JsonValue::number(1)},  // full
                    {"hoverProvider", JsonValue::boolean(true)},
                    {"definitionProvider", JsonValue::boolean(true)},
                    {"semanticTokensProvider", JsonValue::object({
                        {"legend", JsonValue::object({
                            {"tokenTypes", JsonValue::array({
                                JsonValue::string("keyword"),
                                JsonValue::string("function"),
                                JsonValue::string("variable"),
                                JsonValue::string("string"),
                                JsonValue::string("number"),
                                JsonValue::string("operator")
                            })},
                            {"tokenModifiers", JsonValue::array({})}
                        })},
                        {"full", JsonValue::boolean(true)}
                    })},
                    {"completionProvider", JsonValue::object({
                        {"resolveProvider", JsonValue::boolean(false)}
                    })},
                    {"documentSymbolProvider", JsonValue::boolean(true)},
                    {"workspaceSymbolProvider", JsonValue::boolean(true)}
                })},
                {"serverInfo", JsonValue::object({
                    {"name", JsonValue::string("kern-lsp")},
                    {"version", JsonValue::string("0.1.0")}
                })}
            });
            sendResponse(*id, result);
            return;
        }
        if (method == "shutdown") {
            shutdownRequested_ = true;
            sendResponse(*id, JsonValue::null());
            return;
        }
        if (method == "textDocument/completion") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            auto it = docs_.find(uri);
            if (it == docs_.end()) {
                sendResponse(*id, JsonValue::array({}));
                return;
            }
            SourceSpan pos = lspPosToSpan(params ? objGet(*params, "position") : nullptr);
            sendResponse(*id, handleCompletion(it->second, pos.line, pos.column));
            return;
        }
        if (method == "textDocument/definition") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            auto it = docs_.find(uri);
            if (it == docs_.end()) {
                sendResponse(*id, JsonValue::null());
                return;
            }
            SourceSpan pos = lspPosToSpan(params ? objGet(*params, "position") : nullptr);
            sendResponse(*id, handleDefinition(it->second, pos.line, pos.column));
            return;
        }
        if (method == "textDocument/hover") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            auto it = docs_.find(uri);
            if (it == docs_.end()) {
                sendResponse(*id, JsonValue::null());
                return;
            }
            SourceSpan pos = lspPosToSpan(params ? objGet(*params, "position") : nullptr);
            sendResponse(*id, handleHover(it->second, pos.line, pos.column));
            return;
        }
        if (method == "textDocument/semanticTokens/full") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            auto it = docs_.find(uri);
            if (it == docs_.end()) {
                sendResponse(*id, JsonValue::object({{"data", JsonValue::array({})}}));
                return;
            }
            sendResponse(*id, buildSemanticTokensData(it->second.text));
            return;
        }
        if (method == "textDocument/documentSymbol") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            auto it = docs_.find(uri);
            if (it == docs_.end()) {
                sendResponse(*id, JsonValue::array({}));
                return;
            }
            sendResponse(*id, handleDocumentSymbol(it->second));
            return;
        }
        if (method == "workspace/symbol") {
            std::string query = asString(params ? objGet(*params, "query") : nullptr);
            sendResponse(*id, handleWorkspaceSymbol(query));
            return;
        }

        sendError(*id, -32601, "Method not found");
    }

    void handleNotification(const JsonValue& req) {
        std::string method = asString(objGet(req, "method"));
        const JsonValue* params = objGet(req, "params");

        if (method == "initialized" || method == "$/cancelRequest") return;
        if (method == "exit") {
            exitNow_ = true;
            return;
        }
        if (method == "textDocument/didOpen") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            if (!td) return;
            DocumentState doc;
            doc.uri = asString(objGet(*td, "uri"));
            doc.path = uriToPath(doc.uri);
            doc.text = asString(objGet(*td, "text"));
            doc.version = asInt(objGet(*td, "version"), 0);
            docs_[doc.uri] = doc;
            publishDiagnosticsFor(docs_[doc.uri]);
            return;
        }
        if (method == "textDocument/didChange") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            auto it = docs_.find(uri);
            if (it == docs_.end()) return;
            if (td) it->second.version = asInt(objGet(*td, "version"), it->second.version);
            const JsonValue* changes = params ? objGet(*params, "contentChanges") : nullptr;
            if (changes && changes->type == JsonValue::Type::Array && !changes->arrayValue.empty()) {
                const JsonValue& first = changes->arrayValue.front();
                const JsonValue* text = objGet(first, "text");
                if (text && text->type == JsonValue::Type::String) {
                    it->second.text = text->stringValue; // full sync
                }
            }
            publishDiagnosticsFor(it->second);
            return;
        }
        if (method == "textDocument/didClose") {
            const JsonValue* td = params ? objGet(*params, "textDocument") : nullptr;
            std::string uri = asString(td ? objGet(*td, "uri") : nullptr);
            docs_.erase(uri);
            sendNotification("textDocument/publishDiagnostics", JsonValue::object({
                {"uri", JsonValue::string(uri)},
                {"diagnostics", JsonValue::array({})}
            }));
            return;
        }
    }

    void handleMessage(const JsonValue& req) {
        if (req.type != JsonValue::Type::Object) return;
        const JsonValue* id = objGet(req, "id");
        if (id) handleRequest(req);
        else handleNotification(req);
    }
};

} // namespace

void runKernLsp() {
    KernLspServer server;
    server.run();
}

} // namespace kern

int main() {
    kern::runKernLsp();
    return 0;
}

