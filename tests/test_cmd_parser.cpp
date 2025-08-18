#include "cmd_parser.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include "./utils.test.cpp"

namespace fs = std::filesystem;

namespace {

    // ------- argv owner (null-terminated) -------
    struct Argv {
        std::vector<std::string> storage; // owns bytes
        std::vector<char*> ptrs;          // argv-style pointers (null-terminated)

        Argv(std::initializer_list<std::string> args) {
            storage.reserve(args.size() + 1);
            storage.emplace_back("./simulator"); // argv[0]
            for (const auto& s : args) storage.emplace_back(s);
            ptrs.reserve(storage.size() + 1);
            for (auto& s : storage) ptrs.push_back(s.data());
            ptrs.push_back(nullptr); // argv[argc]
        }

        int argc() const { return static_cast<int>(ptrs.size() - 1); }
        char** argv()    { return ptrs.data(); }
    };
}

// ---------------- Tests ----------------

TEST(CmdParserTest, ValidComparativeBasic) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt";
    const fs::path gmDir   = t.path() / "gm_folder";
    const fs::path a1Path  = t.path() / "algo1.so";
    const fs::path a2Path  = t.path() / "algo2.so";

    // Real artifacts
    touch(mapPath, "dummy");
    fs::create_directories(gmDir);
    touch(gmDir / "gm_impl.so", "");        // ensure folder contains at least one .so
    touch(a1Path, "");
    touch(a2Path, "");

    Argv a({
        "-comparative",
        std::string("game_map=") + mapPath.string(),
        std::string("game_managers_folder=") + gmDir.string(),
        std::string("algorithm1=") + a1Path.string(),
        std::string("algorithm2=") + a2Path.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    EXPECT_EQ(result.mode, CmdParser::Mode::Comparative);
    EXPECT_EQ(fs::path(result.gameMapFile).filename().string(), "map.txt");
    EXPECT_EQ(fs::path(result.algorithm1File).filename().string(), "algo1.so");
}

TEST(CmdParserTest, ValidCompetitionBasic) {
    TempDir t;
    const fs::path mapsDir = t.path() / "maps";
    const fs::path gmSo    = t.path() / "gm.so";
    const fs::path algos   = t.path() / "algos";

    fs::create_directories(mapsDir);
    fs::create_directories(algos);
    // minimal contents the parser likely validates:
    touch(mapsDir / "m1.map", "content");   // at least one map file
    touch(gmSo, "");
    touch(algos / "a1.so", "");
    touch(algos / "a2.so", "");             // at least two algos

    Argv a({
        "-competition",
        std::string("game_maps_folder=") + mapsDir.string(),
        std::string("game_manager=") + gmSo.string(),
        std::string("algorithms_folder=") + algos.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    EXPECT_EQ(result.mode, CmdParser::Mode::Competition);
    EXPECT_EQ(fs::path(result.algorithmsFolder).filename().string(), "algos");
}

TEST(CmdParserTest, MissingComparativeArgument) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt";
    const fs::path a1Path  = t.path() / "algo1.so";
    const fs::path a2Path  = t.path() / "algo2.so";
    touch(mapPath, "dummy");
    touch(a1Path, "");
    touch(a2Path, "");

    Argv a({
        "-comparative",
        std::string("game_map=") + mapPath.string(),
        std::string("algorithm1=") + a1Path.string(),
        std::string("algorithm2=") + a2Path.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.errorMessage.find("game_managers_folder"), std::string::npos);
}

TEST(CmdParserTest, AmbiguousMode) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt";
    const fs::path gmDir   = t.path() / "gm";
    const fs::path a1Path  = t.path() / "algo1.so";
    const fs::path a2Path  = t.path() / "algo2.so";
    touch(mapPath, "dummy");
    fs::create_directories(gmDir);
    touch(gmDir / "g1.so", "");
    touch(a1Path, "");
    touch(a2Path, "");

    Argv a({
        "-comparative",
        "-competition",
        std::string("game_map=") + mapPath.string(),
        std::string("game_managers_folder=") + gmDir.string(),
        std::string("algorithm1=") + a1Path.string(),
        std::string("algorithm2=") + a2Path.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.errorMessage.find("Exactly one of"), std::string::npos);
}

TEST(CmdParserTest, InvalidFormat) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt";
    const fs::path gmDir   = t.path() / "gm";
    const fs::path a1Path  = t.path() / "algo1.so";
    const fs::path a2Path  = t.path() / "algo2.so";
    touch(mapPath, "dummy");
    fs::create_directories(gmDir);
    touch(gmDir / "g1.so", "");
    touch(a1Path, "");
    touch(a2Path, "");

    Argv a({
        "-comparative",
        std::string("game_map = ") + mapPath.string(), // invalid format (space)
        std::string("game_managers_folder=") + gmDir.string(),
        std::string("algorithm1=") + a1Path.string(),
        std::string("algorithm2=") + a2Path.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
}

TEST(CmdParserTest, OptionalNumThreadsParsed) {
    TempDir t;
    const fs::path mapsDir = t.path() / "maps";
    const fs::path gmSo    = t.path() / "gm.so";
    const fs::path algos   = t.path() / "algos";
    fs::create_directories(mapsDir);
    touch(mapsDir / "m1.map", "content");   // ensure non-empty
    fs::create_directories(algos);
    touch(algos / "a1.so", "");
    touch(algos / "a2.so", "");
    touch(gmSo, "");

    Argv a({
        "-competition",
        std::string("game_maps_folder=") + mapsDir.string(),
        std::string("game_manager=") + gmSo.string(),
        std::string("algorithms_folder=") + algos.string(),
        "num_threads=8"
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    ASSERT_TRUE(result.numThreads.has_value());
    EXPECT_EQ(result.numThreads.value(), 8);
}

TEST(CmdParserTest, ParsesVerboseFlag) {
    TempDir t;
    const fs::path mapsDir = t.path() / "maps";
    const fs::path gmSo    = t.path() / "gm.so";
    const fs::path algos   = t.path() / "algos";
    fs::create_directories(mapsDir);
    touch(mapsDir / "m1.map", "content");
    fs::create_directories(algos);
    touch(algos / "a1.so", "");
    touch(algos / "a2.so", "");
    touch(gmSo, "");

    Argv a({
        "-competition",
        std::string("game_maps_folder=") + mapsDir.string(),
        std::string("game_manager=") + gmSo.string(),
        std::string("algorithms_folder=") + algos.string(),
        "-verbose"
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    EXPECT_TRUE(result.verbose);
}