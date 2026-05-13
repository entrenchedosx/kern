#ifndef KERN_ANALYZER_PROJECT_ANALYZER_HPP
#define KERN_ANALYZER_PROJECT_ANALYZER_HPP

#include <string>
#include <vector>

namespace kern {

enum class IssueSeverity {
    Critical,
    Warning,
    Info
};

struct AnalyzerIssue {
    IssueSeverity severity = IssueSeverity::Info;
    std::string file;
    int line = 1;
    int column = 1;
    std::string type;
    std::string message;
    std::string fix;
    bool autoFixable = false;
    bool uncertain = false;
};

struct AnalyzerOptions {
    std::string projectRoot = ".";
    bool applyFixes = false;
    bool dryRun = false;
    bool includeRuntimeHeuristics = true;
};

struct AnalyzerFixChange {
    std::string file;
    std::string reason;
};

struct AnalyzerReport {
    std::string projectRoot;
    std::vector<std::string> kernFiles;
    std::vector<std::string> configFiles;
    std::vector<AnalyzerIssue> issues;
    std::vector<AnalyzerFixChange> appliedChanges;
    std::vector<AnalyzerFixChange> pendingReviewChanges;
    std::string backupRoot;
    int criticalCount = 0;
    int warningCount = 0;
    int infoCount = 0;
};

AnalyzerReport analyzeProjectAndMaybeFix(const AnalyzerOptions& options);
bool undoFixesFromBackup(const std::string& backupPath, std::string& error);
std::string analyzerReportToJson(const AnalyzerReport& report);
std::string severityToString(IssueSeverity s);

} // namespace kern

#endif // kERN_ANALYZER_PROJECT_ANALYZER_HPP
