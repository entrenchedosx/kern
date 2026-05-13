#ifndef KERN_DIAGNOSTICS_SOURCE_SPAN_HPP
#define KERN_DIAGNOSTICS_SOURCE_SPAN_HPP

namespace kern {

struct SourceSpan {
    int line = 0;
    int column = 0;
    int lineEnd = 0;
    int columnEnd = 0;
};

inline constexpr SourceSpan normalizeSourceSpan(int line, int column, int lineEnd, int columnEnd) {
    SourceSpan s{};
    s.line = line > 0 ? line : 0;
    s.column = column > 0 ? column : 0;
    s.lineEnd = lineEnd > 0 ? lineEnd : 0;
    s.columnEnd = columnEnd > 0 ? columnEnd : 0;
    if (s.line > 0 && s.column <= 0) s.column = 1;
    if (s.lineEnd <= 0) s.lineEnd = s.line;
    if (s.lineEnd < s.line) s.lineEnd = s.line;
    if (s.line == 0) {
        s.column = 0;
        s.lineEnd = 0;
        s.columnEnd = 0;
        return s;
    }
    // inclusive range invariant: point spans use column == columnEnd.
    if (s.columnEnd <= 0) s.columnEnd = s.column;
    if (s.lineEnd == s.line && s.columnEnd < s.column) s.columnEnd = s.column;
    return s;
}

inline constexpr bool hasFullSourceSpan(const SourceSpan& s) {
    return s.line > 0 && s.column > 0;
}

} // namespace kern

#endif // kERN_DIAGNOSTICS_SOURCE_SPAN_HPP
