#include "drag_drop_dialog.hpp"
#include "app.hpp"
#include "imgui.h"
#include <algorithm>

namespace {

/// Classify a file path into a broad category.
enum class FileKind { Document, Image, Data, Unknown };

FileKind classify(const std::filesystem::path& p) {
    const auto ext = p.extension().string();
    auto low = ext;
    std::ranges::transform(low, low.begin(), ::tolower);

    if (low == ".json") return FileKind::Document;
    if (low == ".png" || low == ".jpg" || low == ".jpeg" ||
        low == ".bmp" || low == ".tga")      return FileKind::Image;
    if (low == ".db"  || low == ".duckdb" ||
        low == ".csv" || low == ".parquet") return FileKind::Data;
    return FileKind::Unknown;
}

} // namespace

void DragDropDialog::on_files_dropped(
    shadebug::App& app, const std::vector<std::filesystem::path>& paths)
{
    if (paths.empty()) return;

    // Handle first recognised file of each kind
    bool handled = false;

    for (const auto& p : paths) {
        switch (classify(p)) {
        case FileKind::Document:
            app.open_document(p);
            handled = true;
            break;
        case FileKind::Image:
            app.image_cache.GetOrLoad(p.string());
            app.set_status("Image loaded: " + p.filename().string());
            handled = true;
            break;
        case FileKind::Data:
            app.set_status("Cannot open data file — DuckDB browser removed: " +
                           p.filename().string());
            break;
        case FileKind::Unknown:
            app.set_status("Unknown file type: " + p.filename().string());
            break;
        }
        if (handled) break;
    }
}

void DragDropDialog::draw(shadebug::App& /*app*/) {
    // Drag-over visual feedback is handled directly by the OS / sokol;
    // this function is a hook for future custom overlay rendering.
}
