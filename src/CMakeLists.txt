# PaintSplash Game CMake Configuration

# Collect all source files automatically
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "*.cpp"
    "*.hpp"
    "*.h"
)

# Create executable with all source files
add_executable(paintsplash ${GAME_SOURCES})

# Include directories
target_include_directories(paintsplash PRIVATE
    .
    core
    ecs
    ecs/Components
    ecs/Systems
    rendering
    events
    utils
    networking
    ${raylib_SOURCE_DIR}/src
    ${raygui_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(paintsplash PRIVATE
    raylib
    # raygui  # Skip for now - having linking issues
    # enet  # Skip for Phase 1
)

# Compiler flags
if(MSVC)
    target_compile_options(paintsplash PRIVATE /W4)
else()
    target_compile_options(paintsplash PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Define for raygui
target_compile_definitions(paintsplash PRIVATE
    RAYGUI_IMPLEMENTATION
)

# Copy assets to build directory
add_custom_command(TARGET paintsplash POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:paintsplash>/assets
)

# Copy shaders to build directory
add_custom_command(TARGET paintsplash POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:paintsplash>/shaders
)
