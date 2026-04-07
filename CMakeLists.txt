cmake_minimum_required(VERSION 3.14)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
# Default vcpkg triplet on Windows/MSVC so Raylib matches KERN_PREFER_STATIC_RAYLIB (/MT) and standalone EXEs.
# Override with -DVCPKG_TARGET_TRIPLET=... at configure time if needed.
if(WIN32 AND MSVC)
  if(NOT VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet (static Raylib for Kern)")
  endif()
endif()
project(Kern LANGUAGES CXX)
enable_language(C)

include(cmake/kern_paths.cmake)

set(CMAKE_CXX_STANDARD 17)
if(MSVC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:10485760")
  # Large translation units (e.g. main.cpp + builtins.hpp) exceed default COFF section limit without /bigobj.
  add_compile_options(/bigobj)
endif()

# First-party targets: elevated diagnostics (third-party includes kept quiet on MSVC via /external:W0).
function(kern_apply_strict_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /external:W0)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

# Version from KERN_VERSION.txt (for --version and kern_version() builtin).
# Do not use a file named "VERSION" at repo root: on case-insensitive filesystems (macOS default)
# it shadows the C++20 standard header <version>.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/KERN_VERSION.txt")
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/KERN_VERSION.txt" KERN_VERSION_RAW)
  string(STRIP "${KERN_VERSION_RAW}" KERN_VERSION)
else()
  set(KERN_VERSION "1.0.0")
endif()
string(REPLACE "." ";" _kern_version_parts "${KERN_VERSION}")
list(LENGTH _kern_version_parts _kern_version_len)
if(_kern_version_len LESS 3)
  list(APPEND _kern_version_parts 0 0)
endif()
list(GET _kern_version_parts 0 KERN_VERSION_MAJOR)
list(GET _kern_version_parts 1 KERN_VERSION_MINOR)
list(GET _kern_version_parts 2 KERN_VERSION_PATCH)
set(KERN_VERSION_BUILD 0)

set(KERN_BUILD_ID "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE KERN_BUILD_ID
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
  endif()
endif()
# Windows PE version resource only (Apple Clang / Unix toolchains must not compile .rc).
set(KERN_WIN32_RESOURCES "")
if(WIN32)
  configure_file(
    "${KERN_CLI_DIR}/version_info.rc.in"
    "${CMAKE_BINARY_DIR}/version_info.rc"
    @ONLY
  )
  list(APPEND KERN_WIN32_RESOURCES "${CMAKE_BINARY_DIR}/version_info.rc")
endif()

# Default ON so kern.exe includes game + g2d when Raylib is available (no extra flags needed)
option(KERN_BUILD_GAME "Build kern_game and g2d into kern/kern_repl (requires Raylib)" ON)
option(KERN_PREFER_STATIC_RAYLIB "Prefer static Raylib so kern.exe has no raylib.dll dependency" ON)
option(KERN_AUTO_FETCH_RAYLIB "Auto-download/build Raylib when not found locally" ON)
option(KERN_BUILD_DOC_FRAMEWORK_DEMO "Build modular document runtime framework demo" ON)
option(KERN_DISTRIBUTION_STATIC_CRT "Build the Kern toolchain with a static MSVC runtime (/MT) for portable drops" ON)
option(KERN_WERROR "Treat compiler warnings as errors (GCC/Clang, Debug configs only)" OFF)

# For public/shareable Windows drops we want the toolchain to run with no MSVC runtime installs.
# Use /MT(/MTd) for the Kern toolchain when enabled.
if(MSVC AND KERN_DISTRIBUTION_STATIC_CRT)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(KERN_WERROR AND NOT MSVC)
  add_compile_options($<$<CONFIG:Debug>:-Werror>)
endif()

if(KERN_BUILD_DOC_FRAMEWORK_DEMO)
  find_package(SDL2 CONFIG QUIET)
  add_executable(doc_runtime_demo
    framework/demo/main.cpp
    framework/src/network/TcpSocket.cpp
    framework/src/network/HttpClient.cpp
    framework/src/document/Tokenizer.cpp
    framework/src/document/Parser.cpp
    framework/src/document/Node.cpp
    framework/src/document/Document.cpp
    framework/src/style/StyleSheet.cpp
    framework/src/style/StyleResolver.cpp
    framework/src/layout/LayoutEngine.cpp
    framework/src/render/RenderTreeBuilder.cpp
    framework/src/render/NullRenderer.cpp
    framework/src/resource/ResourceManager.cpp
    framework/src/event/EventDispatcher.cpp
    framework/src/script/ScriptContext.cpp
    framework/src/app/Application.cpp
  )
  target_include_directories(doc_runtime_demo PRIVATE framework/include)
  target_compile_features(doc_runtime_demo PRIVATE cxx_std_20)
  target_compile_definitions(doc_runtime_demo PRIVATE FW_ENABLE_SDL2=0)
  if(TARGET SDL2::SDL2)
    target_sources(doc_runtime_demo PRIVATE framework/src/render/SDLRenderer.cpp)
    target_link_libraries(doc_runtime_demo PRIVATE SDL2::SDL2)
    target_compile_definitions(doc_runtime_demo PRIVATE FW_ENABLE_SDL2=1)
  endif()
  if(WIN32)
    target_link_libraries(doc_runtime_demo PRIVATE ws2_32)
  endif()
endif()

# Compiler sources (Phase 3: kern/core/compiler/)
set(COMPILER_SOURCES
    "${KERN_COMPILER_DIR}/lexer.cpp"
    "${KERN_COMPILER_DIR}/parser.cpp"
    "${KERN_COMPILER_DIR}/codegen.cpp"
    "${KERN_COMPILER_DIR}/semantic.cpp"
)

# Bytecode / Value model (core layer; shared by compiler + VM)
set(KERN_CORE_BYTECODE_SOURCES
    "${KERN_CORE_BYTECODE_DIR}/value.cpp"
    "${KERN_CORE_BYTECODE_DIR}/bytecode_peephole.cpp"
)

# VM runtime sources (kern/runtime/vm/; excludes bytecode contract — see KERN_CORE_BYTECODE_SOURCES)
set(VM_SOURCES
    ${KERN_CORE_BYTECODE_SOURCES}
    "${KERN_VM_DIR}/vm.cpp"
    "${KERN_VM_DIR}/bytecode_verifier.cpp"
    "${KERN_VM_DIR}/kern_socket.cpp"
)
if(WIN32)
  list(APPEND VM_SOURCES "${KERN_VM_DIR}/http_get_winhttp.cpp")
endif()

# Error reporting
set(ERROR_SOURCES "${KERN_ERRORS_DIR}/errors.cpp")
set(STDLIB_SOURCES "${KERN_SRC_DIR}/stdlib_modules.cpp")
set(IMPORT_SOURCES "${KERN_SRC_DIR}/import_resolution.cpp")
set(SYSTEM_MODULE_SOURCES
    "${KERN_SRC_DIR}/process/process_module.cpp"
    "${KERN_SRC_DIR}/system/event_bus.cpp"
    "${KERN_MODULES_SRC_DIR}/system/input_module.cpp"
    "${KERN_MODULES_SRC_DIR}/system/vision_module.cpp"
    "${KERN_MODULES_SRC_DIR}/system/render_module.cpp"
    "${KERN_MODULES_DIR}/builtin_module_registry.cpp"
)
set(SCANNER_SOURCES
    "${KERN_SRC_DIR}/scanner/scan_driver.cpp"
    "${KERN_SRC_DIR}/scanner/builtin_registry_check.cpp"
    "${KERN_SRC_DIR}/scanner/stdlib_export_check.cpp"
)

set(KERNC_SUPPORT_SOURCES
    "${KERN_COMPILER_DIR}/import_aliases.cpp"
    "${KERN_COMPILER_DIR}/project_resolver.cpp"
    "${KERN_ANALYZER_DIR}/project_analyzer.cpp"
    "${KERN_IR_DIR}/ir_builder.cpp"
    "${KERN_IR_DIR}/typed_ir_builder.cpp"
    "${KERN_IR_DIR}/passes/constant_folding.cpp"
    "${KERN_IR_DIR}/passes/dead_code_elim.cpp"
    "${KERN_IR_DIR}/passes/inline_basic.cpp"
    "${KERN_IR_DIR}/passes/typed_ir_passes.cpp"
    "${KERN_BACKEND_DIR}/cpp_backend.cpp"
    "${KERN_SRC_DIR}/packager/bundle_writer.cpp"
    "${KERN_SRC_DIR}/compile/compile_pipeline.cpp"
    "${KERN_UTILS_DIR}/kernconfig.cpp"
    "${KERN_UTILS_DIR}/build_cache.cpp"
)

# Shared engine (compiler, VM, diagnostics, pipeline glue, scanner drivers — not CLI mains).
set(KERN_ENGINE_SOURCES
    ${COMPILER_SOURCES}
    ${VM_SOURCES}
    ${ERROR_SOURCES}
    ${STDLIB_SOURCES}
    ${IMPORT_SOURCES}
    ${SYSTEM_MODULE_SOURCES}
    ${SCANNER_SOURCES}
    ${KERNC_SUPPORT_SOURCES}
)
add_library(kern_core STATIC ${KERN_ENGINE_SOURCES})
target_include_directories(kern_core PUBLIC ${KERN_TOOLCHAIN_PRIVATE_INCLUDES})
target_compile_definitions(kern_core PRIVATE KERN_VERSION="${KERN_VERSION}")
if(KERN_BUILD_ID)
  target_compile_definitions(kern_core PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
endif()
kern_apply_strict_warnings(kern_core)

set(KERN_MAIN_SOURCES "${KERN_CLI_DIR}/main.cpp")
if(WIN32)
  list(APPEND KERN_MAIN_SOURCES "${KERN_PLATFORM_DIR}/win32_associate_kn.cpp")
endif()
add_executable(kern ${KERN_MAIN_SOURCES} ${KERN_WIN32_RESOURCES})
target_link_libraries(kern PRIVATE kern_core)
target_compile_definitions(kern PRIVATE KERN_VERSION="${KERN_VERSION}")
if(KERN_BUILD_ID)
  target_compile_definitions(kern PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
endif()

add_executable(kern_repl "${KERN_CLI_DIR}/repl_main.cpp" ${KERN_WIN32_RESOURCES})
target_link_libraries(kern_repl PRIVATE kern_core)
target_compile_definitions(kern_repl PRIVATE KERN_VERSION="${KERN_VERSION}")
if(KERN_BUILD_ID)
  target_compile_definitions(kern_repl PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
endif()

add_executable(kernc "${KERN_CLI_DIR}/kernc_main.cpp" ${KERN_WIN32_RESOURCES})
target_link_libraries(kernc PRIVATE kern_core)
target_compile_definitions(kernc PRIVATE KERN_VERSION="${KERN_VERSION}")
if(KERN_BUILD_ID)
  target_compile_definitions(kernc PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
endif()

add_executable(kern_lsp "${KERN_CLI_DIR}/lsp_main.cpp" ${KERN_WIN32_RESOURCES})
target_link_libraries(kern_lsp PRIVATE kern_core)
target_compile_definitions(kern_lsp PRIVATE KERN_VERSION="${KERN_VERSION}")
if(KERN_BUILD_ID)
  target_compile_definitions(kern_lsp PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
endif()

add_executable(kern_contract_humanize "${KERN_SRC_DIR}/tests/humanize_path_contract_test.cpp" ${ERROR_SOURCES})
target_include_directories(kern_contract_humanize PRIVATE ${KERN_TOOLCHAIN_PRIVATE_INCLUDES})
kern_apply_strict_warnings(kern_contract_humanize)

kern_apply_strict_warnings(kern)
kern_apply_strict_warnings(kern_repl)
kern_apply_strict_warnings(kernc)
kern_apply_strict_warnings(kern_lsp)

add_executable(kern-scan "${KERN_CLI_DIR}/scan_main.cpp" ${KERN_WIN32_RESOURCES})
target_link_libraries(kern-scan PRIVATE kern_core)
target_compile_definitions(kern-scan PRIVATE KERN_VERSION="${KERN_VERSION}")
if(KERN_BUILD_ID)
  target_compile_definitions(kern-scan PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
endif()
kern_apply_strict_warnings(kern-scan)

if(WIN32)
  target_link_libraries(kern_core PUBLIC psapi winhttp wininet ws2_32)
  target_link_libraries(kern PRIVATE advapi32 shell32)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/kern_logo.ico")
    add_custom_command(
      TARGET kern
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/kern_logo.ico"
              "$<TARGET_FILE_DIR:kern>/kern_logo.ico"
      COMMENT "Copy kern_logo.ico next to kern.exe (Windows shell icon)")
  endif()
  target_link_libraries(kern_lsp PRIVATE winhttp wininet)
  # Compiler binary is kern-impl.exe; kern.exe is a tiny launcher (tools/kernc_launcher.c) that runs
  # cmake --build ... --target kern then execs kern-impl.exe (stale-compiler issue when only kern.exe was run).
  set_target_properties(kernc PROPERTIES OUTPUT_NAME "kern-impl")
  add_executable(kern_launcher "${KERN_REPO_TOOLS_DIR}/kernc_launcher.c" ${KERN_WIN32_RESOURCES})
  set_target_properties(kern_launcher PROPERTIES OUTPUT_NAME "kernc")
  # Do NOT add_dependencies(kern_launcher kernc): kernc POST_BUILD runs nested build for launcher.
  add_custom_command(TARGET kernc POST_BUILD
    COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config $<CONFIG> --target kern_launcher
    COMMENT "Refresh kern.exe launcher beside kern-impl.exe")
  add_custom_command(TARGET kern_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${KERN_REPO_TOOLS_DIR}/kern-release-dev.cmd"
      "$<TARGET_FILE_DIR:kern_launcher>/kern-release-dev.cmd")
  kern_apply_strict_warnings(kern_launcher)
endif()

# Graphics (g2d/game): link Raylib when found so kern and kern_repl have import("g2d") and graphics.
# NOTE:
# - To embed Raylib in kern.exe on Windows, configure with:
#     -DVCPKG_TARGET_TRIPLET=x64-windows-static
#     -DKERN_PREFER_STATIC_RAYLIB=ON
# - This repo’s recommended publish path (shareable drops) uses these settings by default.
# To get graphics: install Raylib then reconfigure (e.g. vcpkg install raylib, then
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
# or set RAYLIB_ROOT to raylib install dir).
# Linux (FetchContent): install X11/OpenGL/ALSA dev packages — see .github/workflows/linux-kern.yml.
if(KERN_BUILD_GAME)
  set(KERN_RAYLIB_TARGET "")

  # Prefer config targets from package managers.
  find_package(raylib CONFIG QUIET)
  if(TARGET raylib::raylib)
    set(KERN_RAYLIB_TARGET raylib::raylib)
  elseif(TARGET raylib)
    set(KERN_RAYLIB_TARGET raylib)
  else()
    find_package(unofficial-raylib CONFIG QUIET)
    if(TARGET unofficial-raylib::raylib)
      set(KERN_RAYLIB_TARGET unofficial-raylib::raylib)
    endif()
  endif()

  if(KERN_RAYLIB_TARGET STREQUAL "")
    if(DEFINED ENV{RAYLIB_ROOT})
      set(RAYLIB_ROOT "$ENV{RAYLIB_ROOT}")
    endif()
    if(RAYLIB_ROOT)
      find_path(RAYLIB_INCLUDE_DIR raylib.h PATHS ${RAYLIB_ROOT}/include ${RAYLIB_ROOT})
      find_library(RAYLIB_LIBRARY NAMES raylib raylib.lib PATHS ${RAYLIB_ROOT}/lib ${RAYLIB_ROOT})
    endif()
    if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
      find_path(RAYLIB_INCLUDE_DIR raylib.h)
      find_library(RAYLIB_LIBRARY NAMES raylib)
    endif()
    if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
      add_library(kern_raylib_manual UNKNOWN IMPORTED)
      set_target_properties(kern_raylib_manual PROPERTIES
        IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}"
      )
      set(KERN_RAYLIB_TARGET kern_raylib_manual)

      # Manual static fallback: add transitive libs needed by raylib static builds.
      if(WIN32 AND KERN_PREFER_STATIC_RAYLIB)
        find_package(glfw3 CONFIG QUIET)
        if(TARGET glfw)
          target_link_libraries(kern_raylib_manual INTERFACE glfw)
        elseif(TARGET glfw3)
          target_link_libraries(kern_raylib_manual INTERFACE glfw3)
        elseif(TARGET glfw3::glfw)
          target_link_libraries(kern_raylib_manual INTERFACE glfw3::glfw)
        else()
          find_library(GLFW_LIBRARY NAMES glfw3 glfw)
          if(GLFW_LIBRARY)
            target_link_libraries(kern_raylib_manual INTERFACE "${GLFW_LIBRARY}")
          endif()
        endif()
        target_link_libraries(kern_raylib_manual INTERFACE winmm opengl32 gdi32 user32 shell32 advapi32)
      endif()
    endif()
  endif()
  if(KERN_RAYLIB_TARGET STREQUAL "" AND KERN_AUTO_FETCH_RAYLIB)
    message(STATUS "Kern: Raylib not found locally; cloning raylib from GitHub (first time can take a few minutes).")
    message(STATUS "      IDE-only: cancel and reconfigure with -DKERN_BUILD_GAME=OFF to skip Raylib entirely.")
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      raylib_external
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 5.5
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib_external)
    if(TARGET raylib)
      set(KERN_RAYLIB_TARGET raylib)
    endif()
  endif()
  if(NOT KERN_RAYLIB_TARGET STREQUAL "")
    get_target_property(_raylib_loc ${KERN_RAYLIB_TARGET} IMPORTED_LOCATION_RELEASE)
    if(NOT _raylib_loc)
      get_target_property(_raylib_loc ${KERN_RAYLIB_TARGET} IMPORTED_LOCATION)
    endif()
    set(G2D_SOURCES
      "${KERN_MODULES_SRC_DIR}/g2d/g2d.cpp"
      "${KERN_MODULES_SRC_DIR}/g2d/window.cpp"
      "${KERN_MODULES_SRC_DIR}/g2d/renderer.cpp"
      "${KERN_MODULES_SRC_DIR}/g2d/shapes.cpp"
      "${KERN_MODULES_SRC_DIR}/g2d/text.cpp"
      "${KERN_MODULES_SRC_DIR}/g2d/colors.cpp"
      "${KERN_MODULES_SRC_DIR}/g3d/g3d.cpp"
    )
    add_library(kern_gfx STATIC "${KERN_GAME_MODULE_DIR}/game_builtins.cpp" ${G2D_SOURCES})
    target_include_directories(kern_gfx PUBLIC ${KERN_TOOLCHAIN_PRIVATE_INCLUDES})
    target_compile_definitions(kern_gfx PRIVATE KERN_BUILD_GAME=1)
    target_link_libraries(kern_gfx PUBLIC ${KERN_RAYLIB_TARGET})
    kern_apply_strict_warnings(kern_gfx)
    target_link_libraries(kern PRIVATE kern_gfx)
    target_link_libraries(kern_repl PRIVATE kern_gfx)
    target_link_libraries(kernc PRIVATE kern_gfx)
    target_link_libraries(kern-scan PRIVATE kern_gfx)
    target_compile_definitions(kern PRIVATE KERN_BUILD_GAME=1)
    target_compile_definitions(kern_repl PRIVATE KERN_BUILD_GAME=1)
    target_compile_definitions(kernc PRIVATE KERN_BUILD_GAME=1)
    target_compile_definitions(kern-scan PRIVATE KERN_BUILD_GAME=1)
    add_executable(kern_game "${KERN_GAME_MODULE_DIR}/game_main.cpp" ${KERN_WIN32_RESOURCES})
    target_link_libraries(kern_game PRIVATE kern_core kern_gfx)
    target_compile_definitions(kern_game PRIVATE KERN_BUILD_GAME=1 KERN_VERSION="${KERN_VERSION}")
    if(KERN_BUILD_ID)
      target_compile_definitions(kern_game PRIVATE KERN_BUILD_ID="${KERN_BUILD_ID}")
    endif()
    kern_apply_strict_warnings(kern_game)

    # Some static raylib builds require explicitly linking system/libs.
    if(WIN32 AND KERN_PREFER_STATIC_RAYLIB)
      set(_kern_raylib_extra winmm opengl32 gdi32 user32 shell32 advapi32)
      find_library(KERN_GLFW3_LIBRARY NAMES glfw3)
      if(KERN_GLFW3_LIBRARY)
        list(APPEND _kern_raylib_extra "${KERN_GLFW3_LIBRARY}")
      else()
        find_library(KERN_GLFW_LIBRARY NAMES glfw)
        if(KERN_GLFW_LIBRARY)
          list(APPEND _kern_raylib_extra "${KERN_GLFW_LIBRARY}")
        endif()
      endif()
      target_link_libraries(kern_gfx PUBLIC ${_kern_raylib_extra})
    endif()
    if(KERN_PREFER_STATIC_RAYLIB)
      message(STATUS "Kern: graphics integrated - static Raylib preferred (no external raylib.dll when available)")
      if(WIN32 AND _raylib_loc AND _raylib_loc MATCHES "x64-windows[/\\\\]lib[/\\\\]raylib\\.lib")
        message(WARNING "KERN_PREFER_STATIC_RAYLIB=ON but current Raylib appears to be x64-windows (DLL runtime). Reconfigure with -DVCPKG_TARGET_TRIPLET=x64-windows-static to embed Raylib in exe.")
      endif()
    else()
      message(STATUS "Kern: graphics integrated - Raylib available")
    endif()
  else()
    message(WARNING "KERN_BUILD_GAME=ON but Raylib was unavailable. Enable KERN_AUTO_FETCH_RAYLIB=ON or install Raylib manually.")
  endif()
endif()

include(GNUInstallDirs)
install(TARGETS kern kern-scan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
