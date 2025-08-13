#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <mutex>
#include <thread>
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistration.h"
#include "Simulator.h"

using std::string, std::vector, std::unordered_map, std::mutex, std::shared_ptr, std::lock_guard, std::pair,
    std::unique_ptr, std::ofstream, std::ifstream, std::sort, std::cout, std::endl, std::exception, std::make_shared,
    std::min, std::thread;
namespace fs = std::filesystem;

class CompetitiveSimulator : public Simulator {
#ifdef UNIT_TEST
    friend class TestSimulator;
    friend class TestSimulatorWithFakeDlopen;
#endif
public:
    CompetitiveSimulator(bool verbose, size_t numThreads);
    ~CompetitiveSimulator();

    int run(const string& mapsFolder,
            const string& gameManagerSoPath,
            const string& algorithmsFolder);

private:
    struct GameTask {
        fs::path mapPath;
        string algoName1;
        string algoName2;
    };

    vector<shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories>> algorithms_;
    vector<GameTask> scheduledGames_;
    unordered_map<string, int> scores_;
    mutex scoresMutex_; // protect score updates
    unordered_map<string, void*> algoPathToHandle_;
    unordered_map<string, string> algoNameToPath_;
    unordered_map<string, int> algoUsageCounts_;
    mutex handlesMutex_;
    void* gameManagerHandle_ = nullptr;
    GameManagerFactory gameManagerFactory_;

    bool loadGameManager(const string& soPath);
    bool getAlgorithms(const string& folder);
    bool loadMaps(const string& folder, vector<fs::path>& outMaps);
    void scheduleGames(const vector<fs::path>& maps);
    void runGames();
    void CompetitiveSimulator::ensureAlgorithmLoaded(const string& name);
    shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories>
        CompetitiveSimulator::getValidatedAlgorithm(const string& name);
    void runSingleGame(const GameTask& task);
    void updateScore(const string& winnerName, const string& loserName, bool tie);
    void writeOutput(const string& outFolder, const string& mapFolder, const string& gmSoName);
    std::unique_ptr<AbstractGameManager> createGameManager();
    void decreaseUsageCount(const string& algoName);
};
