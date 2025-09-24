# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-src")
  file(MAKE_DIRECTORY "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-src")
endif()
file(MAKE_DIRECTORY
  "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-build"
  "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix"
  "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix/tmp"
  "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix/src/rlimgui-populate-stamp"
  "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix/src"
  "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix/src/rlimgui-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix/src/rlimgui-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/aimac/Development/paintsplash/SGDA-Fall-Game-Jam-2025/build_editor/_deps/rlimgui-subbuild/rlimgui-populate-prefix/src/rlimgui-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
