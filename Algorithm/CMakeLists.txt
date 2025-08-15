# ==============================
# Algorithm/CMakeLists.txt — plugin
# ==============================
# Builds the Algorithm plugin as a shared object for Linux with the exact filename:
#   Algorithm_<id1>_<id2>.so        (no 'lib' prefix)
# Includes:
#   - Algo_include
#   - ../common                      (staff headers; as-is)
#   - ../UserCommon/UC_include       (your shared headers)
# IMPORTANT:
# - Do NOT compile registration .cpp files here. Only include headers and place:
#     REGISTER_TANK_ALGORITHM(...), REGISTER_PLAYER(...)
#   in your TUs. The registration constructors' implementations are compiled in Simulator.
#
# Usage (from Algorithm/):
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSUBMITTER_IDS=<id1>_<id2>
#   cmake --build build --config Release
# Output:
#   build/Algorithm_<id1>_<id2>.so
#
# Assignment references:
# - Algorithm is a .so plugin with unique ID-based name, dynamically loaded by Simulator.
# - Registration implementation lives in Simulator; headers shared across projects.
#


cmake_minimum_required(VERSION 3.16)
project(AlgorithmProj LANGUAGES CXX)

# ---- IDs are required to name the .so per the assignment ----
# Build from Algorithm/:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------- Added for debugging -------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # tooling (VS Code/clangd)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # ensure -fPIC everywhere
# ------- End of debugging additions -------

# ---- Sources (your layout) ----
file(GLOB_RECURSE ALGO_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/Algo_src/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Algo_src/*.cc
        ${CMAKE_CURRENT_LIST_DIR}/Algo_src/*.cxx)

if(ALGO_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under Algorithm/Algo_src")
endif()

set(TARGET_NAME Algorithm_${SUBMITTER_IDS})
add_library(${TARGET_NAME} SHARED ${ALGO_SOURCES})

# Exact filename required: Algorithm_<ids>.so (no 'lib' prefix)
set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "Algorithm_${SUBMITTER_IDS}"
        PREFIX ""                       # => Algorithm_<ids>.so, not libAlgorithm_*.so
        POSITION_INDEPENDENT_CODE ON
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}) # <== Output here

# Includes:
# - your algorithm headers
# - course-provided 'common' (as-is; don't modify/add files there)
# - your shared headers in UserCommon
target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/Algo_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include)

# Warnings
if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-,
    # ------- Added for debugging -------
    $<$<CONFIG:Debug>:-O0 -g -fno-omit-frame-pointer>)
    # ------- End of debugging additions -------
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Do NOT add registration .cpps here — the constructors for
# PlayerRegistration and TankAlgorithmRegistration are implemented
# in the Simulator project and will be resolved at runtime via dlopen.
# (Only the registration *headers* are included here.)
