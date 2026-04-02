// ── Sokol single-TU implementation ───────────────────────────────────────────
// On macOS this file is compiled as Objective-C++ via COMPILE_FLAGS in CMake.

#define SOKOL_GFX_IMPL
#include "sokol_gfx.h"

#define SOKOL_APP_IMPL
#include "sokol_app.h"

#define SOKOL_GLUE_IMPL
#include "sokol_glue.h"

#define SOKOL_LOG_IMPL
#include "sokol_log.h"

// sokol_imgui requires imgui.h to already be included
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "util/sokol_imgui.h"
