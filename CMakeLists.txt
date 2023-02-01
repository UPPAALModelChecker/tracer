cmake_minimum_required(VERSION 3.16)
project(tracer VERSION 1.0 LANGUAGES CXX)

include(stdcpp.cmake)
include(sanitizers.cmake)

option(CORA "Priced Timed Automata support from Uppaal CORA" OFF)
option(TRACER_STATIC "Static Linking" OFF)

if (CORA)
    add_compile_definitions(ENABLE_CORA)
    message(STATUS "Enabled Priced Timed Automata from Uppaal CORA")
endif(CORA)

if (TRACER_STATIC)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_link_options(-static)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_link_options(-static)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_link_options(/MT)
    endif()
endif(TRACER_STATIC)

enable_testing()

add_executable(tracer tracer.cpp)

add_test(NAME tracer_cat-and-mouse-cheese
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND $<TARGET_FILE:tracer> cat-and-mouse.if cat-and-mouse-cheese.xtr)

add_test(NAME tracer_cat-and-mouse-1
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND $<TARGET_FILE:tracer> cat-and-mouse.if cat-and-mouse-1.xtr)
