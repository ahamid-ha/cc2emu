cmake_minimum_required(VERSION 3.10)
project(cc2emu)

# Set the output directory for executables and libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

# Include directories
include_directories(include)

# Source files
file(GLOB SRC_FILES "src/*.c")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows-specific settings
    find_package(unofficial-libconfuse CONFIG REQUIRED)
else ()
    # Unix-like systems (Linux, macOS, etc.)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBCONFUSE REQUIRED libconfuse)
endif ()

# Find required packages
find_package(SDL3_image CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

#include_directories(${SDL3_INCLUDE_DIRS} ${LibConfuse_INCLUDE_DIRS} ${SDL3_IMAGE_INCLUDE_DIRS})
add_executable(cc2emu ${SRC_FILES})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows-specific linking
    target_link_libraries(cc2emu
        SDL3::SDL3
        unofficial::libconfuse::libconfuse
        $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>
    )
else ()
    # Unix-like systems linking
    target_link_libraries(cc2emu
        SDL3::SDL3
        ${LIBCONFUSE_LIBRARIES}
        $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>
        m
    )
endif ()
