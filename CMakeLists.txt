# ============================
# Root CMakeLists.txt
# ============================
cmake_minimum_required(VERSION 3.16)
project(Ex3_Tanks LANGUAGES CXX)

# -------------------------
# Global config
# -------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Assignment requires naming all artifacts with both submitter IDs.
set(SUBMITTER_IDS "209277367_322542887" CACHE STRING "Submitter IDs" FORCE)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" ON)
if(ENABLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -O1 -g)
  add_link_options(-fsanitize=address)
endif()

# -------------------------
# Subprojects (order matters: UserCommon first if others link/consume it)
# -------------------------
add_subdirectory(UserCommon)  
add_subdirectory(GameManager)  # builds GameManager_<ids>.so
add_subdirectory(Algorithm)    # builds Algorithm_<ids>.so
add_subdirectory(Simulator)    # builds simulator_<ids> (with main)

set_target_properties(simulator_${SUBMITTER_IDS} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Convenience meta-target to build everything in one go
add_custom_target(build_all
        DEPENDS
        GameManager_${SUBMITTER_IDS}
        Algorithm_${SUBMITTER_IDS}
        simulator_${SUBMITTER_IDS}
)

# -------------------------
# Tests (always available; auto-fetch GoogleTest)
# -------------------------
include(CTest)
enable_testing()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_SOURCE_DIR}/tests")
  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")

  # All implementation sources required by tests (compiled into each test exe)
  set(SIM_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/competitive_simulator.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/comparative_simulator.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/cmd_parser.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/Simulator.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/GameManagerRegistrar.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/GameManagerRegistration.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/AlgorithmRegistrar.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/TankAlgorithmRegistration.cpp
    ${CMAKE_SOURCE_DIR}/Simulator/sim_src/PlayerRegistration.cpp
  )

  # UserCommon pieces the simulator depends on
  set(USERCOMMON_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/UserCommon/UC_src/ExtSatelliteView.cpp
    ${CMAKE_SOURCE_DIR}/UserCommon/UC_src/ExtBattleInfo.cpp
    ${CMAKE_SOURCE_DIR}/UserCommon/UC_src/Shell.cpp
  )

  foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)

    add_executable(${name}
      ${src}
      ${SIM_IMPL_SOURCES}
      ${USERCOMMON_IMPL_SOURCES}
    )

    target_compile_features(${name} PRIVATE cxx_std_20)

    target_include_directories(${name} PRIVATE
      ${CMAKE_SOURCE_DIR}/common
      ${CMAKE_SOURCE_DIR}/UserCommon/UC_include
      ${CMAKE_SOURCE_DIR}/Simulator/sim_include
      ${CMAKE_SOURCE_DIR}/GameManager/GM_include
      ${CMAKE_SOURCE_DIR}/Algorithm/Algo_include
    )

    target_link_libraries(${name} PRIVATE
      GTest::gtest GTest::gtest_main
      pthread dl
    )

    add_test(NAME ${name} COMMAND ${name})

    # --- E2E env vars (so E2E tests don't skip) ---
    set_tests_properties(${name} PROPERTIES
      ENVIRONMENT
        "E2E_MAPS_DIR=${CMAKE_SOURCE_DIR}/tests/utils/maps;\
          E2E_GM_SO=${CMAKE_SOURCE_DIR}/tests/utils/gm/competition_gm.so;\
          E2E_ALGOS_DIR=${CMAKE_SOURCE_DIR}/tests/utils/competition_algos;\
          E2E_COMP_MAP=${CMAKE_SOURCE_DIR}/tests/utils/comp/comparative_map.txt;\
          E2E_COMP_GM_DIR=${CMAKE_SOURCE_DIR}/tests/utils/comp/comparative_gms;\
          E2E_COMP_ALGO1=${CMAKE_SOURCE_DIR}/tests/utils/comp/comparative_algo1.so;\
          E2E_COMP_ALGO2=${CMAKE_SOURCE_DIR}/tests/utils/comp/comparative_algo2.so"
    )
  endforeach()
endif()
