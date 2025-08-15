# ==============================
# Simulator/CMakeLists.txt — exe
# ==============================
# Builds the simulator executable with the exact name:
#   simulator_<id1>_<id2>
# Includes:
#   - sim_include
#   - ../common                      (staff headers; as-is)
#   - ../UserCommon/UC_include       (your shared headers)
# Links (Linux):
#   - pthread (multithreading), dl (dlopen/dlclose)
# Content:
#   - Your main() lives here.
#   - Provide the auto-registration *implementations* here, e.g.:
#       PlayerRegistration.cpp, TankAlgorithmRegistration.cpp, GameManagerRegistration.cpp,
#     and any registrar/helper used to store discovered factories.

cmake_minimum_required(VERSION 3.16)
project(SimulatorProj LANGUAGES CXX)

# ---- IDs are required to name the executable per the assignment spec ----
# Build from the Simulator/ dir like:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#
set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------- Added for debugging purposes -------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# --------------------------------------

# Detect if this dir is the top-level (so we can set local output dirs only then)
set(TOP_LEVEL OFF)
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(TOP_LEVEL ON)
endif()

# --------------------------------------
# UserCommon: build once as a static lib
# --------------------------------------
# All your shared sources live under ../UserCommon/UC_src
file(GLOB UC_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_src/*.cpp)

if(UC_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under UserCommon/UC_src")
endif()

add_library(usercommon STATIC ${UC_SOURCES})
target_include_directories(usercommon PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
)

# For shared linking scenarios elsewhere, PIC is nice to have even on static libs
set_target_properties(usercommon PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --------------------------------------
# Simulator executable and sources
# --------------------------------------
file(GLOB_RECURSE SIM_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/sim_src/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/sim_src/*.cc
        ${CMAKE_CURRENT_LIST_DIR}/sim_src/*.cxx
)

if(SIM_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under Simulator/sim_src")
endif()

set(TARGET_NAME simulator_${SUBMITTER_IDS})
add_executable(${TARGET_NAME} ${SIM_SOURCES})

set_target_properties(${TARGET_NAME} PROPERTIES ENABLE_EXPORTS ON)

# ------- Added for debugging purposes -------
target_link_options(${TARGET_NAME} PRIVATE -Wl,--export-dynamic) 
# --------------------------------------------

# Exact output name required by the assignment: simulator_<ids>
set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "simulator_${SUBMITTER_IDS}"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
)

# Includes for the simulator target
target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/sim_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include
)

# Warnings
if (MSVC)
    # target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
    # ------- Added for debugging purposes -------
    target_compile_options(${TARGET_NAME} PRIVATE
        $<$<CONFIG:Debug>:-O0 -g -fno-omit-frame-pointer>) 
    # --------------------------------------------

else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Threads (std::thread) and dlopen/dlsym/dlclose on Linux
find_package(Threads REQUIRED)
target_link_libraries(${TARGET_NAME}
        PRIVATE
        usercommon
        Threads::Threads
        dl
)
