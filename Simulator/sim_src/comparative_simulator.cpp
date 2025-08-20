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
        if (!algo_registrar) {
            throw std::runtime_error("Error: Failed to get AlgorithmRegistrar instance.");
        }

        game_manager_registrar = &GameManagerRegistrar::getGameManagerRegistrar();
        if (!game_manager_registrar) {
            throw std::runtime_error("Error: Failed to get GameManagerRegistrar instance.");
        }
}


ComparativeSimulator::~ComparativeSimulator() {
    
    // Ensure algorithm objects are destroyed before unloading .so files
    allResults.clear(); // if GameResult keeps algo-allocated objects
    groups.clear(); // clear the groups vector
    algo1_.reset();
    algo2_.reset();

    if (game_manager_registrar) {
        game_manager_registrar->clear();
    }
    if (algo_registrar) {
        algo_registrar->clear();
    }

    lock_guard<mutex> lock(handlesMutex_);
    for (auto& handle : algoHandles_) {
        if (handle) {
            dlclose(handle);
        }
    }
    algoHandles_.clear();
}

/**
 * @brief Runs a comparative simulation between multiple game managers.
 *
 * This function loads the map, dynamically loads the algorithm shared objects,
 * verifies the required factories, initializes the game managers, and runs the
 * simulation. The results are then written to an output file.
 *
 * @param mapPath Path to the input map file.
 * @param gmFolder Path to the folder containing GameManager shared libraries.
 * @param algorithmSoPath1 Path to the first algorithm `.so` file.
 * @param algorithmSoPath2 Path to the second algorithm `.so` file.
 * @return int 0 if the simulation runs successfully, non-zero error code otherwise.
 */
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
    algo1_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->begin()));
    algo2_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->end() - 1));
    
    
    if (!algo1_->hasPlayerFactory() || !algo2_->hasPlayerFactory() || 
        !algo1_->hasTankAlgorithmFactory() || !algo2_->hasTankAlgorithmFactory()) {
        std::cerr << "Error: Missing player or tank algorithm factory for one of the algorithms." << std::endl;
        return 1;
    }

    getGameManagers(gmFolder);
    if (gms_paths_.empty()) {
        std::cerr << "Error: No GameManager shared libraries found in folder: " << gmFolder << std::endl;
        return 1;
    }
    runGames();
    writeOutput(mapPath, algorithmSoPath1, algorithmSoPath2, gmFolder);

    return 0;
}

/**
 * @brief Dynamically loads an Algorithm shared object (.so) file.
 *
 * This function registers a new Algorithm object from the given shared object,
 * attempts to load it using `dlopen`, and stores its handle for later use.
 *
 * @param path Path to the algorithm `.so` file.
 * @return true if the shared object was successfully loaded, false otherwise.
 */
bool ComparativeSimulator::loadAlgoSO(const string& path) {
    auto absPath = std::filesystem::absolute(path);

    if (!algo_registrar) {
        std::cerr << "Error: Algorithm registrar is null\n";
        return false;
    }
    algo_registrar->createAlgorithmFactoryEntry(path);
    
    void* handle = dlopen(absPath.c_str(), RTLD_LAZY);
    if (!handle) {
        if (algo_registrar) { algo_registrar->removeLast(); } //Rollback
        const char* error = dlerror();
        std::cerr << "Failed loading .so file from path: " << path << "\n";
        std::cerr << (error ? error : "Unknown error") << "\n";
        return false;
    }
    algoHandles_.push_back(handle);
    return true;
}

/**
 * @brief Dynamically loads a GameManager shared object (.so) file.
 *
 * This function registers a new GameManager object from the given shared object,
 * attempts to load it using `dlopen`, and returns its handle. 
 * If the load fails, an error message is printed to `stderr`.
 *
 * @param path Path to the GameManager `.so` file.
 * @return A valid handle to the loaded shared object on success, or nullptr on failure.
 */
void* ComparativeSimulator::loadGameManagerSO(const string& path) {
    lock_guard<mutex> lock(gmRegistrarmutex_);
    auto absPath = std::filesystem::absolute(path);

    if (!game_manager_registrar) {
        lock_guard<mutex> lock(stderrMutex_);
        std::cerr << "Error: Game manager registrar is null\n";
        return nullptr;
    }
    game_manager_registrar->createEntry(path);

    void* handle = dlopen(absPath.c_str(), RTLD_LAZY);
    if (!handle) {
        game_manager_registrar->removeLast(); // Rollback the last entry
        lock_guard<mutex> lock(stderrMutex_);
        const char* error = dlerror();
        std::cerr << "Failed loading .so file from path: " << path << "\n";
        std::cerr << (error ? error : "Unknown error") << "\n";
        return nullptr;
    }
    return handle;
}


void ComparativeSimulator::getGameManagers(const string& gameManagerFolder) {
    for (const auto& entry : directory_iterator(gameManagerFolder)) { // Check each entry in the directory
        if (entry.path().extension() == ".so") { // We care only about .so files
            gms_paths_.push_back(entry.path()); // Store the path of the GameManager .so file
        }
    }
}

/**
 * @brief Executes all scheduled games using available GameManager shared objects.
 *
 * This function distributes the execution of games across multiple threads.
 * - If only one thread is available, all games are executed sequentially on the main thread.
 * - If multiple threads are available, a thread pool is created and each worker
 *   retrieves a GameManager path from the shared list and runs the corresponding game.
 *
 * @note The number of threads is limited by `numThreads_` and the number of available
 *       GameManager paths (`gms_paths_`).
 */
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

    // Wait for all threads to finish working
    for (auto& t : workers) {
        t.join();
    }
}

/**
 * @brief Executes a single game using a specified GameManager shared object.
 *
 * This function dynamically loads a GameManager from the given `.so` file, 
 * retrieves its factory, and initializes a new GameManager instance. It then 
 * creates two players and their corresponding tank algorithm factories from the 
 * registered algorithms. The game is executed with these players, and the 
 * result is stored in `allResults`.
 *
 * After execution, the GameManager entry is removed from the registrar and the
 * shared object handle is closed.
 *
 * @param gmPath Path to the GameManager `.so` file to load and execute.
 */
void ComparativeSimulator::runSingleGame(const path& gmPath) {
    // load .so file for Game Manager
    void* gm_handle = loadGameManagerSO(gmPath);
    if (!gm_handle) {
        return;
    }
    {
        unique_ptr<AbstractGameManager> gameManager;
        string gm_name;
        bool createdGameManager = false, createdFactory = false;
        {
        // Get the GameManager factory and name from the registrar
        lock_guard<mutex> lock(gmRegistrarmutex_);
        if (!game_manager_registrar || game_manager_registrar->empty()) {
            std::cerr << "Error: Registrar is empty\n";
            dlclose(gm_handle);
            return; 

        }
        auto gm = (game_manager_registrar->end()[-1]); // Get the last registered GameManager
        createdFactory = gm.hasFactory();
        

        // Create the GameManager instance
        gameManager = gm.create(verbose_);
        createdGameManager = (gameManager != nullptr);
        

        gm_name = gm.name();
        }

        if (!createdFactory){
            lock_guard<mutex> lock(stderrMutex_);
            std::cerr << "Error: GameManager factory not found for: " << gm_name << std::endl;
            dlclose(gm_handle); // Close the GameManager shared object handle
            return;
        }
        if (!createdGameManager) {
            lock_guard<mutex> lock(stderrMutex_);
            std::cerr << "Error: Failed to init Game manager from path: " << gmPath << std::endl;
            dlclose(gm_handle); // Close the GameManager shared object handle
            return;
        }

        // Create players using the algorithm factories
        unique_ptr<Player> player1 = algo1_->createPlayer(0, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
        unique_ptr<Player> player2 = algo2_->createPlayer(1, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
        if (!player1 || !player2) {
            lock_guard<mutex> lock(stderrMutex_);
            std::cerr << "Error: Failed to create players from algorithms." << std::endl;
            dlclose(gm_handle); // Close the GameManager shared object handle
            return;
        }

        // Get algorithm names and factories
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

        // Store the result in allResults
        {
        lock_guard<mutex> lock(allResultsMutex_);
        SnapshotGameResult snap = makeSnapshot(result, mapData_.rows, mapData_.cols);
        if (snap.board.empty()) {
            lock_guard<mutex> lock(stderrMutex_);
            std::cerr << "Error: Empty board in GameResult for GameManager: " << gm_name << std::endl;
            dlclose(gm_handle); // Close the GameManager shared object handle
            return;
        } 
        allResults.emplace_back(snap, gm_name);
        }
    
        lock_guard<mutex> lock(gmRegistrarmutex_);
        game_manager_registrar->eraseByName(gm_name); // Remove the GameManager entry by name
    }
    

    // Remove the GameManager entry from the registrar and close the handle
    dlclose(gm_handle); // Close the GameManager shared object handle
}

/**
 * @brief Compares two GameResult objects to check if they are identical.
 *
 * Two results are considered the same if they have identical winners, reasons,
 * rounds, and identical final map states (compared cell by cell).
 *
 * @param a First GameResult to compare.
 * @param b Second GameResult to compare.
 * @return true if both results are identical, false otherwise.
 */
bool ComparativeSimulator::sameResult(const SnapshotGameResult& a, const SnapshotGameResult& b) const {
    // Check if the winners, reasons, and rounds are the same
    if (a.winner != b.winner || a.reason != b.reason || a.rounds != b.rounds) {
        return false;
    }
    // Check if the end map is the same
    if (a.board.size() != b.board.size()) return false;
    for (size_t y = 0; y < a.board.size(); ++y) {
        if (a.board[y] != b.board[y]) return false;
    }
    return true;
}

/**
 * @brief Groups game results that are equivalent.
 *
 * Iterates over the results and clusters them into groups.
 *
 * @param results Vector of game results paired with their GameManager names.
 */
void ComparativeSimulator::makeGroups(vector<pair<SnapshotGameResult, string>>& results) {
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

/**
 * @brief Writes the comparative simulation results to an output file.
 *
 * Groups the results, sorts them by frequency, builds an output buffer, and
 * writes it to a file in the given folder. If the file cannot be opened,
 * the output buffer is printed to stdout instead.
 *
 * @param mapPath Path to the input map file.
 * @param algorithmSoPath1 Path to the first algorithm `.so` file.
 * @param algorithmSoPath2 Path to the second algorithm `.so` file.
 * @param gmFolder Output folder where results will be saved.
 */
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
        cerr << "Error: failed to open output file." << endl;
        printf("%s\n", outputBuffer.c_str());
        return;
    }
    // Else, write the output buffer to the file
    outFile << outputBuffer;
    outFile.close();
}


void ComparativeSimulator::printSatellite(std::ostream& os,
                           const SnapshotGameResult& result) {
    for (size_t y = 0; y < result.board.size(); ++y) {
        for (size_t x = 0; x < result.board[y].size(); ++x) {
            char cell = result.board[y][x];
            if (cell == '$') cell = '#'; // $ is an internaly used char
            os << cell;
        }
        os << "\n";
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
        auto group = groups.back();
        groups.pop_back();

        if (!group.gm_names.empty()) {
            for (size_t i = 0; i + 1 < group.gm_names.size(); ++i) {
                oss << group.gm_names[i] << ", ";
            }
            oss << group.gm_names.back() << "\n";
        } else {
            oss << "\n"; // defensive: keep shape even if no names
        }

        oss << (
            group.result.winner == 0
            ? (group.result.reason == GameResult::ALL_TANKS_DEAD
                ? "Tie, both players have zero tanks"
                : group.result.reason == GameResult::MAX_STEPS
                    ? "Tie, reached max steps = " + std::to_string(group.result.rounds) +
                      ", player 1 has " + std::to_string(group.result.remaining_tanks[0]) +
                      " tanks, player 2 has " + std::to_string(group.result.remaining_tanks[1]) + " tanks"
                    : "Tie, both players have zero shells for 40 steps") // replace 40 with your constant if you have one
            : "Player " + std::to_string(group.result.winner) + " won with " +
              std::to_string(group.result.remaining_tanks[group.result.winner - 1]) + " tanks still alive"
        ) << "\n";

        oss << group.result.rounds << "\n";

        printSatellite(oss, group.result);

        if (!groups.empty()) {
            oss << "\n";
        }
    }

    return oss.str();
}



