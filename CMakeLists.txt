cmake_minimum_required(VERSION 3.16)
project(tracer VERSION 1.0 LANGUAGES CXX C)

include(stdcpp.cmake)
include(sanitizers.cmake)

enable_testing()

add_executable(tracer tracer.cpp)

add_test(NAME tracer_cat-and-mouse-cheese
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND $<TARGET_FILE:tracer> cat-and-mouse.if cat-and-mouse-cheese.xtr)

add_test(NAME tracer_cat-and-mouse-1
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND $<TARGET_FILE:tracer> cat-and-mouse.if cat-and-mouse-1.xtr)
