file(GLOB_RECURSE ENGINE_SOURCE_FILES
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
    ${CMAKE_SOURCE_DIR}/src/transform.cpp
    ${CMAKE_SOURCE_DIR}/src/physics.cpp
)

add_library(engine STATIC ${ENGINE_SOURCE_FILES})

target_link_libraries(engine 
    PUBLIC Vulkan::Vulkan
    PUBLIC glfw
)


target_include_directories(engine
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/third_party/glfw/include
        ${PROJECT_SOURCE_DIR}/third_party/include
        ${PROJECT_SOURCE_DIR}/gen
        ${Vulkan_INCLUDE_DIRS}
)

target_compile_options(engine PRIVATE 
    -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address)
target_link_options(engine PUBLIC -fsanitize=address)
