#define UNIT_TEST
#include "sim_include/competitive_simulator.h"
#include "gtest/gtest.h"
#include "sim_include/competitive_simulator.h"
#include "../common/Player.h"
#include "../common/TankAlgorithm.h"
#include "sim_include/AlgorithmRegistrar.h"
#include <fstream>
#include <filesystem>


// ==========================
// Fake classes for registration
// ==========================

class FakePlayer : public Player {
public:
    FakePlayer(int, size_t, size_t, size_t, size_t) {}
    void updateTankWithBattleInfo(TankAlgorithm&, SatelliteView&) override {}
};

class FakeTankAlgorithm : public TankAlgorithm {
public:
    FakeTankAlgorithm(int, int) {}
    ActionRequest getAction() override { return ActionRequest::DoNothing; }
    void updateBattleInfo(BattleInfo&) override {}
};

// ==========================
// Fake dlopen-like registration
// ==========================

void* fakeDlopenAndRegister(const std::string& name) {
    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
    registrar.createAlgorithmFactoryEntry(name);
    registrar.addPlayerFactoryToLastEntry(
        [](int p, size_t x, size_t y, size_t s, size_t n) {
            return std::make_unique<FakePlayer>(p, x, y, s, n);
        });
    registrar.addTankAlgorithmFactoryToLastEntry(
        [](int p, int t) {
            return std::make_unique<FakeTankAlgorithm>(p, t);
        });
    return reinterpret_cast<void*>(0xDEADBEEF);
}

// ==========================
// Derived simulator for testing internal behavior
// ==========================

class TestSimulator : public CompetitiveSimulator {
public:
    using CompetitiveSimulator::algoNameToPath_;
    using CompetitiveSimulator::algoUsageCounts_;
    using CompetitiveSimulator::algoPathToHandle_;
    using CompetitiveSimulator::scheduledGames_;
    using CompetitiveSimulator::scores_;
    using CompetitiveSimulator::updateScore;
    using CompetitiveSimulator::scheduleGames;
    using CompetitiveSimulator::decreaseUsageCount;
    using CompetitiveSimulator::loadMaps;
    using CompetitiveSimulator::getValidatedAlgorithm;
    using CompetitiveSimulator::algorithms_;
    using CompetitiveSimulator::handlesMutex_;
};

class TestSimulatorWithFakeDlopen : public TestSimulator {
public:
    using TestSimulator::TestSimulator;

    void ensureAlgorithmLoadedFake(const std::string& name) {
        std::lock_guard<std::mutex> lock(handlesMutex_);
        const std::string& soPath = algoNameToPath_[name];
        if (algoPathToHandle_.count(soPath)) return;

        void* handle = fakeDlopenAndRegister(name);
        algoPathToHandle_[soPath] = handle;

        try {
            AlgorithmRegistrar::getAlgorithmRegistrar().validateLastRegistration();
            algorithms_.push_back(std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(
                AlgorithmRegistrar::getAlgorithmRegistrar().end()[-1]));
        } catch (...) {
            algoPathToHandle_.erase(soPath);
            algoNameToPath_.erase(name);
            algoUsageCounts_.erase(name);
            throw;
        }
    }
};

// ==========================
// TESTS
// ==========================

TEST(CompetitiveSimulatorTest, ScheduleGamesCreatesCorrectPairings) {
    TestSimulator sim;
    sim.algoNameToPath_ = {
        {"AlgoA", "path/to/algoA.so"},
        {"AlgoB", "path/to/algoB.so"},
        {"AlgoC", "path/to/algoC.so"}
    };
    sim.algoUsageCounts_ = {
        {"AlgoA", 0},
        {"AlgoB", 0},
        {"AlgoC", 0}
    };

    std::vector<std::filesystem::path> dummyMaps = {
        "map1.txt", "map2.txt"
    };

    sim.scheduleGames(dummyMaps);

    EXPECT_EQ(sim.scheduledGames_.size(), 6); // 3 matchups * 2 maps
    for (const auto& [name, count] : sim.algoUsageCounts_) {
        EXPECT_EQ(count, 4); // each plays 2 games/map × 2 maps = 4
    }
}

TEST(CompetitiveSimulatorTest, UpdateScoreWinAndTie) {
    TestSimulator sim;
    sim.updateScore("AlgoX", "AlgoY", false);
    sim.updateScore("AlgoA", "AlgoB", true);

    EXPECT_EQ(sim.scores_["AlgoX"], 3);
    EXPECT_EQ(sim.scores_["AlgoY"], 0);
    EXPECT_EQ(sim.scores_["AlgoA"], 1);
    EXPECT_EQ(sim.scores_["AlgoB"], 1);
}

TEST(CompetitiveSimulatorTest, DecreaseUsageCountUnloadsAlgorithm) {
    TestSimulator sim;
    sim.algoNameToPath_["AlgoZ"] = "path/to/algoZ.so";
    sim.algoUsageCounts_["AlgoZ"] = 1;
    sim.algoPathToHandle_["path/to/algoZ.so"] = reinterpret_cast<void*>(0xDEADBEEF);

    sim.decreaseUsageCount("AlgoZ");

    EXPECT_EQ(sim.algoUsageCounts_.count("AlgoZ"), 0);
    EXPECT_EQ(sim.algoNameToPath_.count("AlgoZ"), 0);
    EXPECT_EQ(sim.algoPathToHandle_.count("path/to/algoZ.so"), 0);
}

TEST(CompetitiveSimulatorTest, LoadMapsLoadsOnlyFiles) {
    TestSimulator sim;
    std::vector<std::filesystem::path> out;

    std::filesystem::create_directory("test_maps");
    std::ofstream("test_maps/map1.txt");
    std::ofstream("test_maps/map2.txt");

    bool result = sim.loadMaps("test_maps", out);

    EXPECT_TRUE(result);
    EXPECT_EQ(out.size(), 2);

    std::filesystem::remove_all("test_maps");
}

TEST(CompetitiveSimulatorTest, EnsureAlgorithmLoadedFakeRegistersSuccessfully) {
    AlgorithmRegistrar::getAlgorithmRegistrar().clear();

    TestSimulatorWithFakeDlopen sim;
    sim.algoNameToPath_["FakeAlgo"] = "FakeAlgo.so";
    sim.algoUsageCounts_["FakeAlgo"] = 1;

    sim.ensureAlgorithmLoadedFake("FakeAlgo");
    auto algo = sim.getValidatedAlgorithm("FakeAlgo");

    ASSERT_NE(algo, nullptr);
    EXPECT_TRUE(algo->hasPlayerFactory());
    EXPECT_TRUE(algo->hasTankAlgorithmFactory());
}
