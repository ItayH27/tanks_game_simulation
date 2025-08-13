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
#   - At runtime, the Simulator dlopens Algorithm_*.so and GameManager_*.so, which
#     contain REGISTER_* static objects that call into these constructors.
#
# Usage (from Simulator/):
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSUBMITTER_IDS=<id1>_<id2>
#   cmake --build build --config Release
# Run (examples):
#   ./build/simulator_<id1>_<id2> -comparative game_map=<map> game_managers_folder=<dir> \
#       algorithm1=<a1.so> algorithm2=<a2.so> [num_threads=<n>] [-verbose]
#   ./build/simulator_<id1>_<id2> -competition game_maps_folder=<maps_dir> \
#       game_manager=<gm.so> algorithms_folder=<algos_dir> [num_threads=<n>] [-verbose]
#
# Assignment references:
# - Simulator is the executable; loads .so plugins; provides registration implementations.
# - Command-line modes and naming conventions for artifacts.
#

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

# Detect if this dir is the top-level (so we can set local output dirs only then)
set(TOP_LEVEL OFF)
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(TOP_LEVEL ON)
endif()

# ---- Sources (your layout) ----
file(GLOB_RECURSE SIM_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_LIST_DIR}/sim_src/*.cpp
        ${CMAKE_CURRENT_LIST_DIR}/sim_src/*.cc
        ${CMAKE_CURRENT_LIST_DIR}/sim_src/*.cxx)

if(SIM_SOURCES STREQUAL "")
    message(FATAL_ERROR "No sources under Simulator/sim_src")
endif()

set(TARGET_NAME simulator_${SUBMITTER_IDS})
add_executable(${TARGET_NAME} ${SIM_SOURCES})

# Exact output name required by the assignment: simulator_<ids>
set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "simulator_${SUBMITTER_IDS}"
)

# Includes:
target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/sim_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include
)

# Warnings
if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Threads (std::thread) and dlopen/dlsym/dlclose on Linux
find_package(Threads REQUIRED)
target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads dl)

# Put built exe in a local bin/ only when building this dir directly
if (TOP_LEVEL)
    set_target_properties(${TARGET_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin
    )
endif()
