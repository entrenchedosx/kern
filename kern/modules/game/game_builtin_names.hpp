/* *
 * Thin header: game builtin identifiers for static analysis (no VM value types).
 */
#ifndef KERN_GAME_BUILTIN_NAMES_HPP
#define KERN_GAME_BUILTIN_NAMES_HPP

#include <string>
#include <vector>

namespace kern {
std::vector<std::string> getGameBuiltinNames();
}

#endif
