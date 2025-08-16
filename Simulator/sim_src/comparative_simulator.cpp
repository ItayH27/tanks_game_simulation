#include "../sim_include/comparative_simulator.h"
#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <memory>
#include <utility>
#include <condition_variable>
#include <atomic>

#include "GameManagerRegistrar.h"

using std::mutex, std::lock_guard, std::thread, std::filesystem::directory_iterator, std::ofstream, std::ostringstream, std::sort;

ComparativeSimulator::ComparativeSimulator(bool verbose, size_t numThreads)
    : Simulator(verbose, numThreads) {
        algo_registrar = &AlgorithmRegistrar::getAlgorithmRegistrar();
        game_manager_registrar = &GameManagerRegistrar::getGameManagerRegistrar();
}


ComparativeSimulator::~ComparativeSimulator() {
    lock_guard<mutex> lock(handlesMutex_);
    for (auto& handle : algoHandles_) {
        if (handle) {
            dlclose(handle);
        }
    }
    algoHandles_.clear();
}


int ComparativeSimulator::run(const string& mapPath,
                              const string& gmFolder,
                              const string& algorithmSoPath1,
                              const string& algorithmSoPath2) {
    
    mapData_ = readMap(mapPath);
    if (mapData_.failedInit) {
        std::cerr << "Error: failed to load the map data." << std::endl;
        return 1;
    }

    if (!loadAlgoSO(algorithmSoPath1) || !loadAlgoSO(algorithmSoPath2)) {
        std::cerr << "Error: failed to load one or both algorithms." << std::endl;
        return 1;
    }
    algo1_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->end() - 1));
    algo2_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->begin()));
    
    if (!algo1_->hasPlayerFactory() || !algo2_->hasPlayerFactory() || 
        !algo1_->hasTankAlgorithmFactory() || !algo2_->hasTankAlgorithmFactory()) {
        std::cerr << "Error: Missing player or tank algorithm factory for one of the algorithms." << std::endl;
        return 1;
    }

    getGameManagers(gmFolder);
    // loadGameManagers(gms_paths_);

    runGames();
    writeOutput(mapPath, algorithmSoPath1, algorithmSoPath2, gmFolder);

    return 0;
}


bool ComparativeSimulator::loadAlgoSO(const string& path) {
    algo_registrar->createAlgorithmFactoryEntry(path);
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        const char* error = dlerror();
        std::cerr << "Failed loading .so file from path: " << path << "\n";
        std::cerr << (error ? error : "Unknown error") << "\n";
        return false;
    }
    algoHandles_.push_back(handle);
    return true;
}

void* ComparativeSimulator::loadGameManagerSO(const string& path) {
    game_manager_registrar->createEntry(path);
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        const char* error = dlerror();
        std::cerr << "Failed loading .so file from path: " << path << "\n";
        std::cerr << (error ? error : "Unknown error") << "\n";
        return nullptr;
    }
    return handle;
}


void ComparativeSimulator::getGameManagers(const string& gameManagerFolder) {
    for (const auto& entry : directory_iterator(gameManagerFolder)) {
        if (entry.path().extension() == ".so") { // We care only about .so files
            gms_paths_.push_back(entry.path());
        }
    }
}


void ComparativeSimulator::runGames() {
    size_t threadCount = std::min(numThreads_, gms_paths_.size());
    if (threadCount == 1) { // Main thread runs all games sequentially
        for (const auto& task : gms_paths_) {
            runSingleGame(task); // Run all games sequentially if only one thread
        }
        return;
    }

    mutex gms_mutex_;
    std::atomic<size_t> nextGameManagers{0};
    vector<thread> workers;

    // Worker workflow
    auto worker = [&]() {
        while (true) {
            path gmPath;
            size_t idx = nextGameManagers.fetch_add(1, std::memory_order_relaxed);
            // Make sure each game is safely grabbed by a single thread
            if (idx >= gms_paths_.size()) return;
            gmPath = gms_paths_[idx];
            runSingleGame(gmPath); // Runs the scheduled game
        }
    };

    // Create worker pool
    for (size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back(worker);
    }

    for (auto& t : workers) {
        t.join();
    }
}


void ComparativeSimulator::runSingleGame(const path& gmPath) {
    // load .so file for Game Manager
    void* gm_handle = loadGameManagerSO(gmPath);
    if (!gm_handle) {
        return;
    }
    {
        // Get the GameManager factory and name from the registrar
        auto gm = (game_manager_registrar->end()[-1]); // Get the last registered GameManager
        if (!gm.hasFactory()){
            std::cerr << "Error: GameManager factory not found for: " << gm.name() << std::endl;
            dlclose(gm_handle); // Close the GameManager shared object handle
            return;
        }

        // game_manager_registrar->removeLast();
        unique_ptr<AbstractGameManager> gameManager = gm.create(verbose_);
        if (!gameManager) {
            std::cerr << "Error: Failed to init Game manager from path: " << gmPath << std::endl;
            return;
        }
        auto gm_name = gm.name();

        unique_ptr<Player> player1 = algo1_->createPlayer(0, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
        unique_ptr<Player> player2 = algo1_->createPlayer(1, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
        if (!player1 || !player2) {
            std::cerr << "Error: Failed to create players from algorithms." << std::endl;
            return;
        }

        string name1 = algo1_->name();
        string name2 = algo2_->name();

        TankAlgorithmFactory tankAlgorithmFactory1 = algo1_->getTankAlgorithmFactory();
        TankAlgorithmFactory tankAlgorithmFactory2 = algo2_->getTankAlgorithmFactory();

        // Run game manager with players and factories
        
        GameResult result = gameManager->run(
            mapData_.cols, mapData_.rows,
            *mapData_.satelliteView, mapData_.name,
            mapData_.maxSteps, mapData_.numShells,
            *player1, name1, *player2, name2,
            tankAlgorithmFactory1, tankAlgorithmFactory2);

        allResults.emplace_back(std::move(result), gm_name);
    }

    game_manager_registrar->removeLast();
    dlclose(gm_handle); // Close the GameManager shared object handle
}


bool ComparativeSimulator::sameResult(const GameResult& a, const GameResult& b) const {
    // Check if the winners, reasons, and rounds are the same
    if (a.winner != b.winner || a.reason != b.reason || a.rounds != b.rounds) {
        return false;
    }
    // Check if the end map is the same
    const SatelliteView& viewA = *a.gameState;
    const SatelliteView& viewB = *b.gameState;

    for (int y = 0; y < mapData_.rows; y++) {
        for (int x = 0; x < mapData_.cols; x++) {
            if (viewA.getObjectAt(x,y) != viewB.getObjectAt(x,y)) {
                return false;
            }
        }
    }
    return true;
}


void ComparativeSimulator::makeGroups(vector<pair<GameResult, string>>& results) {
    for (auto& result : results) {
        bool placed = false;
        for (auto& group : groups) {
            if (sameResult(result.first, group.result)) {
                group.gm_names.push_back(result.second);
                group.count += 1;
                placed = true;
                break;
            }
        }
        if (!placed) {
            groups.push_back({ std::move(result.first), { result.second }, 1 });
        }
    }
}


void ComparativeSimulator::writeOutput(const string& mapPath,
                     const string& algorithmSoPath1,
                     const string& algorithmSoPath2,
                     const string& gmFolder) {
    // Make the groups from all results
    makeGroups(allResults);

    // Sort the groups by count in ascending order
    std::sort(groups.begin(), groups.end(),
            [](const GameResultInfo& a, const GameResultInfo& b) {
            return a.count < b.count; // Sort by count in ascending order
        });

    // Build the output buffer
    string outputBuffer = BuildOutputBuffer(mapPath, algorithmSoPath1, algorithmSoPath2);

    // Create the output file with a timestamp
    string time = timestamp();
    ofstream outFile(gmFolder + "/comparative_results_" + time +".txt");

    // If file didn't open, print error and the output buffer
    if (!outFile) {
        printf("Error: failed to open output file.\n");
        printf("%s\n", outputBuffer.c_str());
        return;
    }
    // Else, write the output buffer to the file
    outFile << outputBuffer;
    outFile.close();
}


static void printSatellite(std::ostream& os,
                           const SatelliteView& view,
                           size_t width, size_t height) {
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width;  ++x) {
            os << view.getObjectAt(x, y);
        }
        os << '\n';
    }
}


string ComparativeSimulator::BuildOutputBuffer(const string& mapPath,
                                                 const string& algorithmSoPath1,
                                                 const string& algorithmSoPath2) {
    ostringstream oss;
    oss << "game_map=" << getFilename(mapPath) << "\n"
        << "algorithm1=" << getFilename(algorithmSoPath1) << "\n"
        << "algorithm2=" << getFilename(algorithmSoPath2) << "\n"
        << "\n";

    while (!groups.empty()) {
        auto group = move(groups.back());
        groups.pop_back();

        for (size_t i = 0; i < group.gm_names.size()-1 ; i++) {
            oss << group.gm_names[i] << ", ";
        }
        oss << group.gm_names[group.gm_names.size() - 1] << "\n"
            << "Winner: " << group.result.winner << ", Reason: " << group.result.reason << "\n"
            << group.result.rounds << "\n"
            << endl;

        printSatellite(oss, *group.result.gameState, mapData_.cols, mapData_.rows);
    }



    return oss.str();
}

