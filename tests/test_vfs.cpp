#include <cstdint>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

// Tests for shadebug::vfs::VirtualFileSystem
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "../src/io/vfs.hpp"

using namespace shadebug::vfs;
namespace fs = std::filesystem;

static inline std::uint64_t get_process_id() noexcept {
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

struct TempDir {
    fs::path dir;
    TempDir() {
        static std::atomic<int> s_id{0};
        dir = fs::temp_directory_path() / ("dg_fs_" + std::to_string(get_process_id()) + "_" + std::to_string(++s_id));
        fs::create_directories(dir);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(dir, ec); }
    fs::path make(const std::string &sub) const { return dir / sub; }
    void write(const fs::path &p, const std::string &s) const { fs::create_directories(p.parent_path()); std::ofstream f(p, std::ios::binary); f << s; }
};

TEST_CASE("vfs basic operations: mount/find/read/write/list/remove", "[vfs]") {
    TempDir tmp;
    VirtualFileSystem vfs;

    // prepare two data roots
    auto rootA = tmp.make("rootA");
    auto rootB = tmp.make("rootB");
    fs::create_directories(rootA);
    fs::create_directories(rootB);

    // unique files
    std::ofstream(rootA / "only_a.txt") << "A";
    std::ofstream(rootB / "only_b.txt") << "B";

    // duplicate file present in both
    std::ofstream(rootA / "dup.txt") << "fromA";
    std::ofstream(rootB / "dup.txt") << "fromB";

    // mount rootA then rootB (rootB is searched first because mounts insert at front)
    vfs.mount("data", rootA);
    vfs.mount("data", rootB);

    SECTION("find unique files") {
        auto pa = vfs.find("data://only_a.txt");
        REQUIRE(pa.has_value());
        REQUIRE(pa->parent_path() == rootA);
        auto pb = vfs.find("data://only_b.txt");
        REQUIRE(pb.has_value());
        CHECK(pb->parent_path() == rootB);
    }

    SECTION("find prefers most-recently-mounted root when duplicate") {
        auto pdup = vfs.find("data://dup.txt");
        REQUIRE(pdup.has_value());
        // rootB was mounted last -> should be chosen
        CHECK(pdup->parent_path() == rootB);
        auto content = vfs.read_text("data://dup.txt");
        REQUIRE(content.has_value());
        CHECK(content.value() == "fromB");
    }

    SECTION("list returns filenames") {
        auto names = vfs.list("data://");
        // ensure at least one known file appears
        REQUIRE(!names.empty());
    }

    SECTION("write_text to config mount and remove") {
        auto cfg = tmp.make("cfg"); fs::create_directories(cfg);
        vfs.mount("config", cfg);
        REQUIRE(vfs.write_text("config://foobar.txt", "hello"));
        auto disk = cfg / "foobar.txt";
        REQUIRE(fs::exists(disk));
        REQUIRE(vfs.remove("config://foobar.txt"));
        REQUIRE(!fs::exists(disk));
    }

    SECTION("readonly mount rejects writes") {
        auto ro = tmp.make("ro"); fs::create_directories(ro);
        vfs.mount("ro", ro, MountFlags::ReadOnly);
        REQUIRE(vfs.is_readonly("ro") == true);
        REQUIRE(vfs.write_text("ro://a.txt", "x") == false);
    }

    SECTION("absolute path passthrough") {
        auto absf = tmp.make("abs.txt"); std::ofstream(absf) << "abs";
        auto p = vfs.find(absf.string());
        REQUIRE(p.has_value());
        auto c = vfs.read_text(absf.string());
        REQUIRE(c.has_value());
        CHECK(c.value() == "abs");
    }
}


