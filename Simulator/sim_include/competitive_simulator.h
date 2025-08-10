#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <mutex>  // Required for std::mutex
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistration.h"
#include "AbstractGameManager.h"
#include "Simulator.h"

/**
 * @brief Tracks a shared library handle and its usage count.
 */

class CompetitiveSimulator : Simulator {
public:
    CompetitiveSimulator(bool verbose, size_t numThreads);
    ~CompetitiveSimulator();

    int run(const std::string& mapsFolder,
            const std::string& gameManagerSoPath,
            const std::string& algorithmsFolder);

private:
    struct GameTask {
        std::filesystem::path mapPath;
        std::shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories> algo1;
        std::shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories> algo2;
    };

    bool verbose_;
    size_t numThreads_;
    void* gameManagerHandle_ = nullptr;
    GameManagerFactory gameManagerFactory_;

    std::vector<std::shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories>> algorithms_;
    std::vector<GameTask> scheduledGames_;
    std::unordered_map<std::string, int> scores_;
    std::mutex scoresMutex_; // protect score updates
    std::unordered_map<std::string, void*> algoPathToHandle_;
    std::unordered_map<std::string, std::string> algoNameToPath_;
    std::unordered_map<std::string, int> algoUsageCounts_;
    std::mutex handlesMutex_;

    bool loadGameManager(const std::string& soPath);
    bool getAlgorithms(const std::string& folder);
    bool loadMaps(const std::string& folder, std::vector<std::filesystem::path>& outMaps);
    void scheduleGames(const std::vector<std::filesystem::path>& maps);
    void runGames();
    void CompetitiveSimulator::ensureAlgorithmLoaded(const std::string& name);
    std::shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories>
        CompetitiveSimulator::getValidatedAlgorithm(const std::string& name);
    void runSingleGame(const GameTask& task);
    void updateScore(const std::string& winnerName, const std::string& loserName, bool tie);
    void writeOutput(const std::string& outFolder, const std::string& mapFolder, const std::string& gmSoName);
    std::unique_ptr<AbstractGameManager> createGameManager();
    void decreaseUsageCount(const std::string& algoName);
};
