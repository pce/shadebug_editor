#include "canvas_panel.hpp"
#include "../app.hpp"
#include "../utils.hpp"
#include "../renderer/draw_ctx.hpp"
#include "imgui.h"
#include "svg_preview.hpp"
#include <algorithm>
#include <cmath>

namespace shadebug::panels {

void draw_canvas_panel(shadebug::App& app) {
    if (!app.panels.show_canvas) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::Begin("Canvas");
    ImGui::PopStyleVar();
    if (!open) { ImGui::End(); return; }

    const ImVec2 avail  = ImGui::GetContentRegionAvail();
    const auto*  page   = app.document.active_page();

    if (!page || avail.x < 4.f || avail.y < 4.f) {
        utils::centered_text("No page");
        ImGui::End();
        return;
    }

    //  Canvas geometry
    const ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImDrawList*  dl        = ImGui::GetWindowDrawList();

    //  Fit page
    constexpr float kMargin = 20.f;
    const auto fit = utils::fit_rect(page->width_mm, page->height_mm,
                                     avail.x, avail.y, kMargin);

    const ImVec2 p0 = { canvas_p0.x + fit.offset.x,
                        canvas_p0.y + fit.offset.y };
    const ImVec2 p1 = { p0.x + fit.size.x, p0.y + fit.size.y };

    // GPU rects: canvas bg + page shadow + page surface
    // Pushed into the shared draw_ctx — rendered behind ImGui by frame_cb.
    auto& ctx = app.draw_ctx;

    // Canvas dark background
    ctx.push_rect(canvas_p0.x, canvas_p0.y, avail.x, avail.y,
                  0.149f, 0.149f, 0.149f, 1.f);

    // Drop shadow (offset +6,+6, soft black, rounded)
    ctx.push_rect(p0.x + 6.f, p0.y + 6.f, fit.size.x, fit.size.y,
                  0.f, 0.f, 0.f, 0.35f, /*corner_radius=*/4.f);

    // Page white surface with subtle border
    ctx.push_rect(p0.x, p0.y, fit.size.x, fit.size.y,
                  1.f, 1.f, 1.f, 1.f,
                  /*corner_radius=*/2.f, /*border_width=*/1.f,
                  0.588f, 0.588f, 0.588f, 0.784f);  // border: #969696CC

    for (const auto& layer : page->layers) {
        if (!layer.visible) continue;
        for (const auto& elem : layer.elements) {
            if (!elem.visible) continue;

            const float ex = p0.x + elem.bounds.pos.x  * fit.scale;
            const float ey = p0.y + elem.bounds.pos.y  * fit.scale;
            const float ew = elem.bounds.size.x * fit.scale;
            const float eh = elem.bounds.size.y * fit.scale;
            const ImVec2 ep0{ ex,      ey      };
            const ImVec2 ep1{ ex + ew, ey + eh };

            const auto& s = elem.style;

            switch (elem.kind) {
            case doc::BlockKind::Shape:
                // GPU-rendered: instanced SDF rect — handles fill, border, radius
                ctx.push_rect(ex, ey, ew, eh,
                              s.fill.r, s.fill.g, s.fill.b, s.fill.a * s.opacity,
                              s.corner_radius * fit.scale,
                              s.stroke_width,
                              s.stroke.r, s.stroke.g, s.stroke.b, s.stroke.a);
                break;
            case doc::BlockKind::Text: {
                // Background via GPU, text via ImDrawList
                ctx.push_rect(ex, ey, ew, eh,
                              s.fill.r, s.fill.g, s.fill.b, s.fill.a * s.opacity,
                              s.corner_radius * fit.scale);
                if (!elem.content.empty()) {
                    const ImU32 tc = IM_COL32(
                        static_cast<int>(s.stroke.r * 255),
                        static_cast<int>(s.stroke.g * 255),
                        static_cast<int>(s.stroke.b * 255),
                        static_cast<int>(s.stroke.a * 255));
                    dl->AddText(ep0, tc, elem.content.c_str());
                }
                break;
            }
            case doc::BlockKind::SVG:
                // SVG: parse-on-change + cached primitives, render via ImDrawList
                // Keep the same background rect as placeholder for the page surface
                ctx.push_rect(ex, ey, ew, eh,
                              0.902f, 0.941f, 1.f, 0.47f,   // light blue fill
                              0.f, 1.5f,
                              0.392f, 0.549f, 0.863f, 0.71f);

                // Compute hash and parse lazily into App cache
                if (!elem.id.empty()) {
                    const std::size_t h = shadebug::panels::svg_content_hash(elem.content);
                    const auto it = app.svg_cache_hash.find(elem.id);
                    if (it == app.svg_cache_hash.end() || it->second != h) {
                        // (re)parse into cache
                        auto &d = app.svg_cache[elem.id];
                        shadebug::panels::parse_svg(elem, d);
                        app.svg_cache_hash[elem.id] = h;
                    }
                    const auto &data = app.svg_cache[elem.id];
                    // Render mapped into element rect (ew,eh). render_svg will map
                    // viewBox coords -> screen automatically if present.
                    shadebug::panels::render_svg(data, dl, ep0, ew, eh);
                } else {
                    // Fallback label if no id
                    dl->AddText({ ep0.x + 4.f, ep0.y + 4.f }, IM_COL32(80, 80, 160, 255), "SVG");
                }
                break;
            default:
                ctx.push_rect(ex, ey, ew, eh,
                              s.fill.r, s.fill.g, s.fill.b, s.fill.a * s.opacity);
                break;
            }

            // Selection highlight (ImDrawList overlay — needs to appear on top)
            if (app.selection.element_id == elem.id)
                dl->AddRect(ep0, ep1, IM_COL32(80, 160, 255, 255),
                            0.f, ImDrawFlags_None, 2.f);
        }
    }

    //  SVG placeholder when page is empty
    if (page->layers.empty() ||
        std::ranges::all_of(page->layers,
            [](const auto& l){ return l.elements.empty(); }))
    {
        const ImVec2 mid  = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
        const float  r    = fit.size.x * 0.06f;
        dl->AddCircleFilled(mid, r,     IM_COL32(200, 200, 200, 60));
        dl->AddCircle      (mid, r,     IM_COL32(180, 180, 180, 140), 0, 1.5f);
        const char* msg = "SVG renderer — coming soon";
        const float tw  = ImGui::CalcTextSize(msg).x;
        dl->AddText({ mid.x - tw * 0.5f, mid.y + r + 6.f },
                    IM_COL32(160, 160, 160, 200), msg);
    }

    // Page size label
    {
        char label[64];
        std::snprintf(label, sizeof(label), "%.0f × %.0f mm  (%.0f%%)",
                      page->width_mm, page->height_mm, fit.scale * 100.f);
        const float lw = ImGui::CalcTextSize(label).x + 12.f;
        dl->AddRectFilled({ p0.x, p1.y + 4.f },
                          { p0.x + lw, p1.y + 20.f },
                          IM_COL32(30, 30, 30, 160), 3.f);
        dl->AddText({ p0.x + 6.f, p1.y + 6.f },
                    IM_COL32(190, 190, 190, 255), label);
    }

    // Invisible dummy to fill the canvas area
    ImGui::Dummy(avail);
    ImGui::End();
}

} // namespace shadebug::panels
