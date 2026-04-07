/* *
 * kern import resolution – shared by main and REPL.
 * registers __import builtin that resolves: game, g2d, process, stdlib, and file-based modules.
 */
#ifndef KERN_IMPORT_RESOLUTION_HPP
#define KERN_IMPORT_RESOLUTION_HPP

#include "vm/vm.hpp"
#include "bytecode/value.hpp"
#include <functional>
#include <string>

namespace kern {

/* * builtin index used for __import (must not clash with other builtins).*/
constexpr size_t IMPORT_BUILTIN_INDEX = 200;

/* *
 * register the __import builtin on the given VM.
 * call once after registerAllBuiltins (and optionally registerGameBuiltins).
 */
void registerImportBuiltin(VM& vm);

using EmbeddedModuleProvider = std::function<bool(const std::string& requestPath, std::string& sourceOut, std::string* logicalPathOut)>;
void setEmbeddedModuleProvider(EmbeddedModuleProvider provider);
void clearEmbeddedModuleProvider();

} // namespace kern

#endif
