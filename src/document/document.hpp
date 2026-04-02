#pragma once

#include <string>
#include <vector>
#include <optional>
#include <expected>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace shadebug::doc {

// ── Primitive types ───────────────────────────────────────────────────────────

struct Vec2  { float x{}, y{}; };
struct Rect  { Vec2 pos; Vec2 size; };
struct Color { float r{1.f}, g{1.f}, b{1.f}, a{1.f}; };

enum class BlockKind : std::uint8_t {
    Shape = 0,
    Text,
    Image,
    SVG,    // renderer wired in later
    Table,
};

NLOHMANN_JSON_SERIALIZE_ENUM(BlockKind, {
    { BlockKind::Shape, "shape" },
    { BlockKind::Text,  "text"  },
    { BlockKind::Image, "image" },
    { BlockKind::SVG,   "svg"   },
    { BlockKind::Table, "table" },
})

// ── Style ─────────────────────────────────────────────────────────────────────

struct Style {
    Color       fill         = {1.f, 1.f, 1.f, 1.f};
    Color       stroke       = {0.f, 0.f, 0.f, 1.f};
    float       stroke_width = 1.f;
    float       opacity      = 1.f;
    float       corner_radius = 0.f;
    std::string font_family  = "sans-serif";
    float       font_size    = 14.f;
    bool        bold         = false;
    bool        italic       = false;
};

inline void to_json(nlohmann::json& j, const Color& c) {
    j = { c.r, c.g, c.b, c.a };
}
inline void from_json(const nlohmann::json& j, Color& c) {
    c = { j[0], j[1], j[2], j[3] };
}
inline void to_json(nlohmann::json& j, const Vec2& v) {
    j = { v.x, v.y };
}
inline void from_json(const nlohmann::json& j, Vec2& v) {
    v = { j[0], j[1] };
}
inline void to_json(nlohmann::json& j, const Rect& r) {
    j = { {"pos", r.pos}, {"size", r.size} };
}
inline void from_json(const nlohmann::json& j, Rect& r) {
    j.at("pos").get_to(r.pos);
    j.at("size").get_to(r.size);
}
inline void to_json(nlohmann::json& j, const Style& s) {
    j = {
        {"fill",          s.fill},
        {"stroke",        s.stroke},
        {"stroke_width",  s.stroke_width},
        {"opacity",       s.opacity},
        {"corner_radius", s.corner_radius},
        {"font_family",   s.font_family},
        {"font_size",     s.font_size},
        {"bold",          s.bold},
        {"italic",        s.italic},
    };
}
inline void from_json(const nlohmann::json& j, Style& s) {
    j.value("fill",          nlohmann::json{1,1,1,1}).get_to(s.fill);
    j.value("stroke",        nlohmann::json{0,0,0,1}).get_to(s.stroke);
    s.stroke_width  = j.value("stroke_width",  1.f);
    s.opacity       = j.value("opacity",       1.f);
    s.corner_radius = j.value("corner_radius", 0.f);
    s.font_family   = j.value("font_family",   "sans-serif");
    s.font_size     = j.value("font_size",     14.f);
    s.bold          = j.value("bold",          false);
    s.italic        = j.value("italic",        false);
}

// ── Element ───────────────────────────────────────────────────────────────────

struct Element {
    std::string    id;
    std::string    name;
    BlockKind      kind    = BlockKind::Shape;
    Rect           bounds  = {};
    Style          style   = {};
    std::string    content = {};   // text, image path, svg data, …
    bool           locked  = false;
    bool           visible = true;
    nlohmann::json extra   = {};   // kind-specific extension data
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Element,
    id, name, kind, bounds, style, content, locked, visible, extra)

// ── Layer ─────────────────────────────────────────────────────────────────────

struct Layer {
    std::string          id;
    std::string          name    = "Layer 1";
    bool                 visible = true;
    bool                 locked  = false;
    std::vector<Element> elements;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Layer,
    id, name, visible, locked, elements)

// ── Page ──────────────────────────────────────────────────────────────────────

struct Page {
    std::string         id;
    std::string         name      = "Page 1";
    float               width_mm  = 210.f;  // A4 default
    float               height_mm = 297.f;
    std::vector<Layer>  layers;

    [[nodiscard]] float aspect() const {
        return width_mm > 0.f ? height_mm / width_mm : 1.f;
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Page,
    id, name, width_mm, height_mm, layers)

// ── Document ──────────────────────────────────────────────────────────────────

struct Document {
    std::string       id;
    std::string       title  = "Untitled";
    std::string       author;
    std::vector<Page> pages;
    int               active_page_idx = 0;

    // ── Accessors ─────────────────────────────────────────────────────────────

    [[nodiscard]] Page* active_page() {
        if (pages.empty()) return nullptr;
        const auto i = std::clamp(active_page_idx, 0,
                                  static_cast<int>(pages.size()) - 1);
        return &pages[static_cast<std::size_t>(i)];
    }
    [[nodiscard]] const Page* active_page() const {
        return const_cast<Document*>(this)->active_page();
    }

    void add_page(std::string name = {}) {
        const auto idx = pages.size() + 1;
        if (name.empty()) name = "Page " + std::to_string(idx);
        pages.push_back(Page{
            .id   = "page-" + std::to_string(idx),
            .name = std::move(name),
        });
        // Ensure each new page has at least one layer
        pages.back().layers.push_back(Layer{
            .id   = "layer-1",
            .name = "Layer 1",
        });
    }

    // ── Factory ───────────────────────────────────────────────────────────────

    [[nodiscard]] static Document make_default() {
        Document d;
        d.id    = "doc-1";
        d.title = "Untitled";
        d.add_page("Page 1");
        return d;
    }

    // ── Serialisation ─────────────────────────────────────────────────────────

    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json{
            {"id",              id},
            {"title",           title},
            {"author",          author},
            {"pages",           pages},
            {"active_page_idx", active_page_idx},
        };
    }

    [[nodiscard]] static std::expected<Document, std::string>
    from_json(const nlohmann::json& j) noexcept {
        try {
            Document d;
            d.id              = j.value("id",    "doc-1");
            d.title           = j.value("title", "Untitled");
            d.author          = j.value("author", "");
            d.active_page_idx = j.value("active_page_idx", 0);
            d.pages           = j.value("pages", std::vector<Page>{});
            return d;
        } catch (const std::exception& e) {
            return std::unexpected(e.what());
        }
    }

    [[nodiscard]] static std::expected<Document, std::string>
    load(const std::filesystem::path& path) noexcept {
        try {
            std::ifstream f(path);
            if (!f) return std::unexpected("Cannot open: " + path.string());
            return from_json(nlohmann::json::parse(f));
        } catch (const std::exception& e) {
            return std::unexpected(e.what());
        }
    }

    [[nodiscard]] std::expected<void, std::string>
    save(const std::filesystem::path& path) const noexcept {
        try {
            std::ofstream f(path);
            if (!f) return std::unexpected("Cannot write: " + path.string());
            f << to_json().dump(2);
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(e.what());
        }
    }
};

} // namespace shadebug::doc
