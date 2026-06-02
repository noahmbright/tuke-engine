cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(game_engine)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)

# SDL3: dev uses system install (dynamic), shipping uses vendored static build.
# To ship: add SDL as a git submodule at third_party/sdl, then:
#   cmake -DSDL_STATIC_LINK=ON ...
# and link against SDL3::SDL3-static instead.
# TODO SDL_STATIC_LINK needs separated into two options:
#      One for using SDL vs GLFW. One for static vs. dynamic linking.
option(SDL_STATIC_LINK "Link SDL3 statically for shipping" OFF)
if(SDL_STATIC_LINK)
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/sdl)
else()
    find_package(SDL3 REQUIRED)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/src)

function(add_game_executable target)
    target_link_libraries(${target} PRIVATE engine)
    target_compile_options(${target} PRIVATE
        -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -Wno-c99-designator)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endfunction()

add_executable(opengl_test ${CMAKE_SOURCE_DIR}/app/opengl_test/opengl_test.cpp)
add_game_executable(opengl_test)

add_executable(top_down_something ${CMAKE_SOURCE_DIR}/app/top_down_something/main.cpp)
add_game_executable(top_down_something)

file(GLOB_RECURSE PONG_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/app/pong/pong.cpp
    ${CMAKE_SOURCE_DIR}/app/pong/main.cpp
)

add_executable(pong ${PONG_SOURCE_FILES})
add_game_executable(pong)

file(GLOB VULKAN_TEST_FILES ${CMAKE_SOURCE_DIR}/app/vulkan_test/*.cpp)
foreach(test_file ${VULKAN_TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    add_game_executable(${test_name})
endforeach()

file(GLOB_RECURSE REFLECTOR_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/reflector/main.cpp
    ${CMAKE_SOURCE_DIR}/reflector/filesystem_utils.cpp
    ${CMAKE_SOURCE_DIR}/reflector/parser.cpp
    ${CMAKE_SOURCE_DIR}/reflector/codegen.cpp
    ${CMAKE_SOURCE_DIR}/reflector/subprocess.cpp
)

# Reflector
option(DEBUG_REFLECTOR "Build reflector in debug mode, default to optimized for faster engine dev" OFF)
add_executable(reflector ${REFLECTOR_SOURCE_FILES})
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR DEBUG_REFLECTOR)
    message("Building reflector in debug mode")
    target_compile_options(reflector PRIVATE
        -g
        -Wall -Wextra -Wpedantic -Werror -Wno-c99-designator
        -fsanitize=address
    )
    target_link_options(reflector PRIVATE
        -fsanitize=address
    )
else() # Release, RelWithDebInfo, etc.
    message("Building reflector in release mode")
    target_compile_options(reflector PRIVATE
        -O3
        -Wall -Wextra -Wpedantic -Werror -Wno-c99-designator
        -DNDEBUG
    )
endif()
