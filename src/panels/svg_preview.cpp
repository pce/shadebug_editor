
#include "svg_preview.hpp"

#include <charconv>
#include <cstring>
#include <functional>

namespace shadebug::panels {

static bool parse_attr_float(std::string_view s, std::string_view name, float &out) {
	auto pos = s.find(name);
	if (pos == std::string_view::npos) return false;
	pos = s.find('=', pos);
	if (pos == std::string_view::npos) return false;
	pos = s.find_first_of("\"'", pos);
	if (pos == std::string_view::npos) return false;
	char quote = s[pos]; ++pos;
	auto end = s.find(quote, pos);
	if (end == std::string_view::npos) return false;
	std::string_view num = s.substr(pos, end - pos);
	auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), out);
	return ec == std::errc();
}

static bool parse_attr_str(std::string_view s, std::string_view name, std::string_view &out) {
	auto pos = s.find(name);
	if (pos == std::string_view::npos) return false;
	pos = s.find('=', pos);
	if (pos == std::string_view::npos) return false;
	pos = s.find_first_of("\"'", pos);
	if (pos == std::string_view::npos) return false;
	char quote = s[pos]; ++pos;
	auto end = s.find(quote, pos);
	if (end == std::string_view::npos) return false;
	out = s.substr(pos, end - pos);
	return true;
}

static shadebug::doc::Color parse_color(std::string_view v, const shadebug::doc::Color &fallback = {0,0,0,1}) {
	if (v.empty()) return fallback;
	if (v[0] == '#') {
		unsigned int val = 0;
		char buf[16];
		std::memset(buf, 0, sizeof(buf));
		auto n = v.size();
		if (n >= 7 && n <= 9) {
			std::memcpy(buf, v.data() + 1, n - 1);
			unsigned long x = std::strtoul(buf, nullptr, 16);
			if (n == 7) { // RRGGBB
				float r = ((x >> 16) & 0xFF) / 255.f;
				float g = ((x >> 8) & 0xFF) / 255.f;
				float b = (x & 0xFF) / 255.f;
				return {r,g,b,1.f};
			} else if (n == 9) { // RRGGBBAA
				float r = ((x >> 24) & 0xFF) / 255.f;
				float g = ((x >> 16) & 0xFF) / 255.f;
				float b = ((x >> 8) & 0xFF) / 255.f;
				float a = (x & 0xFF) / 255.f;
				return {r,g,b,a};
			}
		}
	}
	return fallback;
}

// Simple path parser: approximates SVG path commands as polyline
static void parse_svg_path(std::string_view pathd, std::vector<ImVec2>& points) {
	points.clear();
	float x = 0.f, y = 0.f;
	size_t i = 0;
	while (i < pathd.size()) {
		char cmd = pathd[i];
		if (cmd < 'A') {
			i++;
		} else {
			++i;
			if (cmd == 'M' || cmd == 'L') {
				float nx = 0, ny = 0;
				auto [px, ec] = std::from_chars(pathd.data() + i, pathd.data() + pathd.size(), nx);
				if (ec == std::errc()) {
					i = px - pathd.data();
					while (i < pathd.size() && (pathd[i] == ',' || pathd[i] == ' ')) i++;
					auto [py, ec2] = std::from_chars(pathd.data() + i, pathd.data() + pathd.size(), ny);
					if (ec2 == std::errc()) {
						i = py - pathd.data();
						if (cmd == 'M') points.clear();
						points.push_back({nx, ny});
						x = nx; y = ny;
					}
				}
			} else if (cmd == 'Z') {
				// close path
			}
		}
	}
}

void parse_svg(const shadebug::doc::Element& elem, SvgData& out) {
	out.prims.clear();
	out.has_viewbox = false;
	out.vb_x = out.vb_y = out.vb_w = out.vb_h = 0.f;
	out.has_svg_size = false;
	out.svg_width = out.svg_height = 0.f;
	std::string_view s(elem.content);

	// parse svg tag with viewBox, width, height
	auto svgpos = s.find("<svg");
	if (svgpos != std::string_view::npos) {
		auto svgclose = s.find('>', svgpos);
		if (svgclose != std::string_view::npos) {
			std::string_view svgtag = s.substr(svgpos, svgclose - svgpos + 1);
			std::string_view vb;
			if (parse_attr_str(svgtag, "viewBox", vb)) {
				float a=0,b=0,c=0,d=0;
				std::string tmp(vb);
				for (auto &ch : tmp) if (ch == ',') ch = ' ';
				if (sscanf(tmp.c_str(), "%f %f %f %f", &a, &b, &c, &d) == 4) {
					out.has_viewbox = true;
					out.vb_x = a; out.vb_y = b; out.vb_w = c; out.vb_h = d;
				}
			}
			if (parse_attr_float(svgtag, "width", out.svg_width)) out.has_svg_size = true;
			if (parse_attr_float(svgtag, "height", out.svg_height)) out.has_svg_size = true;
		}
	}

	size_t pos = 0;
	// circles
	while (true) {
		auto start = s.find("<circle", pos);
		if (start == std::string_view::npos) break;
		auto close = s.find('>', start);
		if (close == std::string_view::npos) break;
		std::string_view tag = s.substr(start, close - start + 1);
		SvgPrim p;
		float cx=0, cy=0, r=0;
		parse_attr_float(tag, "cx", cx);
		parse_attr_float(tag, "cy", cy);
		parse_attr_float(tag, "r", r);
		std::string_view fv, sv;
		if (parse_attr_str(tag, "fill", fv)) p.fill = parse_color(fv, p.fill);
		if (parse_attr_str(tag, "stroke", sv)) p.stroke = parse_color(sv, p.stroke);
		float sw = 1.f;
		parse_attr_float(tag, "stroke-width", sw);
		p.type = SvgPrimType::CircleFilled;
		p.x = cx; p.y = cy; p.r = r;
		p.stroke_width = sw;
		out.prims.push_back(p);
		pos = close + 1;
	}

	// rects
	pos = 0;
	while (true) {
		auto start = s.find("<rect", pos);
		if (start == std::string_view::npos) break;
		auto close = s.find('>', start);
		if (close == std::string_view::npos) break;
		std::string_view tag = s.substr(start, close - start + 1);
		SvgPrim p;
		float x=0, y=0, w=0, h=0;
		parse_attr_float(tag, "x", x);
		parse_attr_float(tag, "y", y);
		parse_attr_float(tag, "width", w);
		parse_attr_float(tag, "height", h);
		std::string_view fv, sv;
		if (parse_attr_str(tag, "fill", fv)) p.fill = parse_color(fv, p.fill);
		if (parse_attr_str(tag, "stroke", sv)) p.stroke = parse_color(sv, p.stroke);
		float sw = 1.f;
		parse_attr_float(tag, "stroke-width", sw);
		p.type = SvgPrimType::Rect;
		p.x = x; p.y = y; p.w = w; p.h = h;
		p.stroke_width = sw;
		out.prims.push_back(p);
		pos = close + 1;
	}

	// ellipses
	pos = 0;
	while (true) {
		auto start = s.find("<ellipse", pos);
		if (start == std::string_view::npos) break;
		auto close = s.find('>', start);
		if (close == std::string_view::npos) break;
		std::string_view tag = s.substr(start, close - start + 1);
		SvgPrim p;
		float cx=0, cy=0, rx=0, ry=0;
		parse_attr_float(tag, "cx", cx);
		parse_attr_float(tag, "cy", cy);
		parse_attr_float(tag, "rx", rx);
		parse_attr_float(tag, "ry", ry);
		std::string_view fv, sv;
		if (parse_attr_str(tag, "fill", fv)) p.fill = parse_color(fv, p.fill);
		if (parse_attr_str(tag, "stroke", sv)) p.stroke = parse_color(sv, p.stroke);
		float sw = 1.f;
		parse_attr_float(tag, "stroke-width", sw);
		p.type = SvgPrimType::Ellipse;
		p.x = cx; p.y = cy; p.w = rx; p.h = ry;
		p.stroke_width = sw;
		out.prims.push_back(p);
		pos = close + 1;
	}

	// paths
	pos = 0;
	while (true) {
		auto start = s.find("<path", pos);
		if (start == std::string_view::npos) break;
		auto close = s.find('>', start);
		if (close == std::string_view::npos) break;
		std::string_view tag = s.substr(start, close - start + 1);
		std::string_view pathd;
		if (parse_attr_str(tag, "d", pathd)) {
			SvgPrim p;
			parse_svg_path(pathd, p.points);
			std::string_view fv, sv;
			if (parse_attr_str(tag, "fill", fv)) p.fill = parse_color(fv, p.fill);
			if (parse_attr_str(tag, "stroke", sv)) p.stroke = parse_color(sv, p.stroke);
			float sw = 1.f;
			parse_attr_float(tag, "stroke-width", sw);
			p.type = SvgPrimType::Path;
			p.stroke_width = sw;
			out.prims.push_back(p);
		}
		pos = close + 1;
	}

	// text
	pos = 0;
	while (true) {
		auto tstart = s.find("<text", pos);
		if (tstart == std::string_view::npos) break;
		auto tclose = s.find('>', tstart);
		if (tclose == std::string_view::npos) break;
		auto tend = s.find("</text>", tclose);
		if (tend == std::string_view::npos) break;
		std::string_view tag = s.substr(tstart, tclose - tstart + 1);
		SvgPrim p;
		float tx=0, ty=0;
		parse_attr_float(tag, "x", tx);
		parse_attr_float(tag, "y", ty);
		std::string_view content = s.substr(tclose + 1, tend - (tclose + 1));
		p.type = SvgPrimType::Text;
		p.x = tx; p.y = ty;
		p.text = content;
		std::string_view fv;
		if (parse_attr_str(tag, "fill", fv)) p.text_color = parse_color(fv, p.text_color);
		parse_attr_float(tag, "font-size", p.font_size);
		out.prims.push_back(p);
		pos = tend + 7;
	}
}

void render_svg(const SvgData& data, ImDrawList* dl,
				const ImVec2& ep0, float ew, float eh) {
	if (!dl) return;

	float sx = 1.f, sy = 1.f, ox = 0.f, oy = 0.f;
	if (data.has_viewbox && data.vb_w > 0.f && data.vb_h > 0.f) {
		// xMidYMid meet: center and fit inside element bounds
		float aspect_svg = data.vb_w / data.vb_h;
		float aspect_elem = (eh > 0) ? (ew / eh) : 1.f;
		if (aspect_elem > aspect_svg) {
			sy = eh / data.vb_h;
			sx = sy;
		} else {
			sx = ew / data.vb_w;
			sy = sx;
		}
		ox = data.vb_x;
		oy = data.vb_y;
	}

	for (const auto& p : data.prims) {
		switch (p.type) {
			case SvgPrimType::CircleFilled: {
				ImVec2 center{ ep0.x + (p.x - ox) * sx, ep0.y + (p.y - oy) * sy };
				float r = p.r * ((sx + sy) * 0.5f);
				ImU32 c = IM_COL32(static_cast<int>(p.fill.r*255), static_cast<int>(p.fill.g*255), static_cast<int>(p.fill.b*255), static_cast<int>(p.fill.a*255));
				dl->AddCircleFilled(center, r, c);
				if (p.stroke_width > 0.f) {
					ImU32 sc = IM_COL32(static_cast<int>(p.stroke.r*255), static_cast<int>(p.stroke.g*255), static_cast<int>(p.stroke.b*255), static_cast<int>(p.stroke.a*255));
					dl->AddCircle(center, r, sc, 0, p.stroke_width);
				}
				break;
			}
			case SvgPrimType::Rect: {
				ImVec2 rp0{ ep0.x + (p.x - ox) * sx, ep0.y + (p.y - oy) * sy };
				ImVec2 rp1{ rp0.x + p.w * sx, rp0.y + p.h * sy };
				ImU32 c = IM_COL32(static_cast<int>(p.fill.r*255), static_cast<int>(p.fill.g*255), static_cast<int>(p.fill.b*255), static_cast<int>(p.fill.a*255));
				dl->AddRectFilled(rp0, rp1, c);
				if (p.stroke_width > 0.f) {
					ImU32 sc = IM_COL32(static_cast<int>(p.stroke.r*255), static_cast<int>(p.stroke.g*255), static_cast<int>(p.stroke.b*255), static_cast<int>(p.stroke.a*255));
					dl->AddRect(rp0, rp1, sc, 0.f, ImDrawFlags_None, p.stroke_width);
				}
				break;
			}
			case SvgPrimType::Ellipse: {
				ImVec2 center{ ep0.x + (p.x - ox) * sx, ep0.y + (p.y - oy) * sy };
				float r = ((p.w * sx) + (p.h * sy)) * 0.25f;
				ImU32 c = IM_COL32(static_cast<int>(p.fill.r*255), static_cast<int>(p.fill.g*255), static_cast<int>(p.fill.b*255), static_cast<int>(p.fill.a*255));
				dl->AddCircleFilled(center, r, c);
				if (p.stroke_width > 0.f) {
					ImU32 sc = IM_COL32(static_cast<int>(p.stroke.r*255), static_cast<int>(p.stroke.g*255), static_cast<int>(p.stroke.b*255), static_cast<int>(p.stroke.a*255));
					dl->AddCircle(center, r, sc, 0, p.stroke_width);
				}
				break;
			}
			case SvgPrimType::Path: {
				if (p.points.size() > 1) {
					std::vector<ImVec2> screen_points;
					for (const auto& pt : p.points) {
						screen_points.push_back({ ep0.x + (pt.x - ox) * sx, ep0.y + (pt.y - oy) * sy });
					}
					ImU32 c = IM_COL32(static_cast<int>(p.fill.r*255), static_cast<int>(p.fill.g*255), static_cast<int>(p.fill.b*255), static_cast<int>(p.fill.a*255));
					dl->AddPolyline(screen_points.data(), static_cast<int>(screen_points.size()), c, ImDrawFlags_None, p.stroke_width);
				}
				break;
			}
			case SvgPrimType::Text: {
				ImU32 tc = IM_COL32(
					static_cast<int>(p.text_color.r*255),
					static_cast<int>(p.text_color.g*255),
					static_cast<int>(p.text_color.b*255),
					static_cast<int>(p.text_color.a*255));
				std::string tmp(p.text);
				dl->AddText({ ep0.x + (p.x - ox) * sx, ep0.y + (p.y - oy) * sy }, tc, tmp.c_str());
				break;
			}
			default:
				break;
		}
	}
}

std::size_t svg_content_hash(const std::string& s) noexcept {
	return std::hash<std::string>{}(s);
}

} // namespace shadebug::panels


