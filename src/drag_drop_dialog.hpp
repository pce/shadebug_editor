#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace shadebug { class App; }

/// Panel for opening .docjson documents, images, or data files via
/// drag-and-drop onto the sokol window or via the Open dialog.
class DragDropDialog {
public:
    /// Call once per frame to render any active drop overlay.
    void draw(shadebug::App& app);

    /// Feed a sokol SAPP_EVENTTYPE_FILES_DROPPED event.
    void on_files_dropped(shadebug::App& app,
                          const std::vector<std::filesystem::path>& paths);

private:
    bool        show_overlay_    = false;
    std::string drop_hint_;
};
