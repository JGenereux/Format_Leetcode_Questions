cmake_minimum_required(VERSION 3.10)
project(GenLeetcodeQuestion)

# Enable testing
enable_testing()

# Specify vcpkg toolchain if available
if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

# Try to find nlohmann_json via CONFIG first, then fallback to system
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(nlohmann_json REQUIRED)
endif()

find_package(CURL REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Main executable
add_executable(main main.cpp)

target_link_libraries(main PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(main PRIVATE CURL::libcurl)

# Try to find Google Test
find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    # Try to find GTest manually if CONFIG mode fails
    find_path(GTEST_INCLUDE_DIR gtest/gtest.h PATHS /usr/include /usr/local/include)
    find_library(GTEST_LIBRARY gtest PATHS /usr/lib /usr/local/lib)
    find_library(GTEST_MAIN_LIBRARY gtest_main PATHS /usr/lib /usr/local/lib)
    
    if(GTEST_INCLUDE_DIR AND GTEST_LIBRARY AND GTEST_MAIN_LIBRARY)
        set(GTest_FOUND TRUE)
        message(STATUS "Google Test found manually")
    endif()
endif()

if(GTest_FOUND)
    message(STATUS "Google Test found, building tests")
    
    # Test executable
    add_executable(test_main test_main.cpp)
    
    if(TARGET GTest::gtest)
        target_link_libraries(test_main PRIVATE 
            nlohmann_json::nlohmann_json
            CURL::libcurl
            GTest::gtest 
            GTest::gtest_main
        )
    else()
        target_include_directories(test_main PRIVATE ${GTEST_INCLUDE_DIR})
        target_link_libraries(test_main PRIVATE 
            nlohmann_json::nlohmann_json
            CURL::libcurl
            ${GTEST_LIBRARY}
            ${GTEST_MAIN_LIBRARY}
            pthread
        )
    endif()
    
    # Add test to CTest
    add_test(NAME MainTests COMMAND test_main)
else()
    message(WARNING "Google Test not found. Install with: sudo apt-get install libgtest-dev")
    message(WARNING "Tests will not be built. Install GTest to enable testing.")
endif()