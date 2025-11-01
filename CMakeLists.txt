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

add_executable(opengl_test ${CMAKE_SOURCE_DIR}/app/opengl_test/opengl_test.cpp)
target_link_libraries(opengl_test PRIVATE engine)

add_executable(top_down_something ${CMAKE_SOURCE_DIR}/app/top_down_something/main.cpp)
target_link_libraries(top_down_something PRIVATE engine)


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

# reflector stuff
option(DEBUG_REFLECTOR "Build reflector in debug mode, default to opimized for faster engine dev" OFF)
add_executable(reflector ${REFLECTOR_SOURCE_FILES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR DEBUG_REFLECTOR)
    message("Building reflector in debug mode")
    target_compile_options(reflector PRIVATE
        -g
        -Wall -Wextra -Wpedantic -Werror
        -fsanitize=address
    )
    target_link_options(reflector PRIVATE
        -fsanitize=address
    )
else() # Release, RelWithDebInfo, etc.
    message("Building reflector in release mode")
    target_compile_options(reflector PRIVATE
        -O3
        -Wall -Wextra -Wpedantic -Werror
        -DNDEBUG
    )
endif()
