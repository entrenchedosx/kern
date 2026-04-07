/* *
 * kern Advanced Error Handling - Implementation
 */

#include "errors/errors.hpp"
#include "diagnostics/traceback_limits.hpp"
#include "platform/env_compat.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

namespace kern {

ErrorReporter g_errorReporter;

// Implements: humanizePathForDisplay-001 (see errors.hpp).
std::string humanizePathForDisplay(const std::string& path) {
    // ----------------------------------------------------------------------------
    // Display contract — authoritative spec: errors.hpp (humanizePathForDisplay).
    // • First match wins (same precedence as the header).
    // • Order is part of the public contract.
    // • Do not reorder the following checks without updating errors.hpp.
    // ----------------------------------------------------------------------------
    if (path.empty()) return "<unknown>";
    if (path == "<repl>") return "<repl> (interactive)";
    return path;
}

static std::string escapeJson(const std::string& s) {
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
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

static bool stderrIsTty() {
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(STDERR_FILENO) != 0;
#endif
}

ErrorReporter::ErrorReporter() {
    const char* noColor = kernGetEnv("NO_COLOR");
    useColors_ = (noColor == nullptr || noColor[0] == '\0') && stderrIsTty();
}

void ErrorReporter::setSource(const std::string& source) {
    sourceLines_.clear();
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) sourceLines_.push_back(line);
}

ErrorReporterImportScope::ErrorReporterImportScope(ErrorReporter& r, std::string path, const std::string& source)
    : rep_(r), prevFilename_(r.getFilename()), prevLines_(r.getSourceLines()) {
    rep_.setFilename(std::move(path));
    rep_.setSource(source);
}

ErrorReporterImportScope::~ErrorReporterImportScope() {
    rep_.setFilename(std::move(prevFilename_));
    rep_.setSourceLines(std::move(prevLines_));
}

std::string ErrorReporter::getLineSnippet(int line, int column, int contextLines, int lineEnd, int columnEnd) const {
    SourceSpan span = normalizeSourceSpan(line, column, lineEnd, columnEnd);
    if (sourceLines_.empty() || span.line < 1 || span.line > static_cast<int>(sourceLines_.size()))
        return "";
    std::ostringstream out;
    int start = std::max(1, span.line - contextLines);
    int end = std::min(static_cast<int>(sourceLines_.size()), span.lineEnd + contextLines);
    const int numWidth = 4;
    for (int i = start; i <= end; ++i) {
        std::string num = std::to_string(i);
        while (static_cast<int>(num.size()) < numWidth) num = " " + num;
        out << colorDim(" " + num + " | ");
        out << sourceLines_[static_cast<size_t>(i - 1)];
        if (i >= span.line && i <= span.lineEnd && span.column >= 1) {
            const std::string& srcLine = sourceLines_[static_cast<size_t>(i - 1)];
            int maxCol = std::max(1, static_cast<int>(srcLine.size()));
            int startCol = 1;
            int endCol = maxCol;
            if (span.line == span.lineEnd) {
                startCol = span.column;
                endCol = span.columnEnd;
            } else if (i == span.line) {
                startCol = span.column;
                endCol = maxCol;
            } else if (i == span.lineEnd) {
                startCol = 1;
                endCol = span.columnEnd;
            }
            int spanStart = std::clamp(startCol, 1, maxCol);
            int spanStop = std::clamp(endCol, spanStart, maxCol);
            out << "\n" << colorDim(std::string(numWidth + 2, ' ') + "| ");
            for (int c = 1; c < spanStart; ++c) out << " ";
            int paint = spanStop - spanStart + 1;
            if (paint > 1) {
                for (int k = 0; k < paint - 1; ++k) out << colorRed("~");
                out << colorRed("^");
            } else {
                out << colorRed("^");
            }
        }
        if (i < end) out << "\n";
    }
    return out.str();
}

std::string ErrorReporter::colorRed(const std::string& s) const {
    if (!useColors_) return s;
    return "\033[1;31m" + s + "\033[0m";
}

std::string ErrorReporter::colorYellow(const std::string& s) const {
    if (!useColors_) return s;
    return "\033[1;33m" + s + "\033[0m";
}

std::string ErrorReporter::colorBlue(const std::string& s) const {
    if (!useColors_) return s;
    return "\033[1;34m" + s + "\033[0m";
}

std::string ErrorReporter::colorCyan(const std::string& s) const {
    if (!useColors_) return s;
    return "\033[36m" + s + "\033[0m";
}

std::string ErrorReporter::colorGreen(const std::string& s) const {
    if (!useColors_) return s;
    return "\033[1;32m" + s + "\033[0m";
}

std::string ErrorReporter::colorDim(const std::string& s) const {
    if (!useColors_) return s;
    return "\033[2m" + s + "\033[0m";
}

void ErrorReporter::printDetailLines(std::ostream& out, const std::string& text) const {
    if (text.empty()) return;
    std::istringstream in(text);
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            out << colorDim("  Note: ") << line << "\n";
            first = false;
        } else {
            out << colorDim("        ") << line << "\n";
        }
    }
}

void ErrorReporter::print(const ReportedItem& item) const {
    std::ostream& out = std::cerr;
    const std::string filePartRaw = item.filename.empty() ? "<repl>" : item.filename;
    const std::string filePart = humanizePathForDisplay(filePartRaw);

    if (item.stackTrace.empty()) {
        // compile-time / single location (C++/GCC + Python style) -----
        out << colorGreen(filePart);
        if (item.line > 0) out << colorCyan(":" + std::to_string(item.line) + ":" + std::to_string(item.column));
        out << colorDim(": ");
        if (item.isWarning) {
            out << colorYellow("warning: ");
        } else {
            out << colorRed(categoryName(item.category)) << ": ";
        }
        out << item.message << "\n";

        if (!item.errorCode.empty())
            out << colorDim("  Code: ") << colorCyan(item.errorCode) << "\n";

        if (!item.codeSnippet.empty()) {
            out << "\n" << item.codeSnippet << "\n";
        }

        printDetailLines(out, item.detail);
    } else {
        // runtime: Python-style traceback + per-frame source when available -----
        out << colorBlue("Traceback (most recent call last):") << "\n";
        const auto& st = item.stackTrace;
        const size_t n = st.size();
        auto printFrame = [&](size_t i, bool withSnippet) {
            const auto& f = st[i];
            const std::string frameRaw = !f.filePath.empty() ? f.filePath : filePartRaw;
            const std::string frameFile = humanizePathForDisplay(frameRaw);
            const bool snippetFromMainSource =
                f.filePath.empty() || f.filePath == item.filename;
            out << colorDim("  File ");
            out << "\"" << colorGreen(frameFile) << "\"";
            out << colorDim(", line ") << colorCyan(std::to_string(f.line));
            out << colorDim(", in ") << (f.functionName.empty() ? "<module>" : f.functionName);
            out << "\n";

            if (!withSnippet) return;

            int col = f.column > 0 ? f.column : 1;
            if (snippetFromMainSource && f.line > 0 && !sourceLines_.empty()) {
                std::string frameSnip = getLineSnippet(f.line, col, 0, f.line, col);
                if (!frameSnip.empty()) {
                    out << "\n" << frameSnip << "\n";
                }
            } else if (i == 0 && item.line > 0 && !sourceLines_.empty() &&
                       item.line >= 1 && item.line <= static_cast<int>(sourceLines_.size())) {
                const std::string& ln = sourceLines_[static_cast<size_t>(item.line - 1)];
                out << colorDim("    ") << ln << "\n";
                if (item.column >= 1) {
                    out << colorDim("    ");
                    for (int c = 1; c < item.column; ++c) out << " ";
                    out << colorRed("^") << "\n";
                }
            }
        };

        if (n <= kTracebackMaxFramesPrinted) {
            for (size_t i = 0; i < n; ++i) printFrame(i, true);
        } else {
            // First head frame only gets source snippet (rest are usually redundant repeats).
            for (size_t i = 0; i < kTracebackHeadFrames; ++i) printFrame(i, i == 0);
            const size_t omitted = n - kTracebackHeadFrames - kTracebackTailFrames;
            out << colorDim("  ... ") << omitted << colorDim(" stack frame(s) omitted (deep call stack) ...\n");
            // Last tail frame: show snippet so the failing line is visible.
            for (size_t i = n - kTracebackTailFrames; i < n; ++i) printFrame(i, i + 1 == n);
        }
        out << colorRed(categoryName(item.category)) << ": " << item.message << "\n";

        if (!item.errorCode.empty())
            out << colorDim("  Code: ") << colorCyan(item.errorCode) << "\n";

        if (!item.codeSnippet.empty() && item.stackTrace.size() <= 1) {
            out << "\n" << item.codeSnippet << "\n";
        }

        printDetailLines(out, item.detail);
    }

    if (!item.hint.empty()) {
        out << colorYellow("  Hint: ") << item.hint << "\n";
    }
}

void ErrorReporter::reportCompileError(ErrorCategory cat, int line, int column,
                                       const std::string& message,
                                       const std::string& hint,
                                       const std::string& errorCode,
                                       const std::string& detail) {
    errorCount_++;
    ReportedItem item;
    item.category = cat;
    item.message = message;
    item.filename = filename_;
    SourceSpan span = normalizeSourceSpan(line, column, line, column);
    item.line = span.line;
    item.column = span.column;
    item.lineEnd = span.lineEnd;
    item.columnEnd = span.columnEnd;
    item.hint = hint;
    item.errorCode = errorCode;
    item.detail = detail;
    item.codeSnippet = getLineSnippet(item.line, item.column, 3, item.lineEnd, item.columnEnd);
    items_.push_back(item);
    if (!suppressHumanItemPrint_) print(item);
}

void ErrorReporter::reportRuntimeError(ErrorCategory cat, int line, int column,
                                       const std::string& message,
                                       const std::vector<StackFrame>& stack,
                                       const std::string& hint,
                                       const std::string& errorCode,
                                       const std::string& detail,
                                       int lineEnd,
                                       int columnEnd) {
    errorCount_++;
    ReportedItem item;
    item.category = cat;
    item.message = message;
    item.filename = filename_;
    SourceSpan span = normalizeSourceSpan(line, column, lineEnd, columnEnd);
    item.line = span.line;
    item.column = span.column;
    item.lineEnd = span.lineEnd;
    item.columnEnd = span.columnEnd;
    item.stackTrace = stack;
    item.hint = hint;
    item.errorCode = errorCode;
    item.detail = detail;
    item.codeSnippet = getLineSnippet(item.line, item.column, 3, item.lineEnd, item.columnEnd);
    items_.push_back(item);
    if (!suppressHumanItemPrint_) print(item);
}

void ErrorReporter::reportWarning(int line, int column, const std::string& message,
                                const std::string& hint,
                                const std::string& errorCode,
                                const std::string& detail) {
    warningCount_++;
    ReportedItem item;
    item.category = ErrorCategory::Other;
    item.message = message;
    item.filename = filename_;
    SourceSpan span = normalizeSourceSpan(line, column, line, column);
    item.line = span.line;
    item.column = span.column;
    item.lineEnd = span.lineEnd;
    item.columnEnd = span.columnEnd;
    item.isWarning = true;
    item.hint = hint;
    item.errorCode = errorCode;
    item.detail = detail;
    item.codeSnippet = getLineSnippet(item.line, item.column, 3, item.lineEnd, item.columnEnd);
    items_.push_back(item);
    if (!suppressHumanItemPrint_) print(item);
}

std::string ErrorReporter::toJson() const {
    std::ostringstream out;
    out << "{";
    out << "\"errors\":" << errorCount_ << ",";
    out << "\"warnings\":" << warningCount_ << ",";
    out << "\"items\":[";
    for (size_t i = 0; i < items_.size(); ++i) {
        const auto& it = items_[i];
        if (i) out << ",";
        out << "{";
        out << "\"kind\":\"" << (it.isWarning ? "warning" : "error") << "\",";
        out << "\"category\":\"" << categoryName(it.category) << "\",";
        out << "\"message\":\"" << escapeJson(it.message) << "\",";
        out << "\"filename\":\"" << escapeJson(it.filename) << "\",";
        out << "\"line\":" << it.line << ",";
        out << "\"column\":" << it.column << ",";
        out << "\"lineEnd\":" << it.lineEnd << ",";
        out << "\"columnEnd\":" << it.columnEnd << ",";
        out << "\"range\":{"
            << "\"start\":{\"line\":" << it.line << ",\"column\":" << it.column << "},"
            << "\"end\":{\"line\":" << it.lineEnd << ",\"column\":" << it.columnEnd << "}"
            << "},";
        out << "\"hint\":\"" << escapeJson(it.hint) << "\",";
        out << "\"code\":\"" << escapeJson(it.errorCode) << "\",";
        out << "\"detail\":\"" << escapeJson(it.detail) << "\"";
        if (!it.stackTrace.empty()) {
            const auto& st = it.stackTrace;
            const size_t n = st.size();
            out << ",\"stack\":[";
            if (n <= kTracebackMaxFramesPrinted) {
                for (size_t j = 0; j < n; ++j) {
                    const auto& sf = st[j];
                    if (j) out << ",";
                    out << "{\"function\":\"" << escapeJson(sf.functionName) << "\","
                        << "\"line\":" << sf.line << ",\"column\":" << sf.column << ","
                        << "\"filename\":\"" << escapeJson(!sf.filePath.empty() ? sf.filePath : it.filename) << "\"}";
                }
            } else {
                size_t jout = 0;
                for (size_t j = 0; j < kTracebackHeadFrames; ++j, ++jout) {
                    if (jout) out << ",";
                    const auto& sf = st[j];
                    out << "{\"function\":\"" << escapeJson(sf.functionName) << "\","
                        << "\"line\":" << sf.line << ",\"column\":" << sf.column << ","
                        << "\"filename\":\"" << escapeJson(!sf.filePath.empty() ? sf.filePath : it.filename) << "\"}";
                }
                out << ",{\"_truncated\":true,\"omitted\":" << (n - kTracebackHeadFrames - kTracebackTailFrames) << "}";
                for (size_t j = n - kTracebackTailFrames; j < n; ++j) {
                    out << ",";
                    const auto& sf = st[j];
                    out << "{\"function\":\"" << escapeJson(sf.functionName) << "\","
                        << "\"line\":" << sf.line << ",\"column\":" << sf.column << ","
                        << "\"filename\":\"" << escapeJson(!sf.filePath.empty() ? sf.filePath : it.filename) << "\"}";
                }
            }
            out << "]";
        }
        out << "}";
    }
    out << "]";
    out << "}";
    return out.str();
}

void ErrorReporter::printSummary() const {
    if (errorCount_ == 0 && warningCount_ == 0) return;
    std::cerr << "\n";
    std::cerr << colorDim("--- ");
    if (errorCount_ > 0) std::cerr << colorRed(std::to_string(errorCount_) + " error(s)");
    if (errorCount_ > 0 && warningCount_ > 0) std::cerr << colorDim(", ");
    if (warningCount_ > 0) std::cerr << colorYellow(std::to_string(warningCount_) + " warning(s)");
    std::cerr << colorDim(" ---") << "\n";
    if (errorCount_ > 0) {
        std::cerr << colorDim(
            "Each error lists a Code (stable id), optional multi-line Note, and a short Hint. "
            "Use `kern --check --json <file>` for machine-readable diagnostics.\n");
    }
}

} // namespace kern
