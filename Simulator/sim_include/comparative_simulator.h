#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <mutex>  // Required for std::mutex
#include "AlgorithmRegistrar.h"
#include "GameManagerRegistration.h"
#include "GameResult.h"
#include "AbstractGameManager.h"

using std::string, std::filesystem::path, std::shared_ptr, std::vector, std::unordered_map, std::mutex;



class ComparativeSimulator {
public:
    ComparativeSimulator(bool verbose, size_t numThreads);
    ~ComparativeSimulator();

    int run(const string& mapPath,
            const string& algorithmSoPath1,
            const string& algorithmSoPath2);
private:

    bool verbose_;
    size_t numThreads_;
    void* algorithmHandle1_ = nullptr;
    void* algorithmHandle2_ = nullptr;

    AlgorithmRegistrar::AlgorithmAndPlayerFactories algorithmFactory1_;
    AlgorithmRegistrar::AlgorithmAndPlayerFactories algorithmFactory2_;

    unordered_map<GameResult, vector<string>> gameResToGameManagers_;
    vector<path> gms_paths_;


    bool loadMap(const string& mapPath);
    bool loadAlgorithm(const string& algorithmSoPath,
                        AlgorithmRegistrar::AlgorithmAndPlayerFactories& algoFactory,
                        void*& algoHandle);
    void getGameManagers(const string& gameManagerFolder);
    bool loadGameManager(const string& gameManagerSoPath,
                         GameManagerFactory& gameManagerFactory);
    void runGames();
    void runSingleGame(const path& gmPath);


    void WriteOutput();
    string timestamp();





};