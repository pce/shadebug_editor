#pragma once

#include <string>
#include <string_view>
#include <functional>

//  TextEditorPanel
//
//  Generic code/text editor pane.  Content is set externally (e.g. by
//  ShaderListPanel via signal) and can be read back for compilation/saving.
//
//  Tab support: multiple named tabs (e.g. "Vertex" / "Fragment").
//
//  Buffer: each tab holds a std::string pre-allocated to kBufSize so ImGui
//  can write directly into it via InputTextMultiline — no fixed-size array.
//

class TextEditorPanel {
public:
    static constexpr int kMaxTabs = 4;
    static constexpr int kBufSize = 64 * 1024;  // 64 KiB per tab

    struct Tab {
        std::string name;
        std::string buf;              // heap-allocated, pre-sized to kBufSize
        bool        modified      = false;
        std::string language_hint;   // "msl", "glsl", "hlsl", "json", …
    };

    using SaveCallback = std::function<void(int tab_idx, std::string_view content)>;

    TextEditorPanel() = default;

    void draw(bool& visible);


    /// Clear all tabs and reset title.
    void clear(std::string_view window_title = "Text Editor");

    /// Change only the window display title — does NOT reset tabs or active tab.
    /// Use this when switching shaders to avoid resetting ImGui tab-bar state.
    void set_title(std::string_view title);

    /// Add or replace a named tab with initial content. Returns tab index.
    int set_tab(std::string name, std::string_view content,
                std::string_view lang_hint = {});

    /// Read current (null-terminated) content of a tab as a view.
    /// Valid until the next modification of that tab's buffer.
    [[nodiscard]] std::string_view get_tab(int idx) const noexcept;

    [[nodiscard]] int  tab_count()  const noexcept { return tab_count_; }
    [[nodiscard]] bool is_visible() const noexcept { return visible_; }

    /// Called when the user presses Ctrl+S on a tab.
    void set_save_callback(SaveCallback cb)  { on_save_  = std::move(cb); }

    /// Called when the user presses Ctrl+Enter (e.g. "apply / recompile").
    void set_apply_callback(SaveCallback cb) { on_apply_ = std::move(cb); }

    /// Message shown when the panel has no tabs.
    void set_empty_hint(std::string hint)    { empty_hint_ = std::move(hint); }

    /// Clear the modified flag for a tab (called after a successful save).
    void mark_saved(int tab_idx) noexcept {
        if (tab_idx >= 0 && tab_idx < tab_count_)
            tabs_[tab_idx].modified = false;
    }

    bool& visible() noexcept { return visible_; }

private:
    std::string  title_     = "Text Editor";
    std::string  win_id_;          // cached: title_ + "##text_editor"
    std::string  empty_hint_;      // shown when tab_count_ == 0
    bool         visible_   = false;
    int          tab_count_ = 0;
    int          active_tab_ = 0;
    Tab          tabs_[kMaxTabs]{};
    SaveCallback on_save_;
    SaveCallback on_apply_;

    void handle_shortcuts(int tab_idx);
    void update_win_id();            // rebuild win_id_ after title change
};
