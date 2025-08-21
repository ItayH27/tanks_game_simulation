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
#include "./utils.test.cpp"

namespace fs = std::filesystem;

// --- test-only access to internals ---
#define private public
#define protected public
#include "../sim_include/comparative_simulator.h"
#undef private
#undef protected

// ---------- Helpers ----------

// Make a tiny board from vector<vector<char>>
static ComparativeSimulator::SnapshotGameResult mkResult(
    int winner, int reason, size_t rounds, std::vector<std::vector<char>> boardRows) {
    ComparativeSimulator::SnapshotGameResult r;
    r.winner = winner;
    r.reason = GameResult::Reason(reason);
    r.rounds = rounds;
    r.board  = std::move(boardRows);
    r.remaining_tanks.clear(); // not used by these tests
    return r;
}

// ---------- Fixture ----------
class ComparativeSimulatorUnitTest : public ::testing::Test {
protected:
    ComparativeSimulator sim{false, 1};
};

// ===================== getGameManagers =====================

TEST_F(ComparativeSimulatorUnitTest, GetGameManagers_OnlySoFilesDiscovered) {
    TempDir dir;
    // Mix of files
    touch(dir.path() / "gmA.so");
    touch(dir.path() / "gmB.so");
    touch(dir.path() / "not_a_gm.txt");
    touch(dir.path() / "libgm.dylib");
    fs::create_directories(dir.path() / "sub"); // directory should be ignored
    touch(dir.path() / "sub" / "nested.so");

    sim.gms_paths_.clear();
    sim.getGameManagers(dir.path().string());

    // We expect only top-level *.so files
    std::unordered_set<std::string> basenames;
    for (auto& p : sim.gms_paths_) basenames.insert(p.filename().string());

    EXPECT_EQ(sim.gms_paths_.size(), 2u);
    EXPECT_TRUE(basenames.count("gmA.so"));
    EXPECT_TRUE(basenames.count("gmB.so"));
    EXPECT_FALSE(basenames.count("nested.so"));
}

// ===================== sameResult =====================

TEST_F(ComparativeSimulatorUnitTest, SameResult_PositiveWhenAllMatch) {
    auto a = mkResult(1, 2, 100, rows({"abc", "def"}));
    auto b = mkResult(1, 2, 100, rows({"abc", "def"}));
    EXPECT_TRUE(sim.sameResult(a, b));
}

TEST_F(ComparativeSimulatorUnitTest, SameResult_FalseOnWinnerMismatch) {
    auto a = mkResult(1, 2, 100, rows({"abc"}));
    auto b = mkResult(2, 2, 100, rows({"abc"}));
    EXPECT_FALSE(sim.sameResult(a, b));
}

TEST_F(ComparativeSimulatorUnitTest, SameResult_FalseOnReasonMismatch) {
    auto a = mkResult(1, 2, 100, rows({"abc"}));
    auto b = mkResult(1, 3, 100, rows({"abc"}));
    EXPECT_FALSE(sim.sameResult(a, b));
}

TEST_F(ComparativeSimulatorUnitTest, SameResult_FalseOnRoundsMismatch) {
    auto a = mkResult(1, 2, 100, rows({"abc"}));
    auto b = mkResult(1, 2, 101, rows({"abc"}));
    EXPECT_FALSE(sim.sameResult(a, b));
}

TEST_F(ComparativeSimulatorUnitTest, SameResult_FalseOnBoardMismatch) {
    auto a = mkResult(1, 2, 100, rows({"abc", "def"}));
    auto b = mkResult(1, 2, 100, rows({"abc", "deg"})); // 2nd row differs
    EXPECT_FALSE(sim.sameResult(a, b));
}

// ===================== makeGroups =====================

TEST_F(ComparativeSimulatorUnitTest, MakeGroups_ClustersEqualResultsAndCounts) {
    // Prepare three results: two identical, one different
    sim.groups.clear();
    std::vector<std::pair<ComparativeSimulator::SnapshotGameResult, std::string>> results;
    results.emplace_back(mkResult(1, 2, 100, rows({"..", "##"})), "GM_A");
    results.emplace_back(mkResult(1, 2, 100, rows({"..", "##"})), "GM_B"); // same as first
    results.emplace_back(mkResult(2, 1, 42,  rows({"xx"})),        "GM_C"); // different

    sim.makeGroups(results);

    // Expect two groups: one with count=2 (GM_A, GM_B), one with count=1 (GM_C)
    ASSERT_EQ(sim.groups.size(), 2u);

    // Find the group with count=2
    const auto* g2 = [&]()->const ComparativeSimulator::GameResultInfo*{
        for (const auto& g : sim.groups) if (g.count == 2) return &g; return nullptr;
    }();
    ASSERT_NE(g2, nullptr);
    EXPECT_EQ(g2->gm_names.size(), 2u);
    EXPECT_NE(std::find(g2->gm_names.begin(), g2->gm_names.end(), "GM_A"), g2->gm_names.end());
    EXPECT_NE(std::find(g2->gm_names.begin(), g2->gm_names.end(), "GM_B"), g2->gm_names.end());

    // And the singleton group
    const auto* g1 = [&]()->const ComparativeSimulator::GameResultInfo*{
        for (const auto& g : sim.groups) if (g.count == 1) return &g; return nullptr;
    }();
    ASSERT_NE(g1, nullptr);
    EXPECT_EQ(g1->gm_names.size(), 1u);
    EXPECT_EQ(g1->gm_names[0], "GM_C");
}

// ===================== printSatellite =====================

TEST_F(ComparativeSimulatorUnitTest, PrintSatellite_RendersBoardWithNewlines) {
    std::ostringstream oss;
    auto res = mkResult(1, 0, 3, rows({"ab", "cd", "ef"}));
    sim.printSatellite(oss, res);
    EXPECT_EQ(oss.str(), "ab\ncd\nef\n");
}

// ===================== BuildOutputBuffer =====================

TEST_F(ComparativeSimulatorUnitTest, BuildOutputBuffer_FormatsHeadersAndGroups) {
    // Prepare groups vector (already “grouped”)
    sim.groups.clear();
    // Two groups: counts 1 and 3 — note writeOutput sorts ascending then pops back.
    ComparativeSimulator::GameResultInfo gA;
    gA.result = mkResult(1, 7, 12, rows({"AA", "BB"}));
    gA.gm_names = {"GM_Z", "GM_Y"};
    gA.count = 3; // frequent

    ComparativeSimulator::GameResultInfo gB;
    gB.result = mkResult(2, 9, 20, rows({"**"}));
    gB.gm_names = {"GM_X"};
    gB.count = 1; // less frequent

    // Emulate writeOutput behavior: ascending sort then pop_back
    sim.groups.push_back(gB); // count=1
    sim.groups.push_back(gA); // count=3 (emitted first)

    const std::string mapPath = "/maps/demo.map";
    const std::string algo1   = "/algos/A1.so";
    const std::string algo2   = "/algos/A2.so";

    std::string buf = sim.BuildOutputBuffer(mapPath, algo1, algo2);

    // Headers use only basenames
    EXPECT_NE(buf.find("game_map=demo.map"), std::string::npos);
    EXPECT_NE(buf.find("algorithm1=A1.so"), std::string::npos);
    EXPECT_NE(buf.find("algorithm2=A2.so"), std::string::npos);

    // Expect the frequent group (gA) first:
    // Names: "GM_Z, GM_Y"
    EXPECT_NE(buf.find("GM_Z, GM_Y\n"), std::string::npos);
    EXPECT_NE(buf.find("Player 1 won with 0 tanks still alive\n"), std::string::npos);
    EXPECT_NE(buf.find("AA\nBB\n"), std::string::npos);

    // Followed by the less frequent group (gB)
    EXPECT_NE(buf.find("GM_X\n"), std::string::npos);
    EXPECT_NE(buf.find("Player 2 won with 0 tanks still alive\n"), std::string::npos);
    EXPECT_NE(buf.find("**\n"), std::string::npos);

    // There should be at least two "Winner:" occurrences
    size_t countWinner = 0;
    for (size_t pos = 0; (pos = buf.find("won", pos)) != std::string::npos; ++pos) ++countWinner;
    EXPECT_GE(countWinner, 2u);
}

TEST_F(ComparativeSimulatorUnitTest, BuildOutputBuffer_ShowsRemainingTanks) {
    sim.groups.clear();

    // --- Prepare groups with explicit remaining_tanks ---
    ComparativeSimulator::GameResultInfo gA;
    gA.result = mkResult(1, 7, 12, rows({"AA", "BB"}));
    gA.result.remaining_tanks = {12, 0}; // Player 1 has 12 tanks
    gA.gm_names = {"GM_Z", "GM_Y"};
    gA.count = 3;

    ComparativeSimulator::GameResultInfo gB;
    gB.result = mkResult(2, 9, 20, rows({"**"}));
    gB.result.remaining_tanks = {0, 20}; // Player 2 has 20 tanks
    gB.gm_names = {"GM_X"};
    gB.count = 1;

    // Emulate writeOutput behavior: ascending sort then pop back
    sim.groups.push_back(gB); // less frequent
    sim.groups.push_back(gA); // more frequent

    const std::string mapPath = "/maps/demo.map";
    const std::string algo1   = "/algos/A1.so";
    const std::string algo2   = "/algos/A2.so";

    std::string buf = sim.BuildOutputBuffer(mapPath, algo1, algo2);

    // --- Assertions ---
    // Headers
    EXPECT_NE(buf.find("game_map=demo.map"), std::string::npos);
    EXPECT_NE(buf.find("algorithm1=A1.so"), std::string::npos);
    EXPECT_NE(buf.find("algorithm2=A2.so"), std::string::npos);

    // Group GM names
    EXPECT_NE(buf.find("GM_Z, GM_Y"), std::string::npos);
    EXPECT_NE(buf.find("GM_X"), std::string::npos);

    // Player wins with correct remaining tanks
    EXPECT_NE(buf.find("Player 1 won with 12 tanks still alive"), std::string::npos);
    EXPECT_NE(buf.find("Player 2 won with 20 tanks still alive"), std::string::npos);

    // Board rows
    EXPECT_NE(buf.find("AA\nBB"), std::string::npos);
    EXPECT_NE(buf.find("**"), std::string::npos);

    // Ensure at least two "Player" lines
    size_t countPlayer = 0;
    for (size_t pos = 0; (pos = buf.find("Player", pos)) != std::string::npos; ++pos) ++countPlayer;
    EXPECT_GE(countPlayer, 2u);
}

// ===================== writeOutput =====================

TEST_F(ComparativeSimulatorUnitTest, WriteOutput_CreatesFileAndWritesSortedGroups) {
    TempDir out;
    sim.groups.clear();
    sim.allResults.clear();

    // Make results: two identical (GM_A, GM_B) and one different (GM_C)
    auto R1 = mkResult(1, 2, 100, rows({"..", "##"}));
    auto R2 = mkResult(1, 2, 100, rows({"..", "##"})); // same
    auto R3 = mkResult(0, 0,   5,   rows({"xo"}));

    sim.allResults.emplace_back(R1, "GM_A");
    sim.allResults.emplace_back(R2, "GM_B");
    sim.allResults.emplace_back(R3, "GM_C");

    // Invoke writeOutput; it will call makeGroups(allResults), sort groups ascending by count,
    // then BuildOutputBuffer pops from back (so the most frequent appears first in the file).
    const std::string mapPath = "/maps/m.map";
    const std::string a1      = "/algos/Algo1.so";
    const std::string a2      = "/algos/Algo2.so";
    sim.writeOutput(mapPath, a1, a2, out.path().string());

    // Find the comparative_results_*.txt
    std::optional<fs::path> found;
    for (const auto& e : fs::directory_iterator(out.path())) {
        if (e.is_regular_file() &&
            e.path().filename().string().rfind("comparative_results_", 0) == 0 &&
            e.path().extension() == ".txt") {
            found = e.path();
            break;
        }
    }
    ASSERT_TRUE(found.has_value()) << "No comparative_results_*.txt produced";

    // Read back
    std::ifstream in(found->string());
    ASSERT_TRUE(in.is_open());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Header basenames
    EXPECT_NE(contents.find("game_map=m.map"), std::string::npos);
    EXPECT_NE(contents.find("algorithm1=Algo1.so"), std::string::npos);
    EXPECT_NE(contents.find("algorithm2=Algo2.so"), std::string::npos);

    // Most frequent group (R1/R2) should appear before the singleton group (R3)
    // Check both GM names are comma-separated on one line
    EXPECT_NE(contents.find("GM_A, GM_B\n"), std::string::npos);
    EXPECT_NE(contents.find("Player 1 won with 0 tanks still alive\n"), std::string::npos);
    EXPECT_NE(contents.find("..\n##\n"), std::string::npos);

    // And the singleton group:
    EXPECT_NE(contents.find("GM_C\n"), std::string::npos);
    EXPECT_NE(contents.find("Tie, both players have zero tanks\n"), std::string::npos);
    EXPECT_NE(contents.find("xo\n"), std::string::npos);
}

// ------------------------- E2E: ComparativeSimulator -------------------------
TEST(E2E_Comparative, RunFull) {
    const char* mapEnv   = std::getenv("E2E_COMP_MAP");
    const char* gmDirEnv = std::getenv("E2E_COMP_GM_DIR");
    const char* a1Env    = std::getenv("E2E_COMP_ALGO1");
    const char* a2Env    = std::getenv("E2E_COMP_ALGO2");

    if (!mapEnv || !gmDirEnv || !a1Env || !a2Env) {
        GTEST_SKIP() << "Set E2E_COMP_MAP, E2E_COMP_GM_DIR, E2E_COMP_ALGO1, E2E_COMP_ALGO2 to run this E2E.";
    }

    namespace fs = std::filesystem;
    const fs::path mapPath   = fs::path(mapEnv);
    const fs::path gmDirPath = fs::path(gmDirEnv);
    const fs::path algo1Path = fs::path(a1Env);
    const fs::path algo2Path = fs::path(a2Env);

    ASSERT_TRUE(fs::exists(mapPath)   && fs::is_regular_file(mapPath))   << "Map file invalid: " << mapPath;
    ASSERT_TRUE(fs::exists(gmDirPath) && fs::is_directory(gmDirPath))    << "GM dir invalid: "  << gmDirPath;
    ASSERT_TRUE(fs::exists(algo1Path) && fs::is_regular_file(algo1Path)) << "Algo1 invalid: "   << algo1Path;
    ASSERT_TRUE(fs::exists(algo2Path) && fs::is_regular_file(algo2Path)) << "Algo2 invalid: "   << algo2Path;

    // Gather at least one GM .so name (for validation later)
    std::vector<std::string> gmSoBasenames;
    for (const auto& e : fs::directory_iterator(gmDirPath)) {
        if (e.is_regular_file() && e.path().extension() == ".so") {
            gmSoBasenames.push_back(e.path().stem().string());
        }
    }
    ASSERT_FALSE(gmSoBasenames.empty()) << "No GameManager .so files found in E2E_COMP_GM_DIR";

    // Pre-scan existing comparative outputs to detect newly created file
    std::unordered_set<std::string> preExisting;
    for (const auto& e : fs::directory_iterator(gmDirPath)) {
        if (e.is_regular_file()) {
            auto fname = e.path().filename().string();
            if (fname.rfind("comparative_results_", 0) == 0 && e.path().extension() == ".txt") {
                preExisting.insert(fname);
            }
        }
    }

    // Run the simulator end-to-end (use multiple threads to exercise pool)
    ComparativeSimulator cs(/*verbose=*/false, /*numThreads=*/std::thread::hardware_concurrency());
    int rc = cs.run(mapPath.string(), gmDirPath.string(), algo1Path.string(), algo2Path.string());
    ASSERT_EQ(rc, 0) << "ComparativeSimulator::run returned non-zero";

    // Find the newly created comparative_results_*.txt in gm folder
    std::optional<fs::path> outFile;
    for (const auto& e : fs::directory_iterator(gmDirPath)) {
        if (!e.is_regular_file()) continue;
        auto fname = e.path().filename().string();
        if (fname.rfind("comparative_results_", 0) == 0 && e.path().extension() == ".txt") {
            if (!preExisting.count(fname)) { outFile = e.path(); break; }
        }
    }
    ASSERT_TRUE(outFile.has_value()) << "No new comparative_results_*.txt found in " << gmDirPath;

    // Read and validate content
    std::ifstream in(outFile->string());
    ASSERT_TRUE(in.is_open()) << "Failed to open output file: " << *outFile;
    const std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Header lines must include basenames of map and both algorithms
    const std::string mapBase  = mapPath.filename().string();
    const std::string a1Base   = algo1Path.filename().string();
    const std::string a2Base   = algo2Path.filename().string();

    EXPECT_NE(contents.find(std::string("game_map=") + mapBase), std::string::npos)
        << "Missing/incorrect game_map header. Got:\n" << contents;
    EXPECT_NE(contents.find(std::string("algorithm1=") + a1Base), std::string::npos)
        << "Missing/incorrect algorithm1 header. Got:\n" << contents;
    EXPECT_NE(contents.find(std::string("algorithm2=") + a2Base), std::string::npos)
        << "Missing/incorrect algorithm2 header. Got:\n" << contents;

    // Ensure at least one GM name from folder appears in results section
    bool foundAnyGmName = false;
    for (const auto& gm : gmSoBasenames) {
        if (contents.find(gm) != std::string::npos) { foundAnyGmName = true; break; }
    }
    EXPECT_TRUE(foundAnyGmName) << "No GameManager names from folder appeared in results.\n" << contents;

    // Ensure at least one "Winner: <num>, Reason: <num>" block exists
    std::regex winRe(R"(Winner:\s*\d+\s*,\s*Reason:\s*\d+)");
    EXPECT_TRUE(std::regex_search(contents, winRe))
        << "No winner/reason block found.\n" << contents;
}
