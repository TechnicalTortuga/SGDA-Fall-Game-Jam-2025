# BSP Editor CMake Configuration

# Collect all editor source files automatically
file(GLOB_RECURSE EDITOR_SOURCES
    "*.cpp"
    "*.hpp"
    "*.h"
)

# Create executable with all source files
add_executable(bsp_editor ${EDITOR_SOURCES})

# Include directories (share some with game)
target_include_directories(bsp_editor PRIVATE
    .
    ${CMAKE_SOURCE_DIR}/src/world    # Share BSPTree
    ${CMAKE_SOURCE_DIR}/src/utils    # Share Logger
    ${raylib_SOURCE_DIR}/src
    ${raygui_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(bsp_editor PRIVATE
    raylib
    raygui
)

# Compiler flags
if(MSVC)
    target_compile_options(bsp_editor PRIVATE /W4)
else()
    target_compile_options(bsp_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Define for raygui
target_compile_definitions(bsp_editor PRIVATE
    RAYGUI_IMPLEMENTATION
)

# Copy assets to build directory
add_custom_command(TARGET bsp_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:bsp_editor>/assets
)
