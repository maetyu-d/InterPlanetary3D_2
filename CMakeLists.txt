cmake_minimum_required(VERSION 3.20)
project(InterPlanetary3D
    HOMEPAGE_URL "https://matd.space"
    LANGUAGES CXX
)

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

set(CPACK_PACKAGE_VENDOR "matd.space")

add_executable(interplanetary3d src/main.cpp)
set_target_properties(interplanetary3d PROPERTIES OUTPUT_NAME "InterPlanetary3D")
target_link_libraries(interplanetary3d PRIVATE glfw)

if(APPLE)
    target_compile_definitions(interplanetary3d PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(interplanetary3d PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(interplanetary3d PRIVATE OpenGL::GL)
endif()
