// tests/comparative_simulator_unit_test.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <string>
#include <vector>
#include <unordered_set>
#include <optional>
#include <thread>

namespace fs = std::filesystem;


// --- test-only access to internals ---
#define private public
#define protected public
#undef private
#undef protected

class TempDir {
public:
    TempDir()
        : path_(fs::temp_directory_path() / "comp_sim_cmp_tests" / TempDir::make_unique_path()) {
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    const fs::path& path() const { return path_; }
private:
    fs::path path_;
    static std::string make_unique_path() {
        // Use time and random for uniqueness
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        auto randval = std::rand();
        std::ostringstream oss;
        oss << "tmp_" << now << "_" << randval;
        return oss.str();
    }
};

static void touch(const fs::path& p, std::string_view data = "") {
    std::ofstream ofs(p, std::ios::binary);
    ofs << data;
}

// Convenience board makers
static std::vector<std::vector<char>> rows(std::initializer_list<std::string> lines) {
    std::vector<std::vector<char>> b;
    b.reserve(lines.size());
    for (const auto& s : lines) b.push_back(std::vector<char>(s.begin(), s.end()));
    return b;
}
