cmake_minimum_required(VERSION 3.20)
project(CubeWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

add_executable(cube_world src/main.cpp)
target_link_libraries(cube_world PRIVATE glfw)

if(APPLE)
    target_compile_definitions(cube_world PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(cube_world PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
