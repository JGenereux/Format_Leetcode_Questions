cmake_minimum_required(VERSION 3.10)
project(GenLeetcodeQuestion)

# Enable C++11 standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Specify vcpkg toolchain (only if on Windows with vcpkg)
if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Create a library with the utility functions
add_library(utils utils.cpp)
target_link_libraries(utils PUBLIC nlohmann_json::nlohmann_json)

# Main executable
add_executable(main main.cpp)
target_link_libraries(main PRIVATE utils nlohmann_json::nlohmann_json CURL::libcurl)

# Test executable (optional, requires Google Test)
option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    # Try to find Google Test
    find_package(GTest QUIET)
    
    if(GTest_FOUND)
        enable_testing()
        
        add_executable(test_main test_main.cpp)
        target_link_libraries(test_main PRIVATE utils GTest::gtest GTest::gtest_main)
        
        # Add test to CTest
        add_test(NAME UtilsTests COMMAND test_main)
        
        message(STATUS "Tests enabled - run 'ctest' or './test_main' to execute tests")
    else()
        message(WARNING "Google Test not found. Tests will not be built. Install with: sudo apt-get install libgtest-dev (Linux) or vcpkg install gtest (Windows)")
    endif()
endif()