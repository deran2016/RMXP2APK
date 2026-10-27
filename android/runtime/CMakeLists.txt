cmake_minimum_required(VERSION 3.22)
project(lunetile_android_game LANGUAGES C CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if (NOT RUBY_LIBRARY OR NOT EXISTS "${RUBY_LIBRARY}")
  message(FATAL_ERROR "RUBY_LIBRARY must point to the Android ARM64 Ruby static library")
endif()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../core" "${CMAKE_CURRENT_BINARY_DIR}/core")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/graphics_builtin.h"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/embed_graphics.py" "${CMAKE_CURRENT_BINARY_DIR}/graphics_builtin.h"
  DEPENDS embed_graphics.py lunetile/timing.rb lunetile/graphics.rb lunetile/tilemap.rb lunetile/rpg_graphics.rb lunetile/autotile_quarters.json VERBATIM)
add_library(lunetile_game SHARED game_jni.cpp bitmap.cpp graphics.cpp raster.cpp surface.cpp
  ../input/input_jni.c ../audio/audio_jni.c "${CMAKE_CURRENT_BINARY_DIR}/graphics_builtin.h"
  $<TARGET_OBJECTS:lunetile_android_encodings>)
target_compile_features(lunetile_game PRIVATE cxx_std_17)
target_include_directories(lunetile_game PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${RUBY_SOURCE_DIR}/include"
  "${RUBY_BUILD_DIR}/.ext/include/aarch64-linux-android-android")
target_link_libraries(lunetile_game PRIVATE lunetile_android_core "${RUBY_LIBRARY}" android log dl m z)
