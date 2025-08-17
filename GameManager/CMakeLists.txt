# ===================================
# GameManager/CMakeLists.txt — plugin
# ===================================
# Builds the GameManager plugin as a shared object for Linux with the exact filename:
#   GameManager_<id1>_<id2>.so      (no 'lib' prefix)
# Includes:
#   - GM_include                     (this project's headers)
#   - ../common                      (staff headers; do NOT modify or add files there)
#   - ../UserCommon/UC_include       (your shared headers)
# IMPORTANT:
# - Do NOT compile any auto-registration .cpp here; only include the registration headers
#   and place REGISTER_GAME_MANAGER(...) in your class TU. The registration constructors'
#   implementations are compiled in the Simulator project.
#
# Usage (from GameManager/):
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSUBMITTER_IDS=<id1>_<id2>
#   cmake --build build --config Release
# Output:
#   build/GameManager_<id1>_<id2>.so
#
# Assignment references:
# - GameManager is a standalone .so with unique ID-based name, dynamically loaded by Simulator.
# - common/UserCommon layout and auto-registration division of labor.
#

# ===================================
# GameManager/CMakeLists.txt — plugin
# ===================================
cmake_minimum_required(VERSION 3.16)
project(GameManagerProj LANGUAGES CXX)

set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE GM_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/GM_src/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/GM_src/*.cc
        ${CMAKE_CURRENT_LIST_DIR}/GM_src/*.cxx)

if(GM_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under GameManager/GM_src")
endif()

set(TARGET_NAME GameManager_${SUBMITTER_IDS})
add_library(${TARGET_NAME} SHARED ${GM_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "GameManager_${SUBMITTER_IDS}"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

# Includes
target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/GM_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include)

# --- Always bundle UserCommon into this .so (no external libusercommon.so) ---
# Pull in the object files from the UserCommon object library
target_sources(${TARGET_NAME} PRIVATE $<TARGET_OBJECTS:usercommon_obj>)
# (No link to 'usercommon' SHARED, no RPATH, no post-build copy)

# Warnings
if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive- 
        $<$<CONFIG:Debug>:-O0 -g -fno-omit-frame-pointer>)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
