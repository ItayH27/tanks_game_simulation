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

# ==============================
# Simulator/CMakeLists.txt — exe
# ==============================
cmake_minimum_required(VERSION 3.16)
project(SimulatorProj LANGUAGES CXX)

set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TOP_LEVEL OFF)
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(TOP_LEVEL ON)
endif()

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

# Export symbols so plugins can resolve registration ctors
set_target_properties(${TARGET_NAME} PROPERTIES ENABLE_EXPORTS ON)
target_link_options(${TARGET_NAME} PRIVATE -Wl,-E)

set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "simulator_${SUBMITTER_IDS}"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
)

target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/sim_include
        ${CMAKE_CURRENT_LIST_DIR}/../common
        ${CMAKE_CURRENT_LIST_DIR}/../UserCommon/UC_include
)

# --- Always bundle UserCommon here as well ---
target_sources(${TARGET_NAME} PRIVATE $<TARGET_OBJECTS:usercommon_obj>)

if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
        $<$<CONFIG:Debug>:-O0 -g -fno-omit-frame-pointer>)
else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(${TARGET_NAME}
        PRIVATE
        Threads::Threads
        dl
)
