cmake_minimum_required(VERSION 3.16)
project(PaintStrike VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent for dependency management
include(FetchContent)

# Find raylib (try system first, then fetch if not found)
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    message(STATUS "raylib not found, fetching from GitHub...")
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Find raygui (fetch from GitHub)
FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4.0
)
FetchContent_MakeAvailable(raygui)

# Find ENet (try system first, then fetch if not found)
# Skip ENet for Phase 1 - will be added in Phase 4
# find_package(ENet QUIET)
# if (NOT ENet_FOUND)
#     message(STATUS "ENet not found, fetching from GitHub...")
#     FetchContent_Declare(
#         enet
#         GIT_REPOSITORY https://github.com/lsalzman/enet.git
#         GIT_TAG v1.3.17
#     )
#     FetchContent_MakeAvailable(enet)
# endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing
enable_testing()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(editor)  # Enable editor for development

# Assets are copied by the src/CMakeLists.txt target

# Install targets (skip for now, will add when both targets are ready)
# install(TARGETS paintsplash bsp_editor
#     RUNTIME DESTINATION bin
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
# )

# Package configuration
set(CPACK_PACKAGE_NAME "PaintStrike")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "P2P Pseudo-3D Paint-Shooter Prototype")
set(CPACK_PACKAGE_VENDOR "PaintStrike Team")
include(CPack)
