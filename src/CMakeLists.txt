set(APP_NAME shadebug)

# Binary output: build/ (not build/src/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Sources
set(APP_SOURCES
    main.cpp
    app.cpp
    utils.cpp
    drag_drop_dialog.cpp
    sokol_impl.cpp
    panels/canvas_panel.cpp
    panels/svg_preview.cpp
    panels/properties_panel.cpp
    panels/layers_panel.cpp
    panels/text_editor_panel.cpp
    panels/shader_list_panel.cpp
    panels/shader_render_panel.cpp
    panels/sdf_panel.cpp
    renderer/gpu_renderer.cpp
    renderer/effect_renderer.cpp
    renderer/scene_renderer_3d.cpp
    renderer/shader_registry.cpp
    renderer/shader_params.cpp
    ui/theme.cpp
    ui/settings.cpp
    ui/image_cache.cpp
    ui/stb_impl.cpp
    ${IGFD_INCLUDE_DIR}/ImGuiFileDialog.cpp
)

add_executable(${APP_NAME} ${APP_SOURCES})

#  C++23
target_compile_features(${APP_NAME} PRIVATE cxx_std_23)

#  Include paths
target_include_directories(${APP_NAME}
    PRIVATE
        ${SOKOL_INCLUDE_DIR}            # sokol_app.h, sokol_gfx.h, util/sokol_imgui.h
        ${imgui_SOURCE_DIR}             # imgui.h (needed by sokol_imgui.h)
        ${STB_INCLUDE_DIR}             # stb_image.h
        ${IGFD_INCLUDE_DIR}             # ImGuiFileDialog.h
        ${CMAKE_CURRENT_SOURCE_DIR}     # app.hpp, panels/, inspector/, …
)

# Platform backend + sokol_impl ObjC++
#
#  sokol_impl.cpp contains all SOKOL_*_IMPL defines and must be compiled with
#  the correct backend define.  On Apple it must be Objective-C++ because
#  sokol_app.h uses MetalKit / Cocoa APIs.
#
if (APPLE)
    target_compile_definitions(${APP_NAME} PRIVATE SOKOL_METAL)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/sokol_impl.cpp
        PROPERTIES COMPILE_FLAGS "-x objective-c++"
    )
    target_link_libraries(${APP_NAME} PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework AudioToolbox"
    )

elseif (WIN32)
    target_compile_definitions(${APP_NAME} PRIVATE SOKOL_D3D11)
    target_link_libraries(${APP_NAME} PRIVATE d3d11 dxgi)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_compile_definitions(${APP_NAME} PRIVATE SOKOL_WGPU)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_link_options(${APP_NAME} PRIVATE
        "-s USE_WEBGPU=1"
        "-s ASYNCIFY"
        "-s WASM=1"
    )

else()
    # Linux / other POSIX — OpenGL core profile
    target_compile_definitions(${APP_NAME} PRIVATE SOKOL_GLCORE)
    find_package(OpenGL REQUIRED)
    find_package(X11 QUIET)
    target_link_libraries(${APP_NAME} PRIVATE OpenGL::GL dl pthread)
    if (X11_FOUND)
        target_link_libraries(${APP_NAME} PRIVATE X11::X11)
    endif()
endif()

# Library dependencies
target_link_libraries(${APP_NAME}
    PRIVATE
        imgui
        nlohmann_json::nlohmann_json
)

# Resources & data
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(
        TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:${APP_NAME}>/resources
        COMMENT "Copying resources → build directory"
    )
endif()

# Copy data/ (shaders, fonts, etc.) next to the binary
add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:${APP_NAME}>/data
    COMMENT "Copying data/ → build directory"
)

# Explicit font copy so fonts are always up-to-date next to the binary
add_custom_command(
    TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data/fonts
        $<TARGET_FILE_DIR:${APP_NAME}>/data/fonts
    COMMENT "Copying fonts → build directory"
)
