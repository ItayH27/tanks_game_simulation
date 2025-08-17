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
# Optional: outer-scope tests (GoogleTest)
# -------------------------
option(BUILD_TESTS "Build tests in tests/ (outer scope)" OFF)
option(FETCH_GTEST "Fetch GoogleTest if not found" OFF)

if(BUILD_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/tests")
    include(CTest)
    enable_testing()

    find_package(GTest QUIET)
    if(NOT GTest_FOUND AND FETCH_GTEST)
        include(FetchContent)
        FetchContent_Declare(
                googletest
                URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    if(GTest_FOUND OR TARGET gtest)
        file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
        foreach(src ${TEST_SOURCES})
            get_filename_component(name ${src} NAME_WE)
            add_executable(${name} ${src}
                    UserCommon/UC_src/ExtSatelliteView.cpp)
            target_compile_features(${name} PRIVATE cxx_std_17)

            target_include_directories(${name} PRIVATE
                    ${CMAKE_SOURCE_DIR}/common
                    ${CMAKE_SOURCE_DIR}/UserCommon/UC_include
                    ${CMAKE_SOURCE_DIR}/Simulator/sim_include
            )
            target_link_libraries(${name} PRIVATE
                    GTest::gtest GTest::gtest_main
                    pthread dl
            )
            add_test(NAME ${name} COMMAND ${name})
        endforeach()
    else()
        message(WARNING "GTest not found and FETCH_GTEST=OFF; skipping tests.")
    endif()
endif()
