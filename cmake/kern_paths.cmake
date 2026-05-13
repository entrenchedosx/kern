# Central path definitions for the Kern toolchain CMake project.
# Single source of truth: change paths here when relocating trees.
#
# CORE LAYER GUARANTEE (kern/core/):
#   Compiler, bytecode, diagnostics, errors, platform — no vm/*.hpp execution internals.
#
# PIPELINE LAYER (kern/pipeline/):
#   IR, analysis, optimization passes — depends on core only; must NOT include vm/*.hpp.
#
# RUNTIME (kern/runtime/vm/): execution, builtins implementation.
#
# GLUE (src/): import/stdlib, process, system, scanner, packager, compile — until migrated (CLI: kern/tools/).

set(KERN_ROOT_DIR "${CMAKE_SOURCE_DIR}")

# ---------------------------------------------------------------------------
set(KERN_SRC_DIR "${KERN_ROOT_DIR}/src")
set(KERN_INCLUDE_DIR "${KERN_ROOT_DIR}/include")

# ---------------------------------------------------------------------------
# Core layer
# ---------------------------------------------------------------------------
set(KERN_CORE_DIR "${KERN_ROOT_DIR}/kern/core")
set(KERN_COMPILER_DIR "${KERN_CORE_DIR}/compiler")
set(KERN_CORE_BYTECODE_DIR "${KERN_CORE_DIR}/bytecode")
set(KERN_DIAGNOSTICS_DIR "${KERN_CORE_DIR}/diagnostics")
set(KERN_ERRORS_DIR "${KERN_CORE_DIR}/errors")
set(KERN_PLATFORM_DIR "${KERN_CORE_DIR}/platform")

set(KERN_CORE_INCLUDES
    ${KERN_CORE_DIR}
)

# Phase 11: shared helpers (config, build cache) — under core.
set(KERN_UTILS_DIR "${KERN_CORE_DIR}/utils")

# ---------------------------------------------------------------------------
# Pipeline layer (Phase 6): IR + project/static analysis + C++ backend (Phase 11)
# ---------------------------------------------------------------------------
set(KERN_PIPELINE_DIR "${KERN_ROOT_DIR}/kern/pipeline")
set(KERN_IR_DIR "${KERN_PIPELINE_DIR}/ir")
set(KERN_ANALYZER_DIR "${KERN_PIPELINE_DIR}/analyzer")
set(KERN_BACKEND_DIR "${KERN_PIPELINE_DIR}/backend")

set(KERN_PIPELINE_INCLUDES
    ${KERN_PIPELINE_DIR}
)

# ---------------------------------------------------------------------------
# Runtime
# ---------------------------------------------------------------------------
set(KERN_RUNTIME_DIR "${KERN_ROOT_DIR}/kern/runtime")
set(KERN_RUNTIME_SUBDIRS vm)
set(KERN_VM_DIR "${KERN_RUNTIME_DIR}/vm")

set(KERN_RUNTIME_INCLUDES
    ${KERN_RUNTIME_DIR}
)

# ---------------------------------------------------------------------------
# Modules layer (Phase 10): native VM extension modules (g2d, g3d, system/*).
# KERN_MODULES_SRC_DIR is the physical tree; today it equals KERN_MODULES_DIR (under kern/).
# ---------------------------------------------------------------------------
set(KERN_MODULES_DIR "${KERN_ROOT_DIR}/kern/modules")
set(KERN_MODULES_SRC_DIR "${KERN_MODULES_DIR}")
set(KERN_MODULES_INCLUDES
    ${KERN_MODULES_DIR}
)
# Phase 11: game module (Raylib integration, game builtins) — sibling to g2d/g3d under modules/.
set(KERN_GAME_MODULE_DIR "${KERN_MODULES_DIR}/game")

# ---------------------------------------------------------------------------
# Transitional include root for remaining src/*.cpp and src/{process,system,scanner,packager,compile}/.
# ---------------------------------------------------------------------------
set(KERN_SRC_GLUE_INCLUDES "${KERN_SRC_DIR}")

# ---------------------------------------------------------------------------
set(KERN_SHARED_INCLUDES
    ${KERN_INCLUDE_DIR}
    ${KERN_ROOT_DIR}
)

set(KERN_TOOLCHAIN_PRIVATE_INCLUDES
    ${KERN_CORE_INCLUDES}
    ${KERN_PIPELINE_INCLUDES}
    ${KERN_MODULES_INCLUDES}
    ${KERN_SRC_GLUE_INCLUDES}
    ${KERN_RUNTIME_INCLUDES}
    ${KERN_SHARED_INCLUDES}
)

# ---------------------------------------------------------------------------
set(KERN_STD_DIR "${KERN_ROOT_DIR}/kern/std")

set(KERN_EXAMPLES_DIR "${KERN_ROOT_DIR}/examples")
# Product CLI / tool entrypoints (main, kernc, REPL, LSP, kern-scan, version resource template).
set(KERN_TOOLS_DIR "${KERN_ROOT_DIR}/kern/tools")
set(KERN_CLI_DIR "${KERN_TOOLS_DIR}")
# Logical tool names (targets are defined in root CMakeLists.txt).
set(KERN_NAMED_TOOLS kern kernc kern_repl kern_lsp kern-scan kern_game)
# Repo-root helper binaries and scripts (e.g. kernc_launcher.c, release cmd).
set(KERN_REPO_TOOLS_DIR "${KERN_ROOT_DIR}/tools")
set(KERN_SCRIPTS_DIR "${KERN_ROOT_DIR}/scripts")
