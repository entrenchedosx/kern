/* *
 * Normalize raw file bytes before lexing (BOM / encoding pitfalls on Windows).
 */

#ifndef KERN_SOURCE_ENCODING_HPP
#define KERN_SOURCE_ENCODING_HPP

#include <cstddef>
#include <string>

namespace kern {

inline void stripUtf8Bom(std::string& s) {
    if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
        static_cast<unsigned char>(s[2]) == 0xBF) {
        s.erase(0, 3);
    }
}

inline bool hasUtf16Bom(const std::string& s) {
    if (s.size() < 2) return false;
    unsigned char a = static_cast<unsigned char>(s[0]);
    unsigned char b = static_cast<unsigned char>(s[1]);
    return (a == 0xFF && b == 0xFE) || (a == 0xFE && b == 0xFF);
}

} // namespace kern

#endif
