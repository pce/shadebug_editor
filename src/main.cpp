#include "app.hpp"
#include "sokol_log.h"

namespace {
    constexpr int kWindowWidth       = 1280;
    constexpr int kWindowHeight      = 800;
    constexpr int kMaxDroppedFiles   = 16;
}

/// Entry point — sokol_app calls sokol_main() to get the app descriptor.
sapp_desc sokol_main(int /*argc*/, char** /*argv*/) {
    return sapp_desc{
        .init_cb    = shadebug::App::init_cb,
        .frame_cb   = shadebug::App::frame_cb,
        .cleanup_cb = shadebug::App::cleanup_cb,
        .event_cb   = shadebug::App::event_cb,
        .width              = kWindowWidth,
        .height             = kWindowHeight,
        .window_title       = "Shadebug",
        .enable_clipboard   = true,            // system copy/paste in text editor
        .clipboard_size     = 1024 * 1024,     // 1 MiB — enough for full shader source
        .enable_dragndrop   = true,
        .max_dropped_files  = kMaxDroppedFiles,
        .icon.sokol_default = true,
        .logger.func        = slog_func,
    };
}
