cmake_minimum_required(VERSION 3.10)

project(sver VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include slang, disabling the test compilation
set(SLANG_INCLUDE_TESTS OFF CACHE BOOL "Include tests in slang build")
add_subdirectory(libs/slang)

# Build lspcpp library
add_subdirectory(libs/lspcpp)

# The real exec
add_executable(sver src/main.cpp)

target_link_libraries(sver PRIVATE slangcompiler)
target_link_libraries(sver PRIVATE lspcpp)
target_include_directories(sver PRIVATE ${LSPCPP_INCLUDE_DIR})
