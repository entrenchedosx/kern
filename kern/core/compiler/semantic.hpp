#ifndef KERN_COMPILER_SEMANTIC_HPP
#define KERN_COMPILER_SEMANTIC_HPP

#include <string>
#include <unordered_map>
#include <vector>

namespace kern {

enum class SemanticSeverity {
    Info,
    Warning,
    Error
};

struct SemanticDiagnostic {
    SemanticSeverity severity = SemanticSeverity::Info;
    std::string file;
    int line = 1;
    int column = 1;
    std::string code;
    std::string message;
};

struct SemanticResult {
    std::vector<SemanticDiagnostic> diagnostics;
    bool hasError = false;
};

SemanticResult analyzeSemanticSource(const std::string& source, const std::string& filePath, bool strictTypes);

/* * emit semantic diagnostics through g_errorReporter (warnings + type errors). Returns true if any Error-level diagnostic.*/
bool applySemanticDiagnosticsToReporter(const std::string& source, const std::string& filePath, bool strictTypes);

const char* semanticSeverityName(SemanticSeverity s);

} // namespace kern

#endif // kERN_COMPILER_SEMANTIC_HPP
