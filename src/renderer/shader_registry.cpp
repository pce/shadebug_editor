#include "shader_registry.hpp"

#include <fstream>
#include <sstream>

namespace shadebug::renderer {

ShaderRegistry& ShaderRegistry::get() {
    static ShaderRegistry inst;
    return inst;
}

int ShaderRegistry::add(std::string name, std::string vs_src, std::string fs_src) {
    const int idx = static_cast<int>(entries_.size());
    entries_.push_back({ std::move(name), std::move(vs_src), std::move(fs_src) });
    if (selected_ < 0) select(idx);
    return idx;
}

int ShaderRegistry::add_from_files(const std::filesystem::path& vs_path,
                                    const std::filesystem::path& fs_path,
                                    std::string display_name) {
    auto vs = read_file(vs_path);
    if (!vs) { last_error_ = std::move(vs.error()); return -1; }

    auto fs = read_file(fs_path);
    if (!fs) { last_error_ = std::move(fs.error()); return -1; }

    last_error_.clear();

    const std::string name = display_name.empty()
        ? vs_path.stem().string()
        : std::move(display_name);

    const int idx = static_cast<int>(entries_.size());
    entries_.push_back({ name, std::move(*vs), std::move(*fs), vs_path, fs_path });
    if (selected_ < 0) select(idx);
    return idx;
}

bool ShaderRegistry::reload(int idx) {
    auto& e = entries_.at(static_cast<std::size_t>(idx));
    if (e.vs_path.empty() || e.fs_path.empty()) return false;

    auto vs = read_file(e.vs_path);
    if (!vs) { last_error_ = std::move(vs.error()); return false; }

    auto fs = read_file(e.fs_path);
    if (!fs) { last_error_ = std::move(fs.error()); return false; }

    last_error_.clear();
    e.vs_src = std::move(*vs);
    e.fs_src = std::move(*fs);
    if (idx == selected_) notify(idx);
    return true;
}

void ShaderRegistry::select(int idx) {
    if (idx == selected_) return;
    selected_ = idx;
    if (idx >= 0 && idx < count()) notify(idx);
}

void ShaderRegistry::update_sources(int idx, std::string_view vs, std::string_view fs) {
    auto& e = entries_.at(static_cast<std::size_t>(idx));
    e.vs_src = vs;
    e.fs_src = fs;
    if (idx == selected_) notify(idx);
}

int ShaderRegistry::add_listener(Listener fn) {
    const int h = next_handle_++;
    listeners_.push_back({ h, std::move(fn) });
    return h;
}

void ShaderRegistry::remove_listener(int handle) {
    std::erase_if(listeners_, [handle](const ListenerEntry& e){ return e.handle == handle; });
}

void ShaderRegistry::notify(int idx) {
    const auto& entry = entries_[static_cast<std::size_t>(idx)];
    for (auto& le : listeners_)
        le.fn(idx, entry);
}

bool ShaderRegistry::save_to_disk(int idx, int tab_idx, std::string_view src) noexcept {
    auto& e = entries_.at(static_cast<std::size_t>(idx));
    const auto& path = (tab_idx == 0) ? e.vs_path : e.fs_path;
    if (path.empty()) {
        last_error_ = "No file path for this shader stage";
        return false;
    }

    // Write atomically: write to .tmp, then rename
    const auto tmp = path.parent_path() / (path.filename().string() + ".tmp");
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) { last_error_ = "Cannot write: " + tmp.string(); return false; }
        f.write(src.data(), static_cast<std::streamsize>(src.size()));
        if (!f) { last_error_ = "Write error: " + tmp.string(); return false; }
    }

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        last_error_ = "Rename failed: " + ec.message();
        std::filesystem::remove(tmp, ec); // best-effort cleanup
        return false;
    }

    // Update in-memory source to match what was saved
    if (tab_idx == 0) e.vs_src = src;
    else              e.fs_src = src;

    last_error_.clear();
    return true;
}

std::expected<std::string, std::string>
ShaderRegistry::read_file(const std::filesystem::path& p) noexcept {
    std::ifstream f(p);
    if (!f) return std::unexpected("Cannot open: " + p.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace shadebug::renderer
