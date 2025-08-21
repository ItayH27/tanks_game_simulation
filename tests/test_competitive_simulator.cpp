// competitive_simulator_test.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>
#include <optional>
#include "./utils.test.cpp" 

namespace fs = std::filesystem;

// --- Make internals visible to tests (test-only hack) ---
#define private public
#define protected public
#include "../sim_include/competitive_simulator.h"
#undef private
#undef protected

// Minimal stubs for anything the header might need (if any).
// If your header already pulls everything in, these won't be necessary.

// Fixture that gives us a fresh simulator each test
class CompetitiveSimulatorTest : public ::testing::Test {
protected:
    // Use 1 thread & non-verbose; threads not exercised in these unit tests
    CompetitiveSimulator sim{false, 1};
};

// ---------- getAlgorithms ----------

TEST_F(CompetitiveSimulatorTest, GetAlgorithms_ReturnsFalseWhenLessThanTwo) {
    TempDir dir;

    // 0 .so files
    EXPECT_FALSE(sim.getAlgorithms(dir.path().string()));

    // 1 .so file
    touch(dir.path() / "A.so");
    EXPECT_FALSE(sim.getAlgorithms(dir.path().string()));

    // 2 .so files
    touch(dir.path() / "B.so");
    EXPECT_TRUE(sim.getAlgorithms(dir.path().string()));

    // Also verify that names were recorded
    ASSERT_GE(sim.algoNameToPath_.size(), 2u);
    EXPECT_TRUE(sim.algoNameToPath_.count("A"));
    EXPECT_TRUE(sim.algoNameToPath_.count("B"));
}

TEST_F(CompetitiveSimulatorTest, GetAlgorithms_IgnoresNonSoFiles) {
    TempDir dir;
    touch(dir.path() / "X.txt");
    touch(dir.path() / "Y.dylib");
    touch(dir.path() / "Z"); // no extension
    EXPECT_FALSE(sim.getAlgorithms(dir.path().string())); // still < 2 .so files

    touch(dir.path() / "P.so");
    touch(dir.path() / "Q.so");
    EXPECT_TRUE(sim.getAlgorithms(dir.path().string())); // now 2+
}

// ---------- loadMaps ----------

TEST_F(CompetitiveSimulatorTest, LoadMaps_FindsRegularFilesOnly) {
    TempDir dir;
    fs::create_directories(dir.path() / "sub");
    touch(dir.path() / "map1.txt");
    touch(dir.path() / "map2.bin");
    // create a directory entry that should be ignored
    touch(dir.path() / "sub" / "nested_map.txt");

    std::vector<fs::path> maps;
    EXPECT_TRUE(sim.loadMaps(dir.path().string(), maps));
    EXPECT_EQ(maps.size(), 2u);

    // Verify we captured the right basenames (order is filesystem-defined)
    std::vector<std::string> names;
    for (auto& p : maps) names.push_back(p.filename().string());
    EXPECT_NE(std::find(names.begin(), names.end(), "map1.txt"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "map2.bin"), names.end());
}

TEST_F(CompetitiveSimulatorTest, LoadMaps_EmptyFolderReturnsFalse) {
    TempDir dir;
    std::vector<fs::path> maps;
    EXPECT_FALSE(sim.loadMaps(dir.path().string(), maps));
    EXPECT_TRUE(maps.empty());
}

// ---------- scheduleGames ----------

TEST_F(CompetitiveSimulatorTest, ScheduleGames_OddN_AllPairsOnce) {
    // Prepare 3 algorithms (odd N)
    sim.algoNameToPath_.clear();
    sim.algoUsageCounts_.clear();
    sim.scheduledGames_.clear();

    sim.algoNameToPath_["A"] = "/tmp/A.so";
    sim.algoNameToPath_["B"] = "/tmp/B.so";
    sim.algoNameToPath_["C"] = "/tmp/C.so";

    sim.algoUsageCounts_["A"] = 0;
    sim.algoUsageCounts_["B"] = 0;
    sim.algoUsageCounts_["C"] = 0;

    // One map
    std::vector<fs::path> maps = { "/maps/m1" };

    sim.scheduleGames(maps);

    // For N=3: A-B, B-C, C-A → 3 games (no mirroring)
    EXPECT_EQ(sim.scheduledGames_.size(), 3u);

    // Each algo plays twice (faces two opponents once each)
    EXPECT_EQ(sim.algoUsageCounts_["A"], 2);
    EXPECT_EQ(sim.algoUsageCounts_["B"], 2);
    EXPECT_EQ(sim.algoUsageCounts_["C"], 2);

    // Sanity: A–B appears exactly once across directions
    int ab = 0;
    for (auto& g : sim.scheduledGames_) {
        if ((g.algoName1 == "A" && g.algoName2 == "B") ||
            (g.algoName1 == "B" && g.algoName2 == "A")) {
            ++ab;
        }
    }
    EXPECT_EQ(ab, 1);
}

TEST_F(CompetitiveSimulatorTest, ScheduleGames_EvenN_SkipMirrorOnMiddleRound) {
    // Prepare 4 algorithms (even N)
    sim.algoNameToPath_.clear();
    sim.algoUsageCounts_.clear();
    sim.scheduledGames_.clear();

    for (auto name : {"A","B","C","D"}) {
        sim.algoNameToPath_[name] = std::string("/tmp/") + name + ".so";
        sim.algoUsageCounts_[name] = 0;
    }

    // R = N - 1 = 3 rounds; middle round index = N/2 - 1 = 1
    // scheduleGames uses r = k % R per map index k.
    // Create 3 maps so r cycles 0,1,2 → we get mirror on r=0 and r=2, but NOT on r=1
    std::vector<fs::path> maps = { "/maps/m0", "/maps/m1", "/maps/m2" };

    sim.scheduleGames(maps);

    // For N=4:
    // r=0: 2 pairs, mirror on → 4 games
    // r=1: 2 pairs, mirror off → 2 games
    // r=2: 2 pairs, mirror on → 4 games
    // Total = 10
    EXPECT_EQ(sim.scheduledGames_.size(), 10u);

    // Per algorithm:
    // r=0: plays 1 opponent * 2 = 2
    // r=1: plays 1 opponent * 1 = 1
    // r=2: plays 1 opponent * 2 = 2
    // Total = 5
    EXPECT_EQ(sim.algoUsageCounts_["A"], 5);
    EXPECT_EQ(sim.algoUsageCounts_["B"], 5);
    EXPECT_EQ(sim.algoUsageCounts_["C"], 5);
    EXPECT_EQ(sim.algoUsageCounts_["D"], 5);
}

// ---------- updateScore ----------

TEST_F(CompetitiveSimulatorTest, UpdateScore_TieAndWinScoring) {
    sim.scores_.clear();

    // Tie between X and Y
    sim.updateScore("X", "Y", /*tie=*/true);
    EXPECT_EQ(sim.scores_["X"], 1);
    EXPECT_EQ(sim.scores_["Y"], 1);

    // Win by X over Y
    sim.updateScore("X", "Y", /*tie=*/false);
    EXPECT_EQ(sim.scores_["X"], 4); // +3
    EXPECT_EQ(sim.scores_["Y"], 1); // unchanged
}

// ---------- writeOutput ----------

TEST_F(CompetitiveSimulatorTest, WriteOutput_CreatesFileWithSortedScores) {
    TempDir outDir;

    // Prepare some scores out of order
    sim.scores_.clear();
    sim.scores_["Gamma"] = 5;
    sim.scores_["Alpha"] = 12;
    sim.scores_["Beta"]  = 8;

    // Write output; gm path only affects header's filename display
    sim.writeOutput(outDir.path().string(), "/maps", "/some/path/GameManager.so");

    // Find the created file (competition_*.txt) in outDir
    std::optional<fs::path> found;
    for (auto& e : fs::directory_iterator(outDir.path())) {
        if (e.is_regular_file() && e.path().filename().string().rfind("competition_", 0) == 0 &&
            e.path().extension() == ".txt") {
            found = e.path();
            break;
        }
    }
    ASSERT_TRUE(found.has_value()) << "Output file not found in " << outDir.path();

    // Read contents
    std::ifstream in(found->string());
    ASSERT_TRUE(in.is_open());

    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Basic header checks
    EXPECT_NE(contents.find("game_maps_folder=/maps"), std::string::npos);
    EXPECT_NE(contents.find("game_manager=GameManager.so"), std::string::npos);

    // Scores must be sorted descending: Alpha 12, Beta 8, Gamma 5 (one per line)
    // Use regex to allow for either \n or \r\n newlines
    std::regex expected(
        ".*Alpha\\s+12\\s*\\nBeta\\s+8\\s*\\nGamma\\s+5\\s*\\n?", std::regex::ECMAScript);
    EXPECT_TRUE(std::regex_search(contents, expected)) << "Got:\n" << contents;
}

// ------------------------- E2E test -------------------------
TEST(E2E, CompetitiveSimulatorRun) {
    const char* mapsEnv = std::getenv("E2E_MAPS_DIR");
    const char* gmEnv   = std::getenv("E2E_GM_SO");
    const char* algEnv  = std::getenv("E2E_ALGOS_DIR");

    if (!mapsEnv || !gmEnv || !algEnv) {
        GTEST_SKIP() << "Skipping E2E: set E2E_MAPS_DIR, E2E_GM_SO, E2E_ALGOS_DIR to run.";
    }

    namespace fs = std::filesystem;
    const fs::path mapsFolder = fs::path(mapsEnv);
    const fs::path gmSoPath   = fs::path(gmEnv);
    const fs::path algFolder  = fs::path(algEnv);

    ASSERT_TRUE(fs::exists(mapsFolder)) << "maps folder not found: " << mapsFolder;
    ASSERT_TRUE(fs::exists(gmSoPath))   << "GameManager .so not found: " << gmSoPath;
    ASSERT_TRUE(fs::exists(algFolder))  << "algorithms folder not found: " << algFolder;

    // Pre-scan existing competition_*.txt to detect the new file reliably
    std::unordered_set<std::string> preExisting;
    for (const auto& e : fs::directory_iterator(algFolder)) {
        if (e.is_regular_file()) {
            auto fname = e.path().filename().string();
            if (fname.rfind("competition_", 0) == 0 && e.path().extension() == ".txt") {
                preExisting.insert(fname);
            }
        }
    }

    // Use multiple threads to exercise the pool
    CompetitiveSimulator sim(/*verbose=*/false, /*numThreads=*/std::thread::hardware_concurrency());

    // Run end-to-end
    int rc = sim.run(mapsFolder.string(), gmSoPath.string(), algFolder.string());
    ASSERT_EQ(rc, 0) << "CompetitiveSimulator::run returned non-zero";

    // Find the newly created output file
    std::optional<fs::path> outFile;
    for (const auto& e : fs::directory_iterator(algFolder)) {
        if (!e.is_regular_file()) continue;
        auto fname = e.path().filename().string();
        if (fname.rfind("competition_", 0) == 0 && e.path().extension() == ".txt") {
            if (!preExisting.count(fname)) {
                outFile = e.path();
                break;
            }
        }
    }
    ASSERT_TRUE(outFile.has_value()) << "No new competition_*.txt found in " << algFolder;

    // Read and validate contents
    std::ifstream in(outFile->string());
    ASSERT_TRUE(in.is_open()) << "Failed to open output file: " << *outFile;
    const std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Header checks
    const std::string expectedMapsHeader = "game_maps_folder=" + mapsFolder.string();
    EXPECT_NE(contents.find(expectedMapsHeader), std::string::npos)
        << "Missing/incorrect maps header. Expected: " << expectedMapsHeader << "\nGot:\n" << contents;

    const std::string gmBase = fs::path(gmSoPath).filename().string();
    const std::string expectedGmHeader = "game_manager=" + gmBase;
    EXPECT_NE(contents.find(expectedGmHeader), std::string::npos)
        << "Missing/incorrect game manager header. Expected: " << expectedGmHeader << "\nGot:\n" << contents;

    // Scores section: require at least two "name score" lines after the blank line
    // Extract the part after the blank line separating headers and scores
    auto splitPos = contents.find("\n\n");
    ASSERT_NE(splitPos, std::string::npos) << "Output missing blank line before scores.";
    const std::string scores = contents.substr(splitPos + 2);

    // Count lines with "<non-space> <integer>"
    std::istringstream ss(scores);
    std::string line;
    int validScoreLines = 0;
    std::regex lineRe(R"(^\S+\s+\d+\s*$)");
    while (std::getline(ss, line)) {
        if (std::regex_match(line, lineRe)) ++validScoreLines;
    }
    EXPECT_GE(validScoreLines, 2) << "Expected scores for at least two algorithms.\nScores block:\n" << scores;
}
