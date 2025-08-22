#include "cmd_parser.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include "./utils/utils.test.cpp"

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

static bool containsAll(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (auto n : needles) if (haystack.find(n) == std::string::npos) return false;
    return true;
}

} // namespace

// ---------------- Happy-path Tests ----------------

TEST(CmdParserTest, ValidComparativeBasic) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt";
    const fs::path gmDir   = t.path() / "gm_folder";
    const fs::path a1Path  = t.path() / "algo1.so";
    const fs::path a2Path  = t.path() / "algo2.so";

    touch(mapPath, "dummy");
    fs::create_directories(gmDir);
    touch(gmDir / "gm_impl.so", "");
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
    touch(mapsDir / "m1.map", "content");
    touch(gmSo, "");
    touch(algos / "a1.so", "");
    touch(algos / "a2.so", "");

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

TEST(CmdParserTest, ParsesVerboseFlagAnywhere) {
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
        "-verbose",
        std::string("game_maps_folder=") + mapsDir.string(),
        "-verbose",
        std::string("game_manager=") + gmSo.string(),
        std::string("algorithms_folder=") + algos.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    EXPECT_TRUE(result.verbose);
}

TEST(CmdParserTest, OptionalNumThreadsParsed) {
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
        "num_threads=8"
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    ASSERT_TRUE(result.numThreads.has_value());
    EXPECT_EQ(result.numThreads.value(), 8);
}

// ---------------- Whitespace & Multi-token Variants ----------------

TEST(CmdParserTest, OrderDoesNotMatter) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt";
    const fs::path gmDir   = t.path() / "gm";
    const fs::path a1Path  = t.path() / "a1.so";
    const fs::path a2Path  = t.path() / "a2.so";
    touch(mapPath, "x");
    fs::create_directories(gmDir);
    touch(gmDir / "g1.so", "");
    touch(a1Path, "");
    touch(a2Path, "");

    Argv a({
        std::string("algorithm2=") + a2Path.string(),
        "-comparative",
        std::string("algorithm1=") + a1Path.string(),
        std::string("game_managers_folder=") + gmDir.string(),
        std::string("game_map=") + mapPath.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
}

// ---------------- Error Aggregation ----------------

TEST(CmdParserTest, AggregatesMissingArgsAndUnsupportedTokens) {
    Argv a({
        "-comparative",
        "-weird",             // unsupported switch
        "foo",                // stray token
        "=bar",               // value w/o key
        "algorithm1=path1"    // still missing gm folder, map, algorithm2
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
    ASSERT_FALSE(result.errorMessage.empty());
    EXPECT_TRUE(containsAll(result.errorMessage, {
        "Missing required argument: game_map",
        "Missing required argument: game_managers_folder",
        "Missing required argument: algorithm2",
        "Unsupported argument: -weird",
        "Unsupported argument: foo",
        "Unsupported argument: =bar"
    }));
}

TEST(CmdParserTest, AmbiguousModeBothSpecified) {
    Argv a({"-comparative", "-competition", "foo=bar"});
    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.errorMessage.find("Exactly one of -comparative or -competition"), std::string::npos);
}

TEST(CmdParserTest, DuplicateKeysReported) {
    Argv a({
        "-competition",
        "game_manager=gm1.so",
        "game_manager=gm2.so",   // duplicate
        "algorithms_folder=algos",
        "algorithms_folder=algos2", // duplicate
        "game_maps_folder=maps"
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(containsAll(result.errorMessage, {
        "Duplicate argument: game_manager",
        "Duplicate argument: algorithms_folder"
    }));
}

// ---------------- num_threads Validation ----------------

TEST(CmdParserTest, NumThreadsDefaultIsOne) {
    TempDir t;
    const fs::path mapsDir = t.path() / "maps";
    const fs::path gmSo    = t.path() / "gm.so";
    const fs::path algos   = t.path() / "algos";
    fs::create_directories(mapsDir);
    touch(mapsDir / "m1.map", "x");
    fs::create_directories(algos);
    touch(algos / "a1.so", "");
    touch(algos / "a2.so", "");
    touch(gmSo, "");

    Argv a({
        "-competition",
        std::string("game_maps_folder=") + mapsDir.string(),
        std::string("game_manager=") + gmSo.string(),
        std::string("algorithms_folder=") + algos.string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_TRUE(result.valid) << result.errorMessage;
    ASSERT_TRUE(result.numThreads.has_value());
    EXPECT_EQ(result.numThreads.value(), 1);
}

TEST(CmdParserTest, NumThreadsRejectsZeroNegativeAndJunk) {
    // zero
    Argv a0({"-comparative", "algorithm1=a.so", "algorithm2=b.so", "game_map=m.map", "game_managers_folder=gm", "num_threads=0"});
    auto r0 = CmdParser::parse(a0.argc(), a0.argv());
    EXPECT_FALSE(r0.valid);
    EXPECT_NE(r0.errorMessage.find("Invalid value for num_threads"), std::string::npos);

    // negative-like (note: '-' is unsupported token; still results in invalid num_threads)
    Argv an({"-comparative", "algorithm1=a.so", "algorithm2=b.so", "game_map=m.map", "game_managers_folder=gm", "num_threads=-3"});
    auto rn = CmdParser::parse(an.argc(), an.argv());
    EXPECT_FALSE(rn.valid);

    // junk
    Argv aj({"-comparative", "algorithm1=a.so", "algorithm2=b.so", "game_map=m.map", "game_managers_folder=gm", "num_threads=3x"});
    auto rj = CmdParser::parse(aj.argc(), aj.argv());
    EXPECT_FALSE(rj.valid);
}

// ---------------- Filesystem Validation ----------------

TEST(CmdParserTest, FailsOnMissingOrInvalidPaths) {
    TempDir t;
    const fs::path mapPath = t.path() / "map.txt"; // will not create => missing
    const fs::path gmDir   = t.path() / "gm";      // empty dir => invalid
    const fs::path a1Path  = t.path() / "a1.so";   // create only a1
    fs::create_directories(gmDir);                  // empty => invalid by contract
    touch(a1Path, "");

    Argv a({
        "-comparative",
        std::string("game_map=") + mapPath.string(),
        std::string("game_managers_folder=") + gmDir.string(),
        std::string("algorithm1=") + a1Path.string(),
        std::string("algorithm2=") + (t.path() / "nope.so").string()
    });

    auto result = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(result.valid);
    // We don't assert exact text; just ensure it's failing for path reasons
}

// ---------------- Edge Cases ----------------

TEST(CmdParserTest, LoneEqualsAndDanglingKeyAreUnsupported) {
    Argv a({"-competition", "=value", "key="});
    auto r = CmdParser::parse(a.argc(), a.argv());
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(containsAll(r.errorMessage, {"Unsupported argument: =value", "Unsupported argument: key="}));
}
