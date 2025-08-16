#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <mutex>  // Required for std::mutex
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistrar.h"
#include "Simulator.h"
#include "GameResult.h"
#include "AbstractGameManager.h"

using std::string, std::filesystem::path, std::shared_ptr, std::vector, std::unordered_map, std::mutex;

class ComparativeSimulator : public Simulator {
public:
    ComparativeSimulator(bool verbose, size_t numThreads);
    ~ComparativeSimulator();

    int run(const string& mapPath,
            const string& algorithmSoPath1,
            const string& algorithmSoPath2,
            const string& gmFolder);
private:

    bool verbose_;
    size_t numThreads_;
    vector<void*> handles_;
    MapData mapData_ = {};

    // void* algorithmHandle1_ = nullptr;
    // void* algorithmHandle2_ = nullptr;

    AlgorithmRegistrar* algo_registrar;
    GameManagerRegistrar game_manager_registrar;

    // AlgorithmRegistrar::AlgorithmAndPlayerFactories algorithmFactory1_;
    // AlgorithmRegistrar::AlgorithmAndPlayerFactories algorithmFactory2_;


    static unordered_map<GameResult, vector<string>> gameResToGameManagers_;

    vector<path> gms_paths_;

    bool loadSO (const string& path);

    void getGameManagers(const string& gameManagerFolder);
    // bool loadGameManagers(const vector<path>& gms_Paths);
    void runGames();
    void runSingleGame(const path& gmPath);

    struct GameResultInfo {
        GameResult result;
        vector<string> gm_names;
        int count = 0;
    };

    vector<pair<GameResult,string>> allResults;
    vector<GameResultInfo> groups;

    bool sameResult(const GameResult& a, const GameResult& b) const;
    void makeGroups(vector<pair<GameResult,string>>& results);

    void writeOutput(const string& mapPath,
                     const string& algorithmSoPath1,
                     const string& algorithmSoPath2,
                     const string& gmFolder);
    string BuildOutputBuffer(const string& mapPath,
                     const string& algorithmSoPath1,
                     const string& algorithmSoPath2);

    static string getFilename(const string& path) {
        return std::filesystem::path(path).filename().string();
    }
};