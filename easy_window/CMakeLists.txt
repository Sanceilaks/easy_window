cmake_minimum_required (VERSION 3.8)

add_executable (easy_window "easy_window.cpp" "easy_window.h")
target_link_libraries (easy_window PUBLIC d3d9)

set_property(TARGET easy_window PROPERTY CXX_STANDARD 23)
