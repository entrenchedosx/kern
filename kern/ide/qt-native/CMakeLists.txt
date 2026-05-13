cmake_minimum_required(VERSION 3.16)
project(kern_ide_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)

qt_standard_project_setup()

qt_add_executable(kern_ide_native
  src/main.cpp
  src/MainWindow.cpp
  src/SplHighlighter.cpp
  include/MainWindow.hpp
  include/SplHighlighter.hpp
)

target_include_directories(kern_ide_native PRIVATE include)

target_link_libraries(kern_ide_native PRIVATE Qt6::Widgets Qt6::Gui Qt6::Core)

set_target_properties(kern_ide_native PROPERTIES OUTPUT_NAME "kern-ide")
