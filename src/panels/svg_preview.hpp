#pragma once

#include "../document/document.hpp"
#include "imgui.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shadebug::panels {

enum class SvgPrimType : uint8_t { Circle, CircleFilled, Rect, Ellipse, Path, Text };

struct SvgPrim {
    SvgPrimType type = SvgPrimType::Circle;
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f, r = 0.f;
    shadebug::doc::Color fill{1.f,1.f,1.f,1.f};
    shadebug::doc::Color stroke{0.f,0.f,0.f,1.f};
    float stroke_width = 1.f;
    std::string_view text; // points into Element::content (zero-copy)
    shadebug::doc::Color text_color{0.f,0.f,0.f,1.f}; // text fill color (separate from primitive fill)
    float font_size = 12.f; // for text primitives
    std::vector<ImVec2> points; // for Path: polyline approximation
};

// Simple container for parsed SVG document data.
struct SvgData {
    std::vector<SvgPrim> prims;
    // optional viewBox: minX, minY, width, height
    bool has_viewbox = false;
    float vb_x = 0.f, vb_y = 0.f, vb_w = 0.f, vb_h = 0.f;
    // optional svg width/height (for preserveAspectRatio)
    bool has_svg_size = false;
    float svg_width = 0.f, svg_height = 0.f;
    // preserveAspectRatio: for now, just assume "xMidYMid meet" (center, fit inside)
};

// Parse small subset of SVG (circle, text) and optional svg viewBox.
// Output is an SvgData (primitives + viewBox).
void parse_svg(const shadebug::doc::Element& elem, SvgData& out);

// Render parsed SvgData to an ImDrawList. ep0 = element top-left in screen
// space. ew/eh are element screen size. If SvgData::has_viewbox, primitives
// are mapped from viewBox coords -> screen via scale/offset; otherwise they
// are interpreted in element local coords.
void render_svg(const SvgData& data, ImDrawList* dl,
                const ImVec2& ep0, float ew, float eh);

// Utility: compute simple hash of SVG content to detect changes.
std::size_t svg_content_hash(const std::string& s) noexcept;

} // namespace shadebug::panels

