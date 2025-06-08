cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(game_engine)

find_package(Vulkan REQUIRED)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 23)
add_compile_options(-Wall -Wextra -Wpedantic -Fsanitize=address)

file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/gl.c
    ${CMAKE_SOURCE_DIR}/src/shaders.cpp
    ${CMAKE_SOURCE_DIR}/src/camera.cpp
    ${CMAKE_SOURCE_DIR}/src/tilemap.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer.cpp
    ${CMAKE_SOURCE_DIR}/src/window.cpp
    ${CMAKE_SOURCE_DIR}/src/player.cpp
    ${CMAKE_SOURCE_DIR}/src/vulkan_base.cpp
    ${CMAKE_SOURCE_DIR}/src/stb_image.cpp
    ${CMAKE_SOURCE_DIR}/src/utils.cpp
)

add_library(engine ${SOURCE_FILES})

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw)
include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/third_party/glfw/include)
include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/third_party/include)
include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/gen)

if(Vulkan_FOUND)
    include_directories(engine PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(engine PRIVATE Vulkan::Vulkan) 
endif()

link_libraries(engine glfw)

add_executable(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_executable(vulkan_test ${CMAKE_SOURCE_DIR}/src/vulkan_test.cpp)
