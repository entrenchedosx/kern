# attach Raylib, g2d/g3d, and game builtins to kernc_standalone (same stack as main KERN_BUILD_GAME targets).
# expects: KERN_STANDALONE_REPO = absolute path to Kern repository root (forward slashes OK).
# target must already exist as kernc_standalone.

if(NOT DEFINED KERN_STANDALONE_REPO)
  message(FATAL_ERROR "kern_standalone_graphics.cmake: KERN_STANDALONE_REPO not set")
endif()

set(_KERN_REPO "${KERN_STANDALONE_REPO}")
# Must match KERN_SRC_DIR in cmake/kern_paths.cmake (legacy: <repo>/src). Do not include kern_paths.cmake here:
# CMAKE_SOURCE_DIR may be the embedding project, not the Kern repo.
set(_KERN_SRC_DIR "${_KERN_REPO}/src")
set(_KERN_MODULES_DIR "${_KERN_REPO}/kern/modules")
set(KERN_RAYLIB_TARGET "")

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
    find_path(RAYLIB_INCLUDE_DIR raylib.h PATHS "${RAYLIB_ROOT}/include" "${RAYLIB_ROOT}")
    find_library(RAYLIB_LIBRARY NAMES raylib raylib.lib PATHS "${RAYLIB_ROOT}/lib" "${RAYLIB_ROOT}")
  endif()
  if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
    find_path(RAYLIB_INCLUDE_DIR raylib.h)
    find_library(RAYLIB_LIBRARY NAMES raylib)
  endif()
  if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)
    add_library(kern_standalone_raylib_imp UNKNOWN IMPORTED)
    set_target_properties(kern_standalone_raylib_imp PROPERTIES
      IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}")
    set(KERN_RAYLIB_TARGET kern_standalone_raylib_imp)
    if(WIN32)
      find_package(glfw3 CONFIG QUIET)
      if(TARGET glfw)
        target_link_libraries(kern_standalone_raylib_imp INTERFACE glfw)
      elseif(TARGET glfw3)
        target_link_libraries(kern_standalone_raylib_imp INTERFACE glfw3)
      elseif(TARGET glfw3::glfw)
        target_link_libraries(kern_standalone_raylib_imp INTERFACE glfw3::glfw)
      else()
        find_library(_KERN_SL_GLFW NAMES glfw3 glfw)
        if(_KERN_SL_GLFW)
          target_link_libraries(kern_standalone_raylib_imp INTERFACE "${_KERN_SL_GLFW}")
        endif()
      endif()
      target_link_libraries(kern_standalone_raylib_imp INTERFACE winmm opengl32 gdi32 user32 shell32 advapi32)
    endif()
  endif()
endif()

if(KERN_RAYLIB_TARGET STREQUAL "")
  message(STATUS "kern_standalone: Raylib not found via package config; fetching Raylib 5.5 (FetchContent)...")
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    raylib_kern_standalone_fc
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
  )
  FetchContent_MakeAvailable(raylib_kern_standalone_fc)
  if(TARGET raylib)
    set(KERN_RAYLIB_TARGET raylib)
  endif()
endif()

if(KERN_RAYLIB_TARGET STREQUAL "")
  message(FATAL_ERROR
    "Standalone EXE needs Raylib (g2d/g3d/game). Options:\n"
    "  - Windows: configure with the bundled vcpkg toolchain + x64-windows-static triplet, or\n"
    "  - cmake -B build -DCMAKE_TOOLCHAIN_FILE=tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static\n"
    "  - Or set RAYLIB_ROOT to a Raylib install with include/ and lib/.")
endif()

set(_KERN_G2D_SRCS
  "${_KERN_MODULES_DIR}/g2d/g2d.cpp"
  "${_KERN_MODULES_DIR}/g2d/window.cpp"
  "${_KERN_MODULES_DIR}/g2d/renderer.cpp"
  "${_KERN_MODULES_DIR}/g2d/shapes.cpp"
  "${_KERN_MODULES_DIR}/g2d/text.cpp"
  "${_KERN_MODULES_DIR}/g2d/colors.cpp"
  "${_KERN_MODULES_DIR}/g3d/g3d.cpp"
)

target_sources(kernc_standalone PRIVATE
  "${_KERN_MODULES_DIR}/game/game_builtins.cpp"
  ${_KERN_G2D_SRCS}
)
target_compile_definitions(kernc_standalone PRIVATE KERN_BUILD_GAME=1)
target_link_libraries(kernc_standalone PRIVATE ${KERN_RAYLIB_TARGET})

if(WIN32 AND MSVC)
  set(_KERN_SL_EXTRA winmm opengl32 gdi32 user32 shell32 advapi32)
  find_library(KERN_SL_GLFW3 NAMES glfw3)
  if(KERN_SL_GLFW3)
    list(APPEND _KERN_SL_EXTRA "${KERN_SL_GLFW3}")
  else()
    find_library(KERN_SL_GLFW NAMES glfw)
    if(KERN_SL_GLFW)
      list(APPEND _KERN_SL_EXTRA "${KERN_SL_GLFW}")
    endif()
  endif()
  target_link_libraries(kernc_standalone PRIVATE ${_KERN_SL_EXTRA})
endif()

# align CRT with the standalone stub (/MT) so vcpkg static Raylib does not pull mismatched defaults.
if(MSVC)
  set_property(TARGET kernc_standalone PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

message(STATUS "kern_standalone: Raylib graphics enabled (g2d/g3d/game)")
