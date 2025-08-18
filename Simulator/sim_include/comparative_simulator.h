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
    ComparativeSimulator(const ComparativeSimulator&) = delete;
    ComparativeSimulator& operator=(const ComparativeSimulator&) = delete;
    ComparativeSimulator(ComparativeSimulator&&) = delete;
    ComparativeSimulator& operator=(ComparativeSimulator&&) = delete;
    ~ComparativeSimulator();

    int run(const string& mapPath,
            const string& algorithmSoPath1,
            const string& algorithmSoPath2,
            const string& gmFolder);
private:

    vector<void*> algoHandles_;
    mutex handlesMutex_;
    mutex allResultsMutex_;
    mutex gmRegistrarmutex_;
    MapData mapData_ = {};

    // void* algorithmHandle1_ = nullptr;
    // void* algorithmHandle2_ = nullptr;

    AlgorithmRegistrar* algo_registrar;
    GameManagerRegistrar* game_manager_registrar;

    shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories> algo1_;
    shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories> algo2_;

    static unordered_map<GameResult, vector<string>> gameResToGameManagers_;

    vector<path> gms_paths_;

    bool loadAlgoSO(const string& path);
    void* loadGameManagerSO(const string& path);
    void getGameManagers(const string& gameManagerFolder);
    // bool loadGameManagers(const vector<path>& gms_Paths);
    void runGames();
    void runSingleGame(const path& gmPath);

    

    struct SnapshotGameResult {
        int winner;
        GameResult::Reason reason;
        std::vector<size_t> remaining_tanks;
        std::vector<std::vector<char>> board; // hard copy of final board
        size_t rounds;
    };

    SnapshotGameResult makeSnapshot(const GameResult& gr, size_t rows, size_t cols) {
        SnapshotGameResult snap;
        snap.winner = gr.winner;
        snap.reason = gr.reason;
        snap.remaining_tanks = gr.remaining_tanks;
        snap.rounds = gr.rounds;

        snap.board.resize(rows, std::vector<char>(cols));
        for (size_t y = 0; y < rows; ++y) {
            for (size_t x = 0; x < cols; ++x) {
                snap.board[y][x] = gr.gameState->getObjectAt(x, y);
            }
        }
        return snap;
    }

    struct GameResultInfo {
        SnapshotGameResult result;
        vector<string> gm_names;
        int count = 0;
    };

    vector<pair<SnapshotGameResult,string>> allResults;
    vector<GameResultInfo> groups;

    bool sameResult(const SnapshotGameResult& a, const SnapshotGameResult& b) const;
    void makeGroups(vector<pair<SnapshotGameResult,string>>& results);

    void writeOutput(const string& mapPath,
                     const string& algorithmSoPath1,
                     const string& algorithmSoPath2,
                     const string& gmFolder);
    string BuildOutputBuffer(const string& mapPath,
                     const string& algorithmSoPath1,
                     const string& algorithmSoPath2);

    static void printSatellite(std::ostream& os, const SnapshotGameResult& result);

    static string getFilename(const string& path) {
        return std::filesystem::path(path).filename().string();
    }
};