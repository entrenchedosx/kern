/* *
 * Shared limits for call-stack tracebacks (human output, JSON, builtins, VM-injected maps).
 * Keeps deep recursion from producing unbounded strings or Value arrays.
 */

#ifndef KERN_TRACEBACK_LIMITS_HPP
#define KERN_TRACEBACK_LIMITS_HPP

#include <cstddef>

namespace kern {

inline constexpr size_t kTracebackMaxFramesPrinted = 32;
inline constexpr size_t kTracebackHeadFrames = 16;
inline constexpr size_t kTracebackTailFrames = 8;

} // namespace kern

#endif
