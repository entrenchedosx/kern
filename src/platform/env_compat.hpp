/* *
 * Portable getenv for first-party code. MSVC marks getenv deprecated (C4996); this keeps semantics
 * identical while silencing the warning in one place.
 */
#ifndef KERN_PLATFORM_ENV_COMPAT_HPP
#define KERN_PLATFORM_ENV_COMPAT_HPP

#include <cstdlib>

namespace kern {

inline const char* kernGetEnv(const char* name) noexcept {
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    const char* r = std::getenv(name);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    return r;
}

} // namespace kern

#endif
