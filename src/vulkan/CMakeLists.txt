add_library(tuke_vulkan STATIC vulkan_base.cpp)

target_include_directories(tuke_vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/glfw/include
    ${PROJECT_SOURCE_DIR}/third_party/include
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(tuke_vulkan PUBLIC Vulkan::Vulkan glfw)

target_compile_options(tuke_vulkan PRIVATE
    -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -Wno-c99-designator)
target_link_options(tuke_vulkan PUBLIC -fsanitize=address,undefined)
