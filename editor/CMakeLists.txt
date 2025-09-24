# Paint Strike Level Editor CMake Configuration
cmake_minimum_required(VERSION 3.16)
project(PaintStrikeEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find raylib
find_package(raylib REQUIRED)

# Collect all editor source files
file(GLOB_RECURSE EDITOR_SOURCES
    "src/**/*.cpp"
    "src/**/*.hpp"
    "src/**/*.h"
)

# Add Dear ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
)
FetchContent_MakeAvailable(rlimgui)

# Create executable
add_executable(paintstrike_editor
    main.cpp
    ${EDITOR_SOURCES}
    # ImGui source files
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    # rlImGui source files
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)

# Include directories
target_include_directories(paintstrike_editor PRIVATE
    .
    src/
    src/scene/               # Scene management classes
    ../src/                    # Game source for shared utilities
    ../src/world              # BSP tree access
    ../src/utils              # Logger, etc.
    ../src/ecs/Components     # Component definitions
    ../src/rendering          # Rendering utilities
    ${raylib_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${rlimgui_SOURCE_DIR}
)

# Link libraries
target_link_libraries(paintstrike_editor PRIVATE
    raylib
)

# Compiler definitions
target_compile_definitions(paintstrike_editor PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

# Compiler flags
if(MSVC)
    target_compile_options(paintstrike_editor PRIVATE /W4)
else()
    target_compile_options(paintstrike_editor PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy assets (commented out for now - editor uses game assets)
# add_custom_command(TARGET paintstrike_editor POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#     ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR>paintstrike_editor/assets
# )

# Copy editor resources
add_custom_command(TARGET paintstrike_editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:paintstrike_editor>/resources
)

# Install target
install(TARGETS paintstrike_editor
    RUNTIME DESTINATION bin
)
