cmake_minimum_required(VERSION 3.16)
project(Conquest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto")
endif()

# Find SDL2
find_package(SDL2 QUIET)

# Find OpenGL
find_package(OpenGL QUIET)

# Source files
set(SOURCES
    src/main.cpp
    src/board.cpp
    src/game.cpp
    src/ai.cpp
    src/renderer.cpp
)

# Include dirs
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
)

# If SDL2 not found via cmake, try pkg-config or manual detection
if(NOT SDL2_FOUND)
    # Try pkg-config
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()

    # Manual fallback - check common locations
    if(NOT SDL2_FOUND)
        # Check for SDL2 in /home/z/.local
        if(EXISTS "/home/z/.local/include/SDL2/SDL.h")
            set(SDL2_INCLUDE_DIRS "/home/z/.local/include")
            set(SDL2_LIBRARIES
                "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0"
            )
            set(SDL2_FOUND TRUE)
        endif()
    endif()
endif()

if(NOT OPENGL_FOUND)
    # Manual OpenGL
    if(EXISTS "/usr/lib/x86_64-linux-gnu/libGL.so.1")
        set(OPENGL_LIBRARIES "/usr/lib/x86_64-linux-gnu/libGL.so.1")
        set(OPENGL_FOUND TRUE)
    endif()
endif()

add_executable(conquest ${SOURCES})

target_include_directories(conquest PRIVATE ${INCLUDE_DIRS})

if(SDL2_FOUND)
    target_include_directories(conquest PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(conquest ${SDL2_LIBRARIES})
else()
    message(FATAL_ERROR "SDL2 not found. Install libsdl2-dev or set SDL2_INCLUDE_DIRS/SDL2_LIBRARIES manually.")
endif()

if(OPENGL_FOUND)
    target_link_libraries(conquest ${OPENGL_LIBRARIES})
else()
    message(FATAL_ERROR "OpenGL not found.")
endif()

# Link math and threading libraries
target_link_libraries(conquest m dl pthread rt)

# Platform-specific - use versioned .so files since dev packages may not be installed
if(UNIX AND NOT APPLE)
    set(X11_LIBS "")
    # Link X11 (we have the .so symlink for this one)
    if(EXISTS "/usr/lib/x86_64-linux-gnu/libX11.so")
        list(APPEND X11_LIBS X11)
    elseif(EXISTS "/usr/lib/x86_64-linux-gnu/libX11.so.6")
        list(APPEND X11_LIBS "/usr/lib/x86_64-linux-gnu/libX11.so.6")
    endif()
    # Other X11 extensions - use versioned .so directly
    foreach(LIB Xi Xcursor Xfixes Xrandr Xinerama Xext)
        set(LIB_PATH "/usr/lib/x86_64-linux-gnu/lib${LIB}.so")
        set(LIB_VER "/usr/lib/x86_64-linux-gnu/lib${LIB}.so.6")
        if(EXISTS "${LIB_PATH}")
            list(APPEND X11_LIBS ${LIB})
        elseif(EXISTS "${LIB_VER}")
            list(APPEND X11_LIBS "${LIB_VER}")
        endif()
    endforeach()
    if(X11_LIBS)
        target_link_libraries(conquest ${X11_LIBS})
    endif()
endif()

# Print build info
message(STATUS "=== Conquest Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "SDL2 include: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 libs: ${SDL2_LIBRARIES}")
message(STATUS "OpenGL libs: ${OPENGL_LIBRARIES}")
message(STATUS "====================================")
