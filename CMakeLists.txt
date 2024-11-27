cmake_minimum_required(VERSION 3.15)
project(DocumentEditor VERSION 1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard (if necessary, although default is usually fine)
set(CMAKE_C_STANDARD 99)

# Paths for dependencies
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(LUNASVG_DIR ${CMAKE_SOURCE_DIR}/external/lunasvg)
set(GLFW_DIR ${CMAKE_SOURCE_DIR}/external/glfw)
set(GLAD_DIR ${CMAKE_SOURCE_DIR}/external/glad)

# Include directories
include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${LUNASVG_DIR}/include
    ${GLAD_DIR}/include
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

# Add LunaSVG library (another submodule)
add_subdirectory(${LUNASVG_DIR} lunasvg)

# Create the executable
add_executable(DocumentEditor ${SOURCES})

# Link libraries
target_link_libraries(DocumentEditor PRIVATE lunasvg glad glfw)

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
