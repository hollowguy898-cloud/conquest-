cmake_minimum_required(VERSION 3.16)
project(Conquest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto")
endif()

# ============================================================================
# Shared source files (no SDL2/OpenGL dependency)
# ============================================================================
set(CORE_SOURCES
    src/board.cpp
    src/game.cpp
    src/ai.cpp
    src/state_encoder.cpp
    src/trainer.cpp
)

# Include dirs
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
)

# ============================================================================
# HEADLESS TRAINING BUILD (no SDL2, no OpenGL)
# Builds: conquest_train
# ============================================================================
set(TRAIN_SOURCES
    ${CORE_SOURCES}
    src/train_main.cpp
)

add_executable(conquest_train ${TRAIN_SOURCES})
target_include_directories(conquest_train PRIVATE ${INCLUDE_DIRS})
target_link_libraries(conquest_train m dl pthread rt)

# ============================================================================
# GUI BUILD (requires SDL2 + OpenGL)
# Builds: conquest
# ============================================================================
option(BUILD_GUI "Build the GUI client (requires SDL2 + OpenGL)" ON)

if(BUILD_GUI)
    # Find SDL2
    find_package(SDL2 QUIET)

    # Find OpenGL
    find_package(OpenGL QUIET)

    # If SDL2 not found via cmake, try pkg-config or manual detection
    if(NOT SDL2_FOUND)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(SDL2 QUIET sdl2)
        endif()

        if(NOT SDL2_FOUND)
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
        if(EXISTS "/usr/lib/x86_64-linux-gnu/libGL.so.1")
            set(OPENGL_LIBRARIES "/usr/lib/x86_64-linux-gnu/libGL.so.1")
            set(OPENGL_FOUND TRUE)
        endif()
    endif()

    if(SDL2_FOUND AND OPENGL_FOUND)
        set(GUI_SOURCES
            ${CORE_SOURCES}
            src/main.cpp
            src/renderer.cpp
        )

        add_executable(conquest ${GUI_SOURCES})
        target_include_directories(conquest PRIVATE ${INCLUDE_DIRS})
        target_include_directories(conquest PRIVATE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(conquest ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES})
        target_link_libraries(conquest m dl pthread rt)

        # Platform-specific X11 libraries
        if(UNIX AND NOT APPLE)
            set(X11_LIBS "")
            if(EXISTS "/usr/lib/x86_64-linux-gnu/libX11.so")
                list(APPEND X11_LIBS X11)
            elseif(EXISTS "/usr/lib/x86_64-linux-gnu/libX11.so.6")
                list(APPEND X11_LIBS "/usr/lib/x86_64-linux-gnu/libX11.so.6")
            endif()
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

        message(STATUS "GUI build: ENABLED (SDL2 + OpenGL found)")
    else()
        message(STATUS "GUI build: DISABLED (SDL2 or OpenGL not found)")
        message(STATUS "  SDL2 found: ${SDL2_FOUND}")
        message(STATUS "  OpenGL found: ${OPENGL_FOUND}")
        message(STATUS "  Only conquest_train will be built.")
    endif()
else()
    message(STATUS "GUI build: DISABLED (BUILD_GUI=OFF)")
endif()

# ============================================================================
# Build info
# ============================================================================
message(STATUS "=== Conquest Build Configuration ===")
message(STATUS "Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "Headless train: ALWAYS BUILT (conquest_train)")
message(STATUS "GUI client:     ${BUILD_GUI}")
message(STATUS "====================================")
