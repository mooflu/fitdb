cmake_minimum_required(VERSION 3.11)

project(fitdb C CXX)

if (NOT VERSION)
    find_package (Git)
    if (GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --always --tags "--abbrev=0"
            OUTPUT_VARIABLE VERSION)
    endif (GIT_FOUND)
endif()

if (NOT VERSION)
    message( FATAL_ERROR "VERSION not specified or no git tag found" )
endif()

string(STRIP ${VERSION} VERSION)
message("VERSION: ${VERSION}")

add_definitions(-DVERSION="${VERSION}")
if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_DEBUG_POSTFIX "_d")

add_subdirectory(${PROJECT_SOURCE_DIR}/fit_sdk)
add_subdirectory(${PROJECT_SOURCE_DIR}/sqlite)
add_subdirectory(${PROJECT_SOURCE_DIR}/src)
