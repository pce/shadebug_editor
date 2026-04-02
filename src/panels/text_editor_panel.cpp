#include "text_editor_panel.hpp"
#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <format>


namespace {

void fill_tab_buf(TextEditorPanel::Tab& tab, std::string_view content) {
    tab.buf.assign(content.data(),
                   std::min(content.size(),
                            static_cast<std::size_t>(TextEditorPanel::kBufSize - 1)));
    // Pad to kBufSize so ImGui can write into the full buffer
    tab.buf.resize(TextEditorPanel::kBufSize, '\0');
    tab.modified = false;
}

} // namespace


void TextEditorPanel::clear(std::string_view window_title) {
    title_      = window_title;
    update_win_id();
    tab_count_  = 0;
    // Do NOT reset active_tab_ here: ImGui controls which tab is visually
    // selected (by its own internal state keyed on the stable ###-id window).
    // Resetting to 0 would make Ctrl+S / Ctrl+Enter target the wrong tab
    // until ImGui fires the next BeginTabItem() callback.
    for (auto& t : tabs_) { t = Tab{}; }
}

void TextEditorPanel::update_win_id() {
    win_id_ = title_;
    // ### (triple-hash) resets ImGui's hash to the seed at this point.
    // "vignette###shadebug_te" and "planet###shadebug_te" both hash to the
    // same ImGui window ID, so tab-bar state (active tab, scroll) is preserved
    // when the shader title changes.  ## (double-hash) would hash the full
    // string, creating a brand-new window on every shader switch → tab always
    // resets to index 0 (Vertex).
    win_id_ += "###shadebug_text_editor";
}

int TextEditorPanel::set_tab(std::string name, std::string_view content,
                              std::string_view lang_hint) {
    // Replace existing tab with same name
    for (int i = 0; i < tab_count_; ++i) {
        if (tabs_[i].name == name) {
            fill_tab_buf(tabs_[i], content);
            tabs_[i].language_hint = lang_hint;
            return i;
        }
    }
    if (tab_count_ >= kMaxTabs) return -1;
    const int idx   = tab_count_++;
    tabs_[idx].name = std::move(name);
    fill_tab_buf(tabs_[idx], content);
    tabs_[idx].language_hint = lang_hint;
    return idx;
}

std::string_view TextEditorPanel::get_tab(int idx) const noexcept {
    if (idx < 0 || idx >= tab_count_) return {};
    // buf is pre-sized to kBufSize; content ends at first '\0'
    return std::string_view(tabs_[idx].buf.data());
}


void TextEditorPanel::draw(bool& visible) {
    if (!visible) return;

    // Lazily init win_id_ (e.g. default-constructed without calling clear())
    if (win_id_.empty()) update_win_id();

    ImGui::SetNextWindowSize(ImVec2(740, 540), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(win_id_.c_str(), &visible, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            if (on_save_ && tab_count_ > 0)
                on_save_(active_tab_, get_tab(active_tab_));
        }
        if (ImGui::MenuItem("Apply", "Ctrl+Enter")) {
            if (on_apply_ && tab_count_ > 0)
                on_apply_(active_tab_, get_tab(active_tab_));
        }
        ImGui::EndMenuBar();
    }

    if (tab_count_ == 0) {
        const char* hint = empty_hint_.empty() ? "(no content)" : empty_hint_.c_str();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 ts    = ImGui::CalcTextSize(hint);
        ImGui::SetCursorPos({ (avail.x - ts.x) * 0.5f,
                              (avail.y - ts.y) * 0.5f + ImGui::GetCursorPosY() });
        ImGui::TextDisabled("%s", hint);
        ImGui::End();
        return;
    }


    if (ImGui::BeginTabBar("##editor_tabs")) {
        for (int i = 0; i < tab_count_; ++i) {
            auto& tab = tabs_[i];

            // Stack-allocated label — no heap allocation per frame
            char label[128];
            std::format_to_n(label, sizeof(label) - 1,
                             "{}{} ##etab{}", tab.name,
                             tab.modified ? "*" : "", i).out[0] = '\0';

            if (ImGui::BeginTabItem(label)) {
                active_tab_ = i;
                handle_shortcuts(i);

                if (!tab.language_hint.empty()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[%s]", tab.language_hint.c_str());
                }

                const float avail_h = ImGui::GetContentRegionAvail().y;

                char ed_id[32];
                std::format_to_n(ed_id, sizeof(ed_id) - 1, "##ed_{}", i).out[0] = '\0';

                if (ImGui::InputTextMultiline(
                        ed_id,
                        tab.buf.data(), kBufSize,
                        ImVec2(-FLT_MIN, avail_h),
                        ImGuiInputTextFlags_AllowTabInput)) {
                    tab.modified = true;
                }

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}


void TextEditorPanel::handle_shortcuts(int tab_idx) {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S) && on_save_)
        on_save_(tab_idx, get_tab(tab_idx));

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Enter) && on_apply_)
        on_apply_(tab_idx, get_tab(tab_idx));
}
