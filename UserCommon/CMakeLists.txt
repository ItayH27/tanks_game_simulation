cmake_minimum_required(VERSION 3.16)
project(UserCommon LANGUAGES CXX)

# C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure -fPIC everywhere (good for shared libs)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Sources
file(GLOB UC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/UC_src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UC_src/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/UC_src/*.cxx
)

if(UC_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under UserCommon/UC_src")
endif()

# Build shared library
add_library(usercommon SHARED ${UC_SOURCES})

# Public headers
target_include_directories(usercommon PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/UC_include
    ${CMAKE_SOURCE_DIR}/common
)

# Nice, stable name
set_target_properties(usercommon PROPERTIES
    OUTPUT_NAME "usercommon"
)
