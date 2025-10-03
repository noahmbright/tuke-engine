cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(game_engine)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/src)

add_executable(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/app/og_main/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE engine)

add_executable(vulkan_test ${CMAKE_SOURCE_DIR}/app/vulkan_test/vulkan_test.cpp)
target_link_libraries(vulkan_test PRIVATE engine)

file(GLOB_RECURSE PONG_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/app/pong/pong.cpp
    ${CMAKE_SOURCE_DIR}/app/pong/main.cpp
)

add_executable(pong ${PONG_SOURCE_FILES})
target_link_libraries(pong PRIVATE engine)

file(GLOB_RECURSE REFLECTOR_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/reflector/main.cpp
    ${CMAKE_SOURCE_DIR}/reflector/filesystem_utils.cpp
    ${CMAKE_SOURCE_DIR}/reflector/parser.cpp
    ${CMAKE_SOURCE_DIR}/reflector/codegen.cpp
    ${CMAKE_SOURCE_DIR}/reflector/subprocess.cpp
)

add_executable(reflector ${REFLECTOR_SOURCE_FILES})
target_compile_options(reflector PRIVATE 
    -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address)
target_link_options(reflector PUBLIC -fsanitize=address)
