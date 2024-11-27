cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(copengl)

set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall -Wextra -Wpedantic -Fsanitize=address)

file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/main.c
    ${CMAKE_SOURCE_DIR}/src/gl.c
)

add_executable(
    ${PROJECT_NAME}
    ${SOURCE_FILES}
)

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/glfw/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/include)
target_link_libraries(${PROJECT_NAME} glfw)
