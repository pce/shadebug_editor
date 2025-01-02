cmake_minimum_required(VERSION 3.15)
project(DocumentEditor VERSION 1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard (if necessary, although default is usually fine)
set(CMAKE_C_STANDARD 99)

# Paths for dependencies
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/external/glfw)
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/external/glad)
set(THORVG_DIR ${CMAKE_SOURCE_DIR}/external/thorvg)
set(THORVG_BUILD_DIR ${CMAKE_BINARY_DIR}/thorvg_build) # Ensure build dir is separate

# Include directories
include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLAD_DIR}/include
    ${THORVG_DIR}/inc
)

# Source files for the project
set(SOURCES
    src/main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

# Add GLAD library (ensure it's added as a C library)
add_library(glad STATIC ${GLAD_DIR}/src/glad.c)

# Make sure to link glad correctly
target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

# Add GLFW library (as a submodule)
add_subdirectory(${GLFW_DIR})

# Build ThorVG using ExternalProject
include(ExternalProject)
ExternalProject_Add(thorvg_project
    PREFIX ${THORVG_BUILD_DIR} # Ensure prefix is set to build directory
    SOURCE_DIR ${THORVG_DIR}
    BINARY_DIR ${THORVG_BUILD_DIR}
    CONFIGURE_COMMAND meson setup ${THORVG_BUILD_DIR} ${THORVG_DIR} # Corrected paths
    BUILD_COMMAND ninja -C ${THORVG_BUILD_DIR}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${THORVG_BUILD_DIR}/src/libthorvg.dylib
)

# Create an interface target for ThorVG dynamic library
add_library(thorvg_lib SHARED IMPORTED)
set_target_properties(thorvg_lib PROPERTIES
    IMPORTED_LOCATION ${THORVG_BUILD_DIR}/src/libthorvg.dylib
)
add_dependencies(thorvg_lib thorvg_project)

# Create the executable
add_executable(DocumentEditor ${SOURCES})

# Link libraries
target_link_libraries(DocumentEditor PRIVATE glad glfw thorvg_lib)

# Platform-specific handling (macOS, Windows, etc.)
if(APPLE)
    target_link_libraries(DocumentEditor PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
elseif(EMSCRIPTEN)
    target_compile_definitions(DocumentEditor PUBLIC -DGL_ES)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(DocumentEditor PRIVATE OpenGL::GL)
endif()
