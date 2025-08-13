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

cmake_minimum_required(VERSION 3.16)
project(GameManagerProj LANGUAGES CXX)

# -------------------------
# Required: your submitter IDs
# -------------------------
# Usage (from GameManager dir):
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
# Produces: build/GameManager_111111111_222222222.so  (Linux)
#
set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------
# Sources & headers (your layout)
# -------------------------
file(GLOB_RECURSE GM_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/GM_src/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/GM_src/*.cc
        ${CMAKE_CURRENT_LIST_DIR}/GM_src/*.cxx)

if(GM_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under GameManager/GM_src")
endif()

set(TARGET_NAME GameManager_${SUBMITTER_IDS})
add_library(${TARGET_NAME} SHARED ${GM_SOURCES}
        ../UserCommon/UC_src/ExtSatelliteView.cpp)

# Pull in your shared implementations so their vtables live inside the GM .so
file(GLOB USERCOMMON_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_src/*.cpp
)

if(USERCOMMON_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under ../UserCommon/UC_src")
endif()

# We already expose headers via target_include_directories (kept as‑is)
# Compile the implementations into the GM .so:
target_sources(${TARGET_NAME} PRIVATE ${USERCOMMON_SOURCES})

# Exact filename required by the assignment (no 'lib' prefix on Linux)
set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "GameManager_${SUBMITTER_IDS}"
        PREFIX ""                       # => GameManager_<ids>.so, not libGameManager_*.so
        POSITION_INDEPENDENT_CODE ON
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})  # <== Output here

# Public includes:
# - this project’s headers
# - course-provided 'common' (as-is; do not modify/add)
# - your 'UserCommon' headers
target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/GM_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include)

# Warnings
if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Nothing to link here; Simulator will dlopen() this .so at runtime (Linux).
# Do NOT compile registration .cpps here; they belong to Simulator.:contentReference[oaicite:1]{index=1}
