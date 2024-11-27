cmake_minimum_required(VERSION 3.15)
project(DocumentEditor VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths for dependencies
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(LUNASVG_DIR ${CMAKE_SOURCE_DIR}/external/lunasvg)

# Include directories
include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${LUNASVG_DIR}/include
)

# Source files
set(SOURCES
    src/main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

# Add LunaSVG library
add_subdirectory(${LUNASVG_DIR} lunasvg)

# Add GLFW as a library
add_subdirectory(${CMAKE_SOURCE_DIR}/external/glfw)


# Executable
add_executable(DocumentEditor ${SOURCES})

# Link libraries
target_link_libraries(DocumentEditor PRIVATE lunasvg)

if(APPLE)
    target_link_libraries(DocumentEditor PRIVATE glfw "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreFoundation")
else()
    target_link_libraries(DocumentEditor PRIVATE glfw OpenGL)
endif()

