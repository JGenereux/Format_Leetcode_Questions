cmake_minimum_required(VERSION 3.10)
project(GenLeetcodeQuestion)

# Specify vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)

set(CURL_INCLUDE_DIR "C:/vcpkg/installed/x64-windows/include")
set(CURL_LIBRARY "C:/vcpkg/installed/x64-windows/lib/libcurl.a")
set(CMAKE_BUILD_TYPE Debug)
add_executable(main main.cpp)

add_executable(unit_tests tests/test_main.cpp)

target_link_libraries(main PRIVATE nlohmann_json::nlohmann_json)

target_link_libraries(main PRIVATE CURL::libcurl)

target_link_libraries(unit_tests PRIVATE nlohmann_json::nlohmann_json)