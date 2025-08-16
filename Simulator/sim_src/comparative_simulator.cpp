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

#include "GameManagerRegistrar.h"

using std::mutex, std::lock_guard, std::thread, std::filesystem::directory_iterator, std::ofstream, std::ostringstream, std::ranges::sort;

ComparativeSimulator::ComparativeSimulator(bool verbose, size_t numThreads)
    : Simulator(verbose, numThreads) {
        algo_registrar = &AlgorithmRegistrar::getAlgorithmRegistrar();

}

ComparativeSimulator::~ComparativeSimulator() {
    for (auto& handle : handles_) {
        if (handle) {
            dlclose(handle);
        }
    }
    handles_.clear();
}

int ComparativeSimulator::run(const string& mapPath,
                              const string& algorithmSoPath1,
                              const string& algorithmSoPath2,
                              const string& gmFolder) {
    game_manager_registrar = GameManagerRegistrar::getGameManagerRegistrar();

    mapData_ = readMap(mapPath);
    if (!mapData_.failedInit) {
        std::cerr << "Error: failed to load the map data." << std::endl;
        return 1;
    }

    if (!loadSO(algorithmSoPath1) || !loadSO(algorithmSoPath2)) {
        std::cerr << "Error: failed to load one or both algorithms." << std::endl;
        return 1;
    }

    getGameManagers(gmFolder);
    // loadGameManagers(gms_paths_);

    runGames();
    writeOutput(mapPath, algorithmSoPath1, algorithmSoPath2, gmFolder);

    return 0;
}

// int ComparativeSimulator::run(const string& mapPath,
//             const string& algorithmSoPath1,
//             const string& algorithmSoPath2){
//
//     //TODO: Read map file and extract game data (num_shells, etc...) and create satellite view
//     //TODO: Load all the data about the map to the struct MapData
//
//     // Load algorithms using folder path
//     if (!loadAlgorithm(algorithmSoPath1, algorithmFactory1_, algorithmHandle1_)) { // Make sure there are least 2 algorithms
//         std::cerr << "Error: failed to load the first algorithm." << std::endl;
//         return 1;
//     }
//
//     if (!loadAlgorithm(algorithmSoPath2, algorithmFactory2_, algorithmHandle2_)) { // Make sure there are least 2 algorithms
//         std::cerr << "Error: failed to load the second algorithm." << std::endl;
//         return 1;
//     }
//
//     runGames(); // Run all games using threads
//
//     return 0;
// }

bool ComparativeSimulator::loadSO(const string& path) {
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        const char* error = dlerror();
        std::cerr << "Failed loading .so file from path: " << path << "\n";
        std::cerr << (error ? error : "Unknown error") << "\n";
        return false;
    }
    handles_.push_back(handle);
    return true;
}

// bool ComparativeSimulator::loadAlgorithm(const string& algorithmSoPath,
//                         AlgorithmRegistrar::AlgorithmAndPlayerFactories& algoFactory,
//                         void*& algoHandle){
//     algoHandle = dlopen(algorithmSoPath.c_str(), RTLD_LAZY);
//     if (!algoHandle) { // Failed to dlopen algorithm
//         std::cerr << "dlopen failed: " << dlerror() << std::endl;
//         return false;
//     }
//
//     // Get factory ptr from loaded algorithm
//     void* factoryPtr = dlsym(algoHandle, "_Z20algorithm_factoryb");
//     const char* error = dlerror();
//     if (error || !factoryPtr) {
//         std::cerr << "dlsym failed to find Algorithm factory: " << (error ? error : "null") << std::endl;
//         return false;
//     }
//
//     // Recast into known type
//     algoFactory = *reinterpret_cast<AlgorithmRegistrar::AlgorithmAndPlayerFactories*>(factoryPtr);
//     return true;
// }

void ComparativeSimulator::getGameManagers(const string& gameManagerFolder) {
    for (const auto& entry : directory_iterator(gameManagerFolder)) {
        if (entry.path().extension() == ".so") { // We care only about .so files
            gms_paths_.push_back(entry.path());
        }
    }
}

// bool ComparativeSimulator::loadGameManagers(const vector<path>& gms_Paths) {
//     for (const auto& gmPath : gms_Paths) {
//         if (!loadSO(gmPath.string())) {
//             return false;
//         }
//     }
//     return true;
// }

// bool ComparativeSimulator::loadGameManager(const string& gameManagerSoPath, GameManagerFactory& gameManagerFactory) {
//     auto& registrar = GameManagerRegistrar::getGameManagerRegistrar();
//
//     // void* handle = dlopen(gameManagerSoPath.c_str(), RTLD_LAZY);
//     // if (!handle) {
//     //     std::cerr << "dlopen failed: " << dlerror() << std::endl;
//     //     return false;
//     // }
//     //
//     // // Get factory ptr from loaded game manger
//     // void* factoryPtr = dlsym(handle, "_Z20game_manager_factoryb");
//     // const char* error = dlerror();
//     // if (error || !factoryPtr) {
//     //     std::cerr << "dlsym failed to find GameManager factory: " << (error ? error : "null") << std::endl;
//     //     return false;
//     // }
//     //
//     // // Recast into known type
//     // gameManagerFactory = *reinterpret_cast<GameManagerFactory*>(factoryPtr);
//     // return true;
// }

void ComparativeSimulator::runGames() {
    mutex gms_mutex_;
    size_t nextGameManagers = 0;
    vector<thread> workers;

    // Worker workflow
    auto worker = [&]() {
        while (true) {
            path gmPath;
            {
                // Make sure each game is safely grabbed by a single thread
                lock_guard<mutex> lock(gms_mutex_);
                if (nextGameManagers >= gms_paths_.size()) return;
                gmPath = gms_paths_[nextGameManagers++];
            }
            runSingleGame(gmPath); // Runs the scheduled game
        }
    };

    // Create worker pool
    size_t threadCount = std::min(numThreads_, gms_paths_.size());
    for (size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back(worker);
    }

    for (auto& t : workers) {
        t.join();
    }
}

void ComparativeSimulator::runSingleGame(const path& gmPath) {
    // load .so file for Game Manager
    loadSO(gmPath);

    // Get the GameManager factory and name from the registrar
    auto gm = game_manager_registrar.end();
    game_manager_registrar.removeLast();
    unique_ptr<AbstractGameManager> gameManager = gm->create(verbose_);
    if (!gameManager) {
        std::cerr << "Error: Failed to init Game manager from path: " << gmPath << std::endl;
        return;
    }
    auto gm_name = gm->name();

    // Create players using the AlgorithmRegistrar
    auto algo1 = algo_registrar->end();
    auto algo2 = algo_registrar->begin();

    unique_ptr<Player> player1 = algo1->createPlayer(0, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
    unique_ptr<Player> player2 = algo2->createPlayer(1, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
    if (!player1 || !player2) {
        std::cerr << "Error: Failed to create players from algorithms." << std::endl;
        return;
    }

    TankAlgorithmFactory tankFactory1 = algo1->getTankAlgorithmFactory();
    TankAlgorithmFactory tankFactory2 = algo2->getTankAlgorithmFactory();

    // Run game manager with players and factories
    GameResult result = gameManager->run(
        mapData_.cols, mapData_.rows,
        *mapData_.satelliteView, mapData_.name,
        mapData_.maxSteps, mapData_.numShells,
        *player1, algo1->name(), *player2, algo2->name(),
        tankFactory1, tankFactory2);

    allResults.emplace_back(std::move(result), gm_name);

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
    sort(groups,
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



// void ComparativeSimulator::runSingleGame(const path& gmPath) {
//     GameManagerFactory gameManagerHandle;
//     if (!loadGameManager(gmPath, gameManagerHandle)) {
//         std::cerr << "Error: Failed to load Game manager." << std::endl;
//         return;
//     }
//
//     auto gameManager = gameManagerHandle(verbose_);
//     if (!gameManager) {
//         std::cerr << "Error: Failed to init Game manager." << std::endl;
//         return;
//     }
//     //TODO: game manager name
//     auto gm_name = gameManager.
//
//     auto name1 = algorithmFactory1_.name();
//     auto name2 = algorithmFactory2_.name();
//
//     //TODO: argument sfor createPlayer should be read from MapData:
//     //TODO: int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells
//     auto player1 = algorithmFactory1_.createPlayer();
//     auto player2 = algorithmFactory2_.createPlayer();
//
//     // Run game manager with players and factories
//     GameResult result = gameManager->run(
//         map_width, map_height,
//         *mapView, map_name,
//         max_steps, num_shells,
//         *player1, name1, *player2, name2,
//         algo1->getTankAlgorithmFactory(),
//         algo2->getTankAlgorithmFactory()
//     );
//
//     //TODO: map the game manager according to its results in gameResToGameManagers_
//
// }