#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistration.h"
#include "AbstractGameManager.h"

class CompetitiveSimulator {
public:
    CompetitiveSimulator(bool verbose, size_t numThreads);
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

    bool loadGameManager(const std::string& soPath);
    bool loadAlgorithms(const std::string& folder);
    bool loadMaps(const std::string& folder, std::vector<std::filesystem::path>& outMaps);
    void scheduleGames(const std::vector<std::filesystem::path>& maps);
    void runGames();
    void runSingleGame(const GameTask& task);
    void updateScore(const std::string& winnerName, const std::string& loserName, bool tie);
    void writeOutput(const std::string& outFolder, const std::string& mapFolder, const std::string& gmSoName);
    std::unique_ptr<AbstractGameManager> createGameManager();
    std::string timestamp();
};
