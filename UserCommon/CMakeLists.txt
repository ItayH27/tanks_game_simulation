cmake_minimum_required(VERSION 3.16)
project(UserCommon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB UC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/UC_src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UC_src/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/UC_src/*.cxx
)
if(UC_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under UserCommon/UC_src")
endif()

# Always bundle: build object lib only
add_library(usercommon_obj OBJECT ${UC_SOURCES})
set_property(TARGET usercommon_obj PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(usercommon_obj PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/UC_include
    ${CMAKE_SOURCE_DIR}/common
)
