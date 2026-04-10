#pragma once

#include "shader_params.hpp"
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
namespace shadebug::renderer {

// PipelineType

enum class PipelineType {
    Rect,    ///< Instanced SDF rounded-rect pipeline (GpuRenderer)
    Effect,  ///< Fullscreen procedural / post-process pipeline (EffectRenderer)
};

// ShaderEntry

struct ShaderEntry {
    std::string name;
    std::string vs_src;
    std::string fs_src;
    std::filesystem::path vs_path;
    std::filesystem::path fs_path;
    std::vector<std::string> roles;      // e.g. {"page_background","element_fill"}
    std::string draw_desc;               // raw JSON draw config from gpu_pipeline.json
    PipelineType pipeline_type = PipelineType::Rect;
    std::string description;             // human-readable description from JSON
    std::vector<ShaderParam> params;     // interactive / animated parameters
};

// ShaderRegistry
//
//  Flat list of named shader pairs.  One entry is "selected"; any listener
//  registered via on_selection_change() is called immediately when the
//  selection changes.
//
//  Lifetime: singleton via get().
//

class ShaderRegistry {
public:
    static ShaderRegistry& get();

    // Registration

    /// Add a shader from in-memory strings.
    int add(std::string name, std::string vs_src, std::string fs_src);

    /// Add a shader from on-disk files (reads contents immediately).
    /// Returns index on success, -1 on IO error (error stored in last_error_).
    int add_from_files(const std::filesystem::path& vs_path,
                       const std::filesystem::path& fs_path,
                       std::string display_name = {});

    /// Reload a shader entry from disk (if it has paths).
    bool reload(int idx);

    // Access
    int  count()    const noexcept { return static_cast<int>(entries_.size()); }
    bool empty()    const noexcept { return entries_.empty(); }

    const ShaderEntry& entry(int idx) const { return entries_.at(static_cast<std::size_t>(idx)); }
    ShaderEntry&       entry(int idx)       { return entries_.at(static_cast<std::size_t>(idx)); }

    // Selection

    int  selected()          const noexcept { return selected_; }
    bool has_selection()     const noexcept { return selected_ >= 0; }

    const ShaderEntry* selected_entry() const noexcept {
        return selected_ >= 0 ? &entries_[static_cast<std::size_t>(selected_)] : nullptr;
    }

    void select(int idx);

    // Change listener (signal pattern)
    using Listener = std::function<void(int idx, const ShaderEntry&)>;

    int  add_listener(Listener fn);
    void remove_listener(int handle);

    const std::string& last_error() const noexcept { return last_error_; }

    /// Update in-memory shader sources (called by text editor on save).
    void update_sources(int idx, std::string_view vs, std::string_view fs);

    /// Write a single shader stage back to its on-disk path.
    /// tab_idx 0 → vertex file, 1 → fragment file.
    /// Returns true on success; last_error() contains the reason on failure.
    bool save_to_disk(int idx, int tab_idx, std::string_view src) noexcept;

private:
    ShaderRegistry()  = default;
    ~ShaderRegistry() = default;

    ShaderRegistry(const ShaderRegistry&)            = delete;
    ShaderRegistry& operator=(const ShaderRegistry&) = delete;
    ShaderRegistry(ShaderRegistry&&)                 = delete;
    ShaderRegistry& operator=(ShaderRegistry&&)      = delete;

    std::vector<ShaderEntry> entries_;
    int selected_ = -1;
    std::string last_error_;

    struct ListenerEntry { int handle; Listener fn; };
    std::vector<ListenerEntry> listeners_;
    int next_handle_ = 0;

    void notify(int idx);
    // Returns file contents on success, or error message as unexpected
    [[nodiscard]] static std::expected<std::string, std::string>
        read_file(const std::filesystem::path& p) noexcept;
};

} // namespace shadebug::renderer
