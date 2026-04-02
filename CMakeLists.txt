cmake_minimum_required(VERSION 3.25)
project(Shadebug VERSION 0.1.0 LANGUAGES CXX C OBJCXX)

# ── C++ standard (targets override this with cxx_std_23) ─────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ── FetchContent ─────────────────────────────────────────────────────────────
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# sokol — use existing submodule (no build needed, header-only)
set(SOKOL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/sokol"
    CACHE PATH "sokol single-header directory")

# imgui — docking branch tip (pinned to latest as of 2026-04-03)
# f5f6ca07 = tip of 'docking' branch with ImGui 1.92.x + RendererHasTextures
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        f5f6ca07be7ce0ea9eed6c04d55833bac3f6b50b
    GIT_SHALLOW    TRUE
)

# nlohmann/json — latest release
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)

# stb — latest master (no release tags; pinned commit 2026-04-03)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        28d546d5eb77d4585506a20480f4de2e706dff4c
    GIT_SHALLOW    TRUE
)

# ImGuiFileDialog — pure-ImGui cross-platform file chooser
# (macOS / Linux / Windows / Emscripten / Android; also works on iOS via
#  std::filesystem sandbox navigation — no native iOS picker)
FetchContent_Declare(ImGuiFileDialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG        d0e97b2adc3d3452d72c750c7305dc0291acd052
    GIT_SHALLOW    FALSE
)

FetchContent_MakeAvailable(imgui nlohmann_json stb ImGuiFileDialog)

set(STB_INCLUDE_DIR "${stb_SOURCE_DIR}" CACHE PATH "stb single-headers directory")
set(IGFD_INCLUDE_DIR "${imguifiledialog_SOURCE_DIR}" CACHE PATH "ImGuiFileDialog source directory")

# ImGuiFileDialog's own CMakeLists.txt doesn't know about our imgui target —
# give it the header directory so it compiles without errors.
target_include_directories(ImGuiFileDialog PRIVATE ${imgui_SOURCE_DIR})

# ── imgui static library ──────────────────────────────────────────────────────
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_compile_features(imgui PRIVATE cxx_std_17)
# Suppress warnings in third-party code
target_compile_options(imgui PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-w>
    $<$<CXX_COMPILER_ID:MSVC>:/w>
)

# ── DuckDB (optional) ─────────────────────────────────────────────────────────
set(DUCKDB_FOUND FALSE)
if(ENABLE_DUCKDB)
    find_library(DUCKDB_LIB NAMES duckdb libduckdb)
    find_path(DUCKDB_INCLUDE NAMES duckdb.hpp duckdb.h)

    if(DUCKDB_LIB AND DUCKDB_INCLUDE)
        add_library(DuckDB UNKNOWN IMPORTED)
        set_target_properties(DuckDB PROPERTIES
            IMPORTED_LOCATION           "${DUCKDB_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${DUCKDB_INCLUDE}"
        )
        set(DUCKDB_FOUND TRUE)
        message(STATUS "DuckDB found: ${DUCKDB_LIB}")
    else()
        message(WARNING
            "DuckDB not found — disabling.\n"
            "Install DuckDB and re-run cmake, or set DUCKDB_LIB + DUCKDB_INCLUDE.")
        set(ENABLE_DUCKDB OFF)
    endif()
endif()

# ── Application ───────────────────────────────────────────────────────────────
add_subdirectory(src)
add_subdirectory(tests)
