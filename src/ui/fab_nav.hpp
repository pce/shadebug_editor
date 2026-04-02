#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>

namespace shadebug::ui {

// ── FabNav ────────────────────────────────────────────────────────────────────
//
//  Dock-style floating action buttons.
//  - Top-level buttons stack vertically on the edge
//  - Clicking one opens its submenus HORIZONTALLY (away from edge)
//  - Only one group is open at a time
//  - Buttons with no children call callback immediately
//  - Labels float ABOVE submenu circles
//  - Drawn on the foreground draw list — no window, no input capture
//
// ─────────────────────────────────────────────────────────────────────────────

class FabNav {
public:
    enum class Position { TopLeft, TopRight, BottomLeft, BottomRight };

    struct Item {
        std::string           id;
        std::string           parent_id;
        std::string           icon;
        std::string           label;
        std::function<void()> callback;
    };

    FabNav() = default;
    explicit FabNav(std::string name, ImVec2 = {}, Position p = Position::TopRight)
        : name_(std::move(name)), position_(p) {}

    void set_position(Position p) { position_ = p; }
    void set_radius(float r)      { radius_   = r; }
    void set_spacing(float s)     { spacing_  = s; }

    float anim_speed_ = 0.14f;

    void add_button(const std::string& id, const std::string& icon,
                    const std::string& /*unused*/ = {},
                    std::function<void()> cb = nullptr) {
        items_.push_back({ id, {}, icon, {}, std::move(cb) });
    }

    void add_submenu(const std::string& parent_id, const std::string& id,
                     const std::string& icon, const std::string& label = {},
                     std::function<void()> cb = nullptr) {
        items_.push_back({ id, parent_id, icon, label, std::move(cb) });
    }

    void draw() {
        if (items_.empty()) return;

        // Smooth animation towards open (1) or closed (0)
        const float target = active_parent_.empty() ? 0.f : 1.f;
        const float dt     = ImGui::GetIO().DeltaTime;
        anim_t_ += (target - anim_t_) * std::min(1.f, anim_speed_ * dt * 400.f);
        anim_t_  = std::clamp(anim_t_, 0.f, 1.f);

        ImDrawList*  dl   = ImGui::GetForegroundDrawList();
        const ImVec2 vp   = ImGui::GetMainViewport()->WorkPos;
        const ImVec2 vs   = ImGui::GetMainViewport()->WorkSize;
        const float  m    = 16.f;
        const float  vstep = radius_ * 2.f + spacing_;
        const float  hstep = radius_ * 2.f + spacing_;

        // Dock anchor (centre of first button)
        ImVec2 anchor;
        switch (position_) {
        case Position::TopLeft:    anchor = { vp.x + m + radius_,         vp.y + m + radius_ }; break;
        case Position::TopRight:   anchor = { vp.x+vs.x - m - radius_,    vp.y + m + radius_ }; break;
        case Position::BottomLeft: anchor = { vp.x + m + radius_,         vp.y+vs.y - m - radius_ }; break;
        case Position::BottomRight:anchor = { vp.x+vs.x - m - radius_,    vp.y+vs.y - m - radius_ }; break;
        }

        // Submenus expand away from the viewport edge
        const bool  right_side = (position_ == Position::TopRight  || position_ == Position::BottomRight);
        const float sub_dir    = right_side ? -1.f : 1.f;

        const ImVec2 mouse   = ImGui::GetIO().MousePos;
        const bool   clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool         hit     = false;

        // ── Top-level dock buttons ────────────────────────────────────────────
        int top_idx = 0;
        for (auto& item : items_) {
            if (!item.parent_id.empty()) continue;

            const ImVec2 c      = { anchor.x, anchor.y + top_idx * vstep };
            const bool   active = (active_parent_ == item.id);
            const bool   hov    = (std::hypot(mouse.x - c.x, mouse.y - c.y) <= radius_);

            draw_btn(dl, c, radius_, item.icon.c_str(), hov || active, true);

            if (hov && clicked) {
                hit = true;
                bool has_subs = false;
                for (const auto& s : items_)
                    if (s.parent_id == item.id) { has_subs = true; break; }

                if (has_subs) {
                    active_parent_ = active ? "" : item.id;  // toggle this group
                    if (item.callback && !active) item.callback();
                } else {
                    active_parent_ = "";
                    if (item.callback) item.callback();
                }
            }

            // ── Submenus for this button ─────────────────────────────────────
            if (active && anim_t_ > 0.01f) {
                int sub_idx = 0;
                for (auto& sub : items_) {
                    if (sub.parent_id != item.id) continue;
                    ++sub_idx;

                    const float  a     = anim_t_;
                    const float  sub_r = radius_ * 0.82f;
                    const ImVec2 sc    = { c.x + sub_dir * hstep * sub_idx * a, c.y };

                    const bool shov = (std::hypot(mouse.x - sc.x, mouse.y - sc.y) <= sub_r);
                    draw_btn(dl, sc, sub_r, sub.icon.c_str(), shov, false, a);

                    // Label above the circle
                    if (!sub.label.empty()) {
                        const ImVec4 tc   = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                        const ImU32  col  = ImGui::ColorConvertFloat4ToU32({tc.x, tc.y, tc.z, tc.w * a});
                        const ImVec2 tsz  = ImGui::CalcTextSize(sub.label.c_str());
                        const float  bg_pad = 3.f;
                        const ImVec2 tp   = { sc.x - tsz.x * 0.5f,
                                              sc.y - sub_r - tsz.y - 6.f };
                        // Small dark pill background for readability
                        dl->AddRectFilled(
                            { tp.x - bg_pad, tp.y - bg_pad },
                            { tp.x + tsz.x + bg_pad, tp.y + tsz.y + bg_pad },
                            ImGui::ColorConvertFloat4ToU32({0,0,0, 0.55f * a}), 4.f);
                        dl->AddText(tp, col, sub.label.c_str());
                    }

                    if (shov && clicked) {
                        hit            = true;
                        active_parent_ = "";
                        anim_t_        = 0.f;
                        if (sub.callback) sub.callback();
                    }
                }
            }

            ++top_idx;
        }

        // Click outside any button → close
        if (clicked && !hit)
            active_parent_ = "";
    }

private:
    std::string       name_;
    Position          position_      = Position::TopRight;
    float             radius_        = 22.f;
    float             spacing_       = 8.f;
    std::string       active_parent_;
    float             anim_t_        = 0.f;
    std::vector<Item> items_;

    void draw_btn(ImDrawList* dl, ImVec2 c, float r,
                  const char* icon, bool hov, bool main_btn,
                  float alpha = 1.f) const
    {
        // Drop shadow
        dl->AddCircleFilled({ c.x + 2.f, c.y + 3.f }, r,
            ImGui::ColorConvertFloat4ToU32({ 0.f, 0.f, 0.f, 0.4f * alpha }));

        // Fill
        const ImVec4 fill = hov
            ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)
            : ImGui::GetStyleColorVec4(ImGuiCol_Button);
        dl->AddCircleFilled(c, r,
            ImGui::ColorConvertFloat4ToU32({ fill.x, fill.y, fill.z, fill.w * alpha }));

        // Border
        const ImVec4 border = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        dl->AddCircle(c, r,
            ImGui::ColorConvertFloat4ToU32({ border.x, border.y, border.z, 0.7f * alpha }),
            0, main_btn ? 1.5f : 1.f);

        // Icon — centred using CalcTextSize so FA glyphs are placed correctly
        if (icon && icon[0]) {
            const ImVec2 tsz = ImGui::CalcTextSize(icon);
            const ImVec4 tc  = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            dl->AddText({ c.x - tsz.x * 0.5f, c.y - tsz.y * 0.5f },
                ImGui::ColorConvertFloat4ToU32({ tc.x, tc.y, tc.z, tc.w * alpha }),
                icon);
        }
    }
};

} // namespace shadebug::ui

