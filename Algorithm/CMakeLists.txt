# ==============================
# Algorithm/CMakeLists.txt — plugin
# ==============================
cmake_minimum_required(VERSION 3.16)
project(AlgorithmProj LANGUAGES CXX)

set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE ALGO_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/Algo_src/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Algo_src/*.cc
        ${CMAKE_CURRENT_LIST_DIR}/Algo_src/*.cxx)

if(ALGO_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under Algorithm/Algo_src")
endif()

set(TARGET_NAME Algorithm_${SUBMITTER_IDS})
add_library(${TARGET_NAME} SHARED ${ALGO_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "Algorithm_${SUBMITTER_IDS}"
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/Algo_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include)

# --- Always bundle UserCommon into this .so (no external libusercommon.so) ---
target_sources(${TARGET_NAME} PRIVATE $<TARGET_OBJECTS:usercommon_obj>)
# (No target_link_libraries(... usercommon), no RPATH, no post-build copy)

if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-,
        $<$<CONFIG:Debug>:-O0 -g -fno-omit-frame-pointer>)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Do NOT add registration .cpps here — constructors live in Simulator.
