cmake_minimum_required(VERSION 3.25)
project(mmm LANGUAGES C VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MMM_BUILD_TEST "if on, build the test" ${PROJECT_IS_TOP_LEVEL})

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_DEPRECATE)
endif()

add_library(mmm INTERFACE)
target_include_directories(mmm INTERFACE ${CMAKE_CURRENT_LIST_DIR}/)

if(MMM_BUILD_TEST)
    add_executable(mmmtest ${CMAKE_CURRENT_LIST_DIR}/test/mmmtest.c)
    target_link_libraries(mmmtest mmm)
endif()
