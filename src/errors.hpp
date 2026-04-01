/* *
 * kern Advanced Error Handling
 * categories, stack traces, colored output, source snippets, summary.
 */

#ifndef KERN_ERRORS_HPP
#define KERN_ERRORS_HPP

#include <iosfwd>
#include <string>
#include <vector>
#include <cstddef>
#include "diagnostics/source_span.hpp"
#include "vm/vm_error_codes.hpp"
#include "vm/vm_error_registry.hpp"

namespace kern {

// error categories -----
enum class ErrorCategory {
    SyntaxError,
    RuntimeError,
    TypeError,
    ValueError,
    FileError,
    ReferenceError,
    ArgumentError,
    IndexError,
    DivisionError,
    Other
};

inline const char* categoryName(ErrorCategory c) {
    switch (c) {
        case ErrorCategory::SyntaxError:    return "SyntaxError";
        case ErrorCategory::RuntimeError:   return "RuntimeError";
        case ErrorCategory::TypeError:      return "TypeError";
        case ErrorCategory::ValueError:     return "ValueError";
        case ErrorCategory::FileError:      return "FileError";
        case ErrorCategory::ReferenceError: return "ReferenceError";
        case ErrorCategory::ArgumentError:  return "ArgumentError";
        case ErrorCategory::IndexError:    return "IndexError";
        case ErrorCategory::DivisionError:  return "DivisionError";
        default: return "Error";
    }
}

/* * map VM error category (int) to ErrorCategory. Used by main/repl/game when catching VMError.*/
inline ErrorCategory vmErrorCategory(int cat) {
    switch (cat) {
        case 1: return ErrorCategory::RuntimeError;
        case 2: return ErrorCategory::TypeError;
        case 3: return ErrorCategory::ValueError;
        case 4: return ErrorCategory::DivisionError;
        case 5: return ErrorCategory::ArgumentError;
        case 6: return ErrorCategory::IndexError;
        case 7: return ErrorCategory::RuntimeError;
        case 8: return ErrorCategory::FileError;
        default: return ErrorCategory::RuntimeError;
    }
}

/* * hint text after LexerError (shared by kern CLI, REPL, --check, --ast).*/
inline const char* lexerCompileErrorHint() {
    return "Check for unclosed strings, invalid characters, or number format.";
}

/* * context-specific hint after ParserError; always returns a non-empty suggestion.*/
inline std::string parserCompileErrorHint(const std::string& msg) {
    if (msg.find("Expected expression") != std::string::npos)
        return "Did you forget an expression or use an invalid token?";
    if (msg.find("')'") != std::string::npos) return "Did you forget a closing parenthesis?";
    if (msg.find("'}'") != std::string::npos) return "Did you forget a closing brace?";
    if (msg.find("'return' is only allowed") != std::string::npos)
        return "At module or class body scope use if/else (or wrap logic in a def), not return.";
    return "Check syntax (parentheses, braces, commas).";
}

/* * non-empty only for common VMError::category values (CLI, REPL, import diagnostics).*/
inline const char* vmRuntimeErrorHint(int category, int code = static_cast<int>(VMErrorCode::NONE)) {
    if (const VMErrorMeta* meta = findVMErrorMeta(code))
        return meta->hint;
    switch (category) {
        case 2: return "Check the types of operands (e.g. cannot call a non-function).";
        case 4: return "Division by zero is undefined.";
        case 5: return "Check the number of arguments passed to the function.";
        case 6: return "Check array/string bounds and index types.";
        case 7: return "BrowserKit rejected this operation. Check runtime mode, platform support, and bridge dependencies.";
        case 8: return "Import failed hard. Check module path, permissions, cycles, and import diagnostics.";
        default: return "";
    }
}

/* * stable short code for tooling / JSON (e.g. VM-DIV, VM-ARG). Category 0/1 map to VM-RUN.*/
inline std::string vmErrorCodeString(int category, int code = static_cast<int>(VMErrorCode::NONE)) {
    if (const VMErrorMeta* meta = findVMErrorMeta(code))
        return meta->stableCode;
    switch (category) {
        case 2: return "VM-TYPE";
        case 3: return "VM-VALUE";
        case 4: return "VM-DIV";
        case 5: return "VM-ARG";
        case 6: return "VM-INDEX";
        case 7: return "VM-BROWSERKIT";
        case 8: return "VM-IMPORT";
        default: return "VM-RUN";
    }
}

/* * long-form explanation for VM failures (stderr “Note” block).*/
inline std::string vmRuntimeErrorDetail(int category, int code = static_cast<int>(VMErrorCode::NONE)) {
    if (const VMErrorMeta* meta = findVMErrorMeta(code))
        return meta->detail;
    switch (category) {
        case 2:
            return "The VM expected a value of a different kind for this operation (for example: calling something "
                   "that is not a function, using an unsupported type as a callable, or mixing incompatible types). "
                   "Inspect the expression at the indicated line and the types flowing into it.";
        case 3:
            return "A value-level rule was violated (assertion, contract, or builtin precondition). "
                   "The message above usually names the failing check.";
        case 4:
            return "Integer or floating division (or modulo) with a zero divisor is not defined. "
                   "Guard the divisor with a comparison before dividing, or use a safe wrapper.";
        case 5:
            return "The number of values passed to a function does not match what the callee expects at this call site. "
                   "Count arguments at the call, default parameters, and variadic/spread usage.";
        case 6:
            return "An index was out of range, or an index could not be applied to this container type. "
                   "Validate length (len / array bounds) before indexing; negative indices wrap from the end.";
        case 7:
            return "BrowserKit rejected this operation by policy or capability checks. "
                   "Typical causes are sandbox-restricted network/OS bridges, unsupported protocol handlers, "
                   "or missing external dependencies (for example websocat/curl).";
        case 8:
            return "Import execution failed after diagnostics were emitted. "
                   "This hard-fail path prevents partial module execution and nil-based fallback behavior. "
                   "Review earlier IMP-* diagnostics (resolve/read/cycle/internal) to fix the root cause.";
        default:
            return "Execution stopped inside the VM. If the message is unclear, reduce the script to a few lines "
                   "that still reproduce the problem and check nearby control flow (loops, try/catch, generators).";
    }
}

inline std::string lexerCompileErrorDetail() {
    return "Tokenization failed before parsing could start. Typical causes:\n"
           "  • A string literal is missing its closing quote, or uses an invalid escape.\n"
           "  • A numeric literal is malformed or exceeds what the implementation can represent.\n"
           "  • A stray byte or character is not valid in Kern source (encoding or copy-paste artifact).\n"
           "Fix the reported line first; later errors often disappear once the lexer can scan past it.";
}

inline std::string parserCompileErrorDetail(const std::string& msg) {
    std::string out =
        "The parser could not build an abstract syntax tree from the token stream. The caret points at the first "
        "token that does not fit the grammar at that position.\n";
    if (msg.find("'return' is only allowed") != std::string::npos) {
        out += "  • `return` only ends the current function body. At file or class scope, structure control flow "
               "with if/else, or move logic into a `def`.\n";
    }
    out += "  • Match delimiters: ( ), { }, [ ].\n"
           "  • Check commas between arguments, and that statements end cleanly before the next top-level construct.";
    return out;
}

inline std::string fileOpenErrorDetail() {
    return "The runtime could not open this path for reading. Confirm:\n"
           "  • The path is spelled correctly (case-sensitive on some platforms).\n"
           "  • You are using the intended working directory (or pass a path relative to the script).\n"
           "  • The file exists and your user has read permission.";
}

inline std::string internalFailureDetail(const std::string& context) {
    return "An unexpected C++ exception escaped from " + context + ". This is not a normal Kern language error.\n"
           "If you can reproduce it, capture the smallest .kn file and the exact command line.";
}

/* * post-codegen heuristic: LOAD_GLOBAL without STORE_GLOBAL in the same unit.*/
inline const char* undefinedGlobalLoadWarningHint() {
    return "Define or assign the name earlier, import a module that defines it, or confirm it is a builtin.";
}

inline std::string undefinedGlobalLoadWarningDetail() {
    return "Static scan of emitted bytecode: a global read appears without a matching global store in this compilation unit.\n"
           "The name may still be valid if it comes from the standard library, a native builtin, import side effects, "
           "or another chunk; treat this as a hint, not a hard guarantee.";
}

inline const char* importRaylibModuleHint() {
    return "Install Raylib (e.g. vcpkg install raylib), reconfigure with KERN_BUILD_GAME, and rebuild Kern.";
}

inline std::string importModuleUnavailableDetail(const std::string& logicalName) {
    return "The module \"" + logicalName + "\" is linked only when Kern is built with Raylib (KERN_BUILD_GAME).\n"
           "This binary does not include that native layer; import fails loudly with a runtime error.\n"
           "Guard optional graphics imports, or use a graphics-enabled build artifact.";
}

inline const char* importResolveFailureHint() {
    return "Confirm CWD, KERN_LIB, spelling, and that the file exists under an allowed root.";
}

inline std::string importResolveFailureDetail() {
    return "Filesystem import resolution searches the process current working directory and KERN_LIB (when set).\n"
           "Path traversal (..) is rejected. Absolute paths must lie under those roots.\n"
           "Bare module names receive a .kn suffix automatically.";
}

inline std::string importCycleDetail() {
    return "A module was requested while it was still loading (recursive import).\n"
           "Split shared definitions into a separate module, or reorder imports so the dependency graph is acyclic.";
}

inline std::string importWhileLoadingDetail(const std::string& modulePath) {
    return "While loading \"" + modulePath + "\": " + importCycleDetail();
}

inline std::string importLexDetail(const std::string& modulePath) {
    return "Tokenization failed inside an imported module. Fix this file first; importers often cascade errors.\n"
           "Module: \"" + modulePath + "\".\n" +
           lexerCompileErrorDetail();
}

inline std::string importParseDetail(const std::string& modulePath, const std::string& parserMsg) {
    return "Parse failure inside imported module \"" + modulePath + "\".\n" + parserCompileErrorDetail(parserMsg);
}

inline std::string importVmDetail(const std::string& modulePath) {
    return "The VM raised an error while executing the top-level body of imported module \"" + modulePath + "\".\n"
           "Top-level side effects in modules run at import time.";
}

inline std::string importEmbeddedFailureDetail(const std::string& modulePath) {
    return "Embedded/bundled module \"" + modulePath + "\" failed to compile or run during import.\n"
           "Verify the embedded source matches what the packager shipped.";
}

// stack frame (for runtime) -----
struct StackFrame {
    std::string functionName;  // "main", "<lambda>", "foo", etc.
    int line = 0;
    int column = 0;
};

// single error or warning -----
struct ReportedItem {
    ErrorCategory category = ErrorCategory::Other;
    std::string message;
    std::string filename;   // empty = stdin/REPL
    int line = 0;
    int column = 0;
    int lineEnd = 0;
    int columnEnd = 0;      // optional span for underline (0 = single caret)
    std::vector<StackFrame> stackTrace;
    std::string codeSnippet;  // line of code with optional caret/underline
    std::string hint;        // short “Did you forget …?” line
    std::string errorCode;  // stable id: VM-DIV, LEX-TOKENIZE, FILE-OPEN, …
    std::string detail;     // multi-line long explanation (optional)
    bool isWarning = false;
};

// global error reporter (counts + optional colors) -----
class ErrorReporter {
public:
    ErrorReporter();
    void setUseColors(bool use) { useColors_ = use; }
    /* * when true, report* still records items for toJson() but does not print each item to stderr (IDE --check --json).*/
    void setSuppressHumanItemPrint(bool suppress) { suppressHumanItemPrint_ = suppress; }
    bool suppressHumanItemPrint() const { return suppressHumanItemPrint_; }
    void setSourceLines(std::vector<std::string> lines) { sourceLines_ = std::move(lines); }
    void setFilename(const std::string& name) { filename_ = name; }
    void setSource(const std::string& source);  // splits into lines

    const std::string& getFilename() const { return filename_; }
    const std::vector<std::string>& getSourceLines() const { return sourceLines_; }

    // report compile-time (lexer/parser) error
    void reportCompileError(ErrorCategory cat, int line, int column,
                           const std::string& message,
                           const std::string& hint = "",
                           const std::string& errorCode = "",
                           const std::string& detail = "");

    // report runtime error with optional stack
    void reportRuntimeError(ErrorCategory cat, int line, int column,
                           const std::string& message,
                           const std::vector<StackFrame>& stack = {},
                           const std::string& hint = "",
                           const std::string& errorCode = "",
                           const std::string& detail = "",
                           int lineEnd = 0,
                           int columnEnd = 0);

    void reportWarning(int line, int column, const std::string& message,
                      const std::string& hint = "",
                      const std::string& errorCode = "",
                      const std::string& detail = "");

    // print to stderr with colors and optional snippet
    void print(const ReportedItem& item) const;

    int errorCount() const { return errorCount_; }
    int warningCount() const { return warningCount_; }
    void resetCounts() { errorCount_ = 0; warningCount_ = 0; items_.clear(); }
    void printSummary() const;
    const std::vector<ReportedItem>& getItems() const { return items_; }
    std::string toJson() const;

private:
    bool useColors_ = true;
    bool suppressHumanItemPrint_ = false;
    std::string filename_;
    std::vector<std::string> sourceLines_;
    int errorCount_ = 0;
    int warningCount_ = 0;
    std::vector<ReportedItem> items_;

    std::string getLineSnippet(int line, int column, int contextLines = 3, int lineEnd = 0, int columnEnd = 0) const;
    std::string colorRed(const std::string& s) const;
    std::string colorYellow(const std::string& s) const;
    std::string colorBlue(const std::string& s) const;
    std::string colorCyan(const std::string& s) const;
    std::string colorGreen(const std::string& s) const;
    std::string colorDim(const std::string& s) const;
    void printDetailLines(std::ostream& out, const std::string& text) const;
};

/* *
 * temporarily point the reporter at another file's source (e.g. during import) so snippets and
 * filenames on ReportedItem match the module; restores previous filename/lines on destruction.
 */
class ErrorReporterImportScope {
public:
    ErrorReporterImportScope(ErrorReporter& r, std::string path, const std::string& source);
    ~ErrorReporterImportScope();
    ErrorReporterImportScope(const ErrorReporterImportScope&) = delete;
    ErrorReporterImportScope& operator=(const ErrorReporterImportScope&) = delete;

private:
    ErrorReporter& rep_;
    std::string prevFilename_;
    std::vector<std::string> prevLines_;
};

// global instance for use from main/repl/vm
extern ErrorReporter g_errorReporter;

} // namespace kern

#endif // kERN_ERRORS_HPP
