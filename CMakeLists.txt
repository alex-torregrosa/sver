cmake_minimum_required(VERSION 3.10)

project(sver VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include slang, disabling the test compilation
set(SLANG_INCLUDE_TESTS OFF CACHE BOOL "Include tests in slang build")
add_subdirectory(libs/slang)
# Disable slang targets
set_target_properties(driver rewriter slangruntime PROPERTIES EXCLUDE_FROM_ALL 1 EXCLUDE_FROM_DEFAULT_BUILD 1)

# Build lspcpp library
set(LSPCPP_BUILD_WEBSOCKETS OFF)
add_subdirectory(libs/lspcpp)

set(SOURCES 
    src/main.cpp
    src/serverHandlers.cpp
    src/DiagnosticParser.cpp
    src/ProjectSources.cpp
    src/NodeVisitor.cpp
    src/CompletionHandler.cpp
)
# The real exec
add_executable(sver ${SOURCES})

target_link_libraries(sver PRIVATE slangcompiler)
target_link_libraries(sver PRIVATE lspcpp)
target_include_directories(sver PRIVATE ${LSPCPP_INCLUDE_DIR})
