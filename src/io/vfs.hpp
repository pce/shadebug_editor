#pragma once
// ── shadebug::vfs::VirtualFileSystem ────────────────────────────────────────────
//
//  Instantiable, thread-safe, header-only virtual filesystem.
//  No singleton. No global state. Create one instance per context.
//
//  Multiple directories per scheme (overlay / search-path):
//    vfs.mount("data", exe_dir / "data");
//    vfs.mount("data", project_dir / "overrides");  // checked first
//
//  Read-only mounts:
//    vfs.mount("data", bundled_assets, MountFlags::ReadOnly);
//    vfs.mount("data", user_overrides);              // writable, checked first
//    // writes always go to the first writable root;
//    // read-only roots participate in search but reject writes silently.
//
//  URI resolution rules
//  ─────────────────────
//    "scheme://rest"  →  search each root in insertion order (last first),
//                        return first existing hit.
//                        Writes: use the first writable root.
//    "/absolute"      →  pass-through
//    "relative"       →  search "data" roots, then current_path()
//
//  Thread safety
//  ─────────────
//  mount/unmount/init → exclusive lock
//  resolve/find/read/list/exists → shared lock (concurrent reads safe)
//  write/mkdir/remove → exclusive lock
//
//  Usage
//  ─────
//    shadebug::vfs::VirtualFileSystem vfs;
//    vfs.init(exe_dir);
//    vfs.mount("data",   bundled, vfs::MountFlags::ReadOnly);
//    vfs.mount("config", exe_dir);
//
//    auto path = vfs.find("data://fonts/foo.ttf");
//    auto text = vfs.read_text("config://settings.json");
//    vfs.write_text("config://settings.json", json_str);
//    auto kids = vfs.list("data://shaders");
// ─────────────────────────────────────────────────────────────────────────────

#include <filesystem>
#include <fstream>
#include <optional>
#include <iostream>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace shadebug::vfs {

enum class MountFlags : unsigned {
    // TODO Bitmask flags for future features like "overlay" (skip masked files), "cache" (writeback to a separate dir), etc.
    None     = 0,
    ReadOnly = 1 << 0,
    ReadWrite = 1 << 1,
    Executable = 1 << 2,
};

[[nodiscard]] constexpr MountFlags operator|(MountFlags a, MountFlags b) noexcept {
    return static_cast<MountFlags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
[[nodiscard]] constexpr bool operator&(MountFlags a, MountFlags b) noexcept {
    return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
}

class VirtualFileSystem {
public:
    VirtualFileSystem()  = default;
    ~VirtualFileSystem() = default;

    VirtualFileSystem(const VirtualFileSystem&)            = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;
    VirtualFileSystem(VirtualFileSystem&&)                 = delete;
    VirtualFileSystem& operator=(VirtualFileSystem&&)      = delete;

    // ── Mount management ─────────────────────────────────────────────────────

    /// Append a root to a scheme's search list. Last-mounted is searched first.
    /// MountFlags::ReadOnly prevents write_text/mkdir/remove from targeting it.
    void mount(std::string_view scheme, std::filesystem::path root,
               MountFlags flags = MountFlags::None) {
        std::unique_lock lock(mutex_);
        auto& vec = mounts_[std::string(scheme)];
        vec.insert(vec.begin(), Mount{ std::move(root), flags });
        const auto n = vec.size();
        std::cerr << "[vfs] " << scheme << "://  (" << n << " root" << (n == 1 ? "" : "s") << ")\n";
        for (auto i = 0u; i < n; ++i)
            std::cerr << "[vfs]   [" << i << "] " << vec[i].root.string() << "  "
                      << ((vec[i].flags & MountFlags::ReadOnly) ? "(ro)" : "(rw)") << '\n';
    }

    /// Remove all roots for a scheme.
    void unmount(std::string_view scheme) {
        std::unique_lock lock(mutex_);
        mounts_.erase(std::string(scheme));
    }

    /// Bootstrap standard shadebug layout:
    ///   data://   → <exe_dir>/data   (read-only by default — bundled assets)
    ///   config:// → <exe_dir>        (read-write — settings, cache)
    void init(const std::filesystem::path& exe_dir,
              MountFlags data_flags = MountFlags::ReadOnly) {
        mount("data",   exe_dir / "data", data_flags);
        // example: mount to ramdisk, localstorga  could be first to memory, second to disk, third to network
        mount("data",   exe_dir / "data_fallback"); // writable, checked first)
        mount("data",   std::filesystem::temp_directory_path() / "data_fallback"); // writable, checked first)

        mount("config", exe_dir);
    }

    // ── URI resolution ───────────────────────────────────────────────────────

    /// Resolve to a concrete path. Does not check existence.
    /// Returns the first existing hit across roots; if none exist, returns the
    /// first writable root (for write callers) or first root (absolute fallback).
    [[nodiscard]] std::filesystem::path resolve(std::string_view uri) const {
        std::shared_lock lock(mutex_);
        return resolve_(uri, /*for_write=*/false);
    }

    /// Resolve and verify existence. Returns nullopt (+ warning) if missing.
    [[nodiscard]] std::optional<std::filesystem::path> find(std::string_view uri) const {
        std::shared_lock lock(mutex_);
        auto p = resolve_(uri, /*for_write=*/false);
        if (!p.empty() && std::filesystem::exists(p)) return p;
        std::cerr << "[vfs] not found: " << uri << '\n';
        return std::nullopt;
    }

    // ── File I/O ─────────────────────────────────────────────────────────────

    /// Read entire file as text. Returns nullopt on any error.
    [[nodiscard]] std::optional<std::string> read_text(std::string_view uri) const {
        const auto path = resolve(uri);
        if (path.empty()) return std::nullopt;
        std::ifstream f(path, std::ios::ate | std::ios::binary);
        if (!f) return std::nullopt;
        const auto size = f.tellg();
        if (size < 0) return std::nullopt;
        f.seekg(0);
        std::string buf(static_cast<std::size_t>(size), '\0');
        f.read(buf.data(), size);
        return buf;
    }

    /// Write (create or replace) a file into the first writable root.
    /// Returns false and warns if all roots for the scheme are read-only.
    [[nodiscard]] bool write_text(std::string_view uri, std::string_view content) {
        std::unique_lock lock(mutex_);
        const auto path = resolve_(uri, /*for_write=*/true);
        lock.unlock();
        if (path.empty()) return false;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { std::cerr << "[vfs] mkdir " << path.string() << ": " << ec.message() << '\n'; return false; }
        std::ofstream f(path);
        if (!f) { std::cerr << "[vfs] write " << path.string() << ": cannot open\n"; return false; }
        f << content;
        return true;
    }

    /// List direct children (filenames only, no path prefix).
    [[nodiscard]] std::vector<std::string> list(std::string_view uri) const {
        const auto path = resolve(uri);
        if (path.empty() || !std::filesystem::is_directory(path)) return {};
        std::vector<std::string> out;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(path, ec))
            out.push_back(e.path().filename().string());
        return out;
    }

    [[nodiscard]] bool exists(std::string_view uri) const {
        return std::filesystem::exists(resolve(uri));
    }

    /// Returns false (+ warning) if all roots for the scheme are read-only.
    [[nodiscard]] bool mkdir(std::string_view uri) {
        std::unique_lock lock(mutex_);
        const auto path = resolve_(uri, /*for_write=*/true);
        lock.unlock();
        if (path.empty()) return false;
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) std::cerr << "[vfs] mkdir " << path.string() << ": " << ec.message() << '\n';
        return !ec;
    }

    /// Returns false (+ warning) if all roots for the scheme are read-only.
    [[nodiscard]] bool remove(std::string_view uri) {
        std::unique_lock lock(mutex_);
        const auto path = resolve_(uri, /*for_write=*/true);
        lock.unlock();
        if (path.empty()) return false;
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) std::cerr << "[vfs] remove " << path.string() << ": " << ec.message() << '\n';
        return !ec;
    }

    /// True if every root under `scheme` is read-only (or scheme is unknown).
    [[nodiscard]] bool is_readonly(std::string_view scheme) const {
        std::shared_lock lock(mutex_);
        const auto it = mounts_.find(std::string(scheme));
        if (it == mounts_.end()) return true;
        for (const auto& m : it->second)
            if (!(m.flags & MountFlags::ReadOnly)) return false;
        return true;
    }

private:
    struct Mount {
        std::filesystem::path root;
        MountFlags            flags = MountFlags::None;
    };

    // Called with mutex_ already held.
    // for_write=true  → skips read-only roots when choosing write target;
    //                   returns empty path if no writable root exists.
    // for_write=false → returns first existing hit, falling back to first root.
    [[nodiscard]] std::filesystem::path
    resolve_(std::string_view uri, bool for_write) const {
        if (uri.empty()) return {};

        // Absolute path — pass straight through
        // POSIX: starts with '/'
        // Windows: drive-letter like 'C:' (index 1 == ':'), or UNC paths starting with '\\'
        if (uri.starts_with('/') || uri.starts_with('\\') || (uri.size() >= 2 && uri[1] == ':'))
            return std::filesystem::path(uri);

        auto search = [&](const std::vector<Mount>& mounts,
                          std::string_view rest) -> std::filesystem::path {
            // Read path: first existing hit across all roots
            if (!for_write) {
                for (const auto& m : mounts) {
                    auto candidate = m.root / rest;
                    if (std::filesystem::exists(candidate)) return candidate;
                }
                // No hit — return first root as absolute fallback
                return mounts.empty() ? std::filesystem::path{} : mounts.front().root / rest;
            }
            // Write path: first writable root (regardless of existence)
            for (const auto& m : mounts)
                if (!(m.flags & MountFlags::ReadOnly)) return m.root / rest;
            std::cerr << "[vfs] write rejected — all roots are read-only for: " << uri << '\n';
            return {};
        };

        // scheme://rest
        if (const auto sep = uri.find("://"); sep != std::string_view::npos) {
            const std::string scheme(uri.substr(0, sep));
            const auto rest = uri.substr(sep + 3);
            if (const auto it = mounts_.find(scheme); it != mounts_.end())
                return search(it->second, rest);
            std::cerr << "[vfs] unknown scheme '" << scheme << "' in: " << uri << '\n';
            return {};
        }

        // Plain relative — search "data" roots, then cwd
        if (const auto it = mounts_.find("data"); it != mounts_.end()) {
            if (!for_write) {
                for (const auto& m : it->second) {
                    auto candidate = m.root / uri;
                    if (std::filesystem::exists(candidate)) return candidate;
                }
            }
        }
        return std::filesystem::current_path() / uri;
    }

    mutable std::shared_mutex                                    mutex_;
    std::unordered_map<std::string, std::vector<Mount>>          mounts_;
};

} // namespace shadebug::vfs
