#ifndef KERN_COMPILER_IMPORT_ALIASES_HPP
#define KERN_COMPILER_IMPORT_ALIASES_HPP

#include <string>

namespace kern {

/* *
 * true if the import string resolves at runtime via __import (stdlib, game/g2d/g3d,
 * kern::process/input/vision/render, process/input/vision/render) and does not require a .kn file on disk.
 * used by project_resolver and static analyzer to avoid false "missing import" diagnostics.
 */
bool isVirtualResolvedImport(const std::string& rawImport);

/* * negative-test fixtures (e.g. import("__does_not_exist__")) — not an on-disk module.*/
bool isIntentionalMissingImportFixture(const std::string& rawImport);

} // namespace kern

#endif
