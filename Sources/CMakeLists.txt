cmake_minimum_required(VERSION 3.0)

set (CMAKE_CXX_STANDARD 17)
add_executable (${PROJECT_NAME} main.cpp)
# set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${SOLUTION_ROOT})
target_link_libraries (${PROJECT_NAME} LINK_PRIVATE raylib-ext)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
