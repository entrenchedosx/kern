#pragma once

#ifdef _WIN32
namespace kern::win32 {

// One-time (per user) Windows setup: .kn → kern.exe, optional kern_logo.ico, Explorer refresh.
// Skips if %APPDATA%\kern\setup_done.flag exists, or env KERN_SKIP_FILE_ASSOCIATION is set.
// Failures are appended to %APPDATA%\kern\setup.log; never throws or terminates the process.
void maybeRegisterKnFileAssociation();

// Re-apply HKCU classes for .kn (ignores setup_done.flag and KERN_SKIP_FILE_ASSOCIATION).
// Writes flag on success; prints a short message to stdout/stderr; safe to call from CMD.
void repairKnFileAssociation();

} // namespace kern::win32
#endif
