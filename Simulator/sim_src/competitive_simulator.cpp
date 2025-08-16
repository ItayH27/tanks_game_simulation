#include "../sim_include/competitive_simulator.h"
#include <dlfcn.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <condition_variable>
#include "GameManagerRegistrar.h"
#include "../sim_include/Simulator.h"
#include "../sim_include/AlgorithmRegistrar.h"


CompetitiveSimulator::CompetitiveSimulator(bool verbose, size_t numThreads)
    : Simulator(verbose, numThreads) {
        algoRegistrar_ = &AlgorithmRegistrar::getAlgorithmRegistrar();
    }

CompetitiveSimulator::~CompetitiveSimulator() {
    lock_guard<mutex> lock(handlesMutex_);
    for (auto& [path, handle] : algoPathToHandle_) {
        if (handle) {
            dlclose(handle);
        }
    }
    algoPathToHandle_.clear();

    if (gameManagerHandle_) {
        dlclose(gameManagerHandle_);
        gameManagerHandle_ = nullptr;
    }
}

/**
 * @brief Runs the competitive simulation using the provided folders and GameManager.
 *
 * Loads the GameManager and all algorithm `.so` files, loads map files, schedules and runs games
 * according to the assignment's round-robin pairing scheme, and writes the results to an output file.
 *
 * @param mapsFolder Path to the folder containing game map files.
 * @param gameManagerSoPath Path to the compiled GameManager `.so` file.
 * @param algorithmsFolder Path to the folder containing compiled algorithm `.so` files.
 * @return int 0 on success, 1 on error.
 */
int CompetitiveSimulator::run(const string& mapsFolder,
                              const string& gameManagerSoPath,
                              const string& algorithmsFolder) {

    // Load gamemanager using .so path
    if (!loadGameManager(gameManagerSoPath)) {
        return 1;
    }

    // Load algorithms using folder path
    if (!getAlgorithms(algorithmsFolder)) { // Make sure there are least 2 algorithms
        cerr << "Usage: At least two algorithms must be present in folder: " << algorithmsFolder << endl;
        return 1;
    }

    // Load maps into vector
    vector<fs::path> maps;
    if (!loadMaps(mapsFolder, maps)) {
        cerr << "Usage Error: No valid map files found in folder: " << mapsFolder << "\n"
          << "Make sure the folder exists and contains at least one valid map file." << endl;
        return 1;
    }

    scheduleGames(maps); // Scheduled all games to run
    runGames(); // Run all games using threads
    writeOutput(algorithmsFolder, mapsFolder, gameManagerSoPath); // Write the require output

    return 0;
}

/**
 * @brief Dynamically loads the GameManager shared library.
 *
 * @param soPath Path to the GameManager `.so` file.
 * @return true if successfully loaded, false otherwise.
 */
bool CompetitiveSimulator::loadGameManager(const string& soPath) {
    auto absPath = fs::absolute(soPath);
    string soName = absPath.stem().string();

    auto& registrar = GameManagerRegistrar::getGameManagerRegistrar();
    registrar.createEntry(soName);

    gameManagerHandle_ = dlopen(absPath.c_str(), RTLD_LAZY | RTLD_NODELETE);
    if (!gameManagerHandle_) {
        std::cerr << "dlopen failed: " << absPath << ": " << dlerror() << std::endl;
        registrar.removeLast();
        return false;
    }

    try {
    registrar.validateLast(); // REGISTER_GAME_MANAGER ran and set a factory

    // Build a callable that looks up the entry by name and calls create(verbose)
    gameManagerFactory_ = [soName, &registrar](bool verbose) -> std::unique_ptr<AbstractGameManager> {
        for (auto it = registrar.begin(); it != registrar.end(); ++it) {
            if (it->name() == soName) {
                return it->create(verbose);
            }
        }
        throw std::runtime_error("GameManager not registered: " + soName);
    };

} catch (const std::exception& e) {
    std::cerr << "Validation failed: " << e.what() << std::endl;
    registrar.removeLast();
    dlclose(gameManagerHandle_);
    gameManagerHandle_ = nullptr;
    return false;
}

    return true;
}

/**
 * @brief Loads all algorithm shared libraries in the given folder and registers their factories.
 *
 * Validates each algorithm's registration and manages their reference counts and handle lifetimes.
 *
 * @param folder Path to the folder containing algorithm `.so` files.
 * @return true if at least one algorithm was successfully loaded, false otherwise.
 */
bool CompetitiveSimulator::getAlgorithms(const string& folder) {
    size_t soFound = 0;

    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.path().extension() == ".so") {
            const string soPath = entry.path().string();
            const string name = entry.path().stem().string();
            
            algoNameToPath_[name] = soPath;
            algoUsageCounts_[name] = 0;

            ++soFound;
        }
    }

    return soFound >= 2; // At least two algorithms are required
}

/**
 * @brief Loads all regular files from the given folder as candidate game maps.
 *
 * @param folder Path to the folder containing map files.
 * @param outMaps Output vector where the discovered map paths will be stored.
 * @return true if any map files were found, false otherwise.
 */
bool CompetitiveSimulator::loadMaps(const string& folder, vector<fs::path>& outMaps) {
    // Iterate over all maps in folder and add them to maps vector
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.is_regular_file()) {
            outMaps.push_back(entry.path());
        }
    }
    return !outMaps.empty();
}

/**
 * @brief Schedules the list of games to be played between algorithms on all maps.
 *
 * Implements the round-robin pairing scheme defined in the assignment for competitive mode.
 *
 * @param maps List of map file paths to use in scheduling games.
 */
void CompetitiveSimulator::scheduleGames(const std::vector<fs::path>& maps) {
    std::vector<std::string> algoNames;
    algoNames.reserve(algoNameToPath_.size());
    for (const auto& [name, _] : algoNameToPath_) algoNames.push_back(name);

    const size_t N = algoNames.size();
    const size_t R = N - 1;

    for (size_t k = 0; k < maps.size(); ++k) {
        const size_t r = k % R;
        const bool evenN_middle_round = (N % 2 == 0) && (r == N/2 - 1);

        for (size_t i = 0; i < N; ++i) {
            size_t j = (i + 1 + r) % N;
            if (i >= j) continue; // avoid duplicate unordered pairs

            // Always schedule i vs j
            scheduledGames_.push_back({maps[k], algoNames[i], algoNames[j]});
            ++algoUsageCounts_[algoNames[i]];
            ++algoUsageCounts_[algoNames[j]];

            // Add mirror game unless it's the symmetric round
            if (!evenN_middle_round) {
                scheduledGames_.push_back({maps[k], algoNames[j], algoNames[i]});
                ++algoUsageCounts_[algoNames[i]];
                ++algoUsageCounts_[algoNames[j]];
            }
        }
    }
}

/**
 * @brief Ensures the specified algorithm's shared library is loaded and registered.
 *
 * If the algorithm is not already loaded, this function dynamically loads its `.so` file
 * using dlopen, creates its registration entry in the AlgorithmRegistrar, validates the
 * registration, and caches the result. If the algorithm has already been loaded,
 * the function returns immediately.
 *
 * On registration failure or dlopen error, the function throws a runtime exception
 * and cleans up any partial state.
 *
 * Thread-safe via locking.
 *
 * @param name The algorithm's unique name (derived from the `.so` filename).
 */
void CompetitiveSimulator::ensureAlgorithmLoaded(const string& name) {
    lock_guard<mutex> lock(handlesMutex_);

    auto it = algoNameToPath_.find(name);
    if (it == algoNameToPath_.end()) {
        throw std::runtime_error("Unknown algorithm: " + name);
    }
    const std::string& soPath = it->second;
    if (algoPathToHandle_.count(soPath)) return;

    algoRegistrar_->createAlgorithmFactoryEntry(name);

    void* handle = dlopen(soPath.c_str(), RTLD_LAZY | RTLD_NODELETE);
    if (!handle) {
        algoRegistrar_->removeLast();
        std::lock_guard<std::mutex> errLock(stderrMutex_);
        throw std::runtime_error(std::string("Failed to load algorithm ") + soPath + ": " + dlerror());
    }

    algoPathToHandle_[soPath] = handle;

    try {
        algoRegistrar_->validateLastRegistration();
        algorithms_.push_back(make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(
            algoRegistrar_->end()[-1]));
    } catch (const AlgorithmRegistrar::BadRegistrationException& e) {
        {
            lock_guard<mutex> errLock(stderrMutex_);
            cerr << "Bad registration in " << name << ": " << e.name << endl;
        }
        algoRegistrar_->removeLast();
        dlclose(handle);
        algoPathToHandle_.erase(soPath);
        algoNameToPath_.erase(name);
        algoUsageCounts_.erase(name);
        throw;
    }
}

/**
 * @brief Retrieves and validates a loaded algorithm by name.
 *
 * Searches the internal list of successfully registered algorithms and returns
 * the matching factory wrapper if found and fully initialized (i.e., both player
 * and tank algorithm factories are present). Returns nullptr if not found or invalid.
 *
 * This is a lightweight lookup and does not attempt to load the algorithm
 * or log errors — it is used after calling ensureAlgorithmLoaded.
 *
 * @param name The algorithm's name (without `.so` extension).
 * @return Shared pointer to the algorithm factories, or nullptr on failure.
 */
shared_ptr<AlgorithmRegistrar::AlgorithmAndPlayerFactories> CompetitiveSimulator::getValidatedAlgorithm(const string& name) {
    lock_guard<mutex> lock(handlesMutex_);
    for (const auto& algo : algorithms_) {
        if (algo->name() == name) {
            if (!algo->hasPlayerFactory() || !algo->hasTankAlgorithmFactory()) return nullptr;
            return algo;
        }
    }
    return nullptr;
}

/**
 * @brief Executes all scheduled games using a thread pool.
 *
 * Creates multiple worker threads (up to the user-specified maximum) that process the game queue concurrently.
 */
void CompetitiveSimulator::runGames() {
    size_t threadCount = min(numThreads_, scheduledGames_.size());
    if (threadCount == 1) { // Main thread runs all games sequentially
        for (const auto& task : scheduledGames_) {
            runSingleGame(task); // Run all games sequentially if only one thread
        }
        return;
    }

    std::atomic<size_t> nextTask{0};
    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    
    // Worker workflow
    auto worker = [&]() {
        while (true) {
            size_t idx = nextTask.fetch_add(1, std::memory_order_relaxed);
            if (idx >= scheduledGames_.size()) break;
            runSingleGame(scheduledGames_[idx]);
        }
    };

    for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(worker);
        }
    
    // Wait for all threads to finish working
    for (auto& t : workers) {
        t.join();
    }
}

/**
 * @brief Runs a single game between two algorithms on a given map.
 *
 * Lazily loads the algorithm shared libraries if not already loaded,
 * creates players from registered factories, and executes the game
 * via the currently loaded GameManager. At the end of the game,
 * it updates the score and manages algorithm usage count (including
 * unloading the shared libraries when no longer in use).
 *
 * If the game map fails to load, or if algorithms are not properly registered,
 * the game is skipped and the error is logged.
 *
 * @param task Game configuration including map path and participating algorithms.
 */
void CompetitiveSimulator::runSingleGame(const GameTask& task) {
    fs::path mapPath = task.mapPath;
    MapData mapData = readMap(mapPath);
    if (mapData.failedInit) {
        lock_guard<mutex> lock(stderrMutex_);
        cerr << "Failed to load map: " << mapPath << endl;
        return;
    }

    // Get algorithm and player factories from shared libraries
    const string& name1 = task.algoName1;
    const string& name2 = task.algoName2;

    try {
        ensureAlgorithmLoaded(name1);
        ensureAlgorithmLoaded(name2);
    } catch (const exception& e) {
        lock_guard<mutex> lock(stderrMutex_);
        cerr << "Failed to load algorithm(s) for game on map: " << mapPath << "\n" << "Reason: " << e.what() << endl;
        return;
    }

    {
        // Get the corresponding AlgorithmAndPlayerFactories
        auto algo1 = getValidatedAlgorithm(name1);
        auto algo2 = getValidatedAlgorithm(name2);
        if (!algo1 || !algo2) {
            lock_guard<mutex> lock(stderrMutex_);
            cerr << "Error: Missing factories for one of the algorithms while running map: " << mapPath << "\n"
                << "Algorithms: " << name1 << " vs. " << name2 << endl;
            return;
        }

        // Create players
        auto player1 = algo1->createPlayer(1, mapData.cols, mapData.rows, mapData.maxSteps, mapData.numShells);
        auto player2 = algo2->createPlayer(2, mapData.cols, mapData.rows, mapData.maxSteps, mapData.numShells);

        // Run game manager with players and factories
        auto gm = createGameManager();
        GameResult result = gm->run(mapData.cols, mapData.rows, *mapData.satelliteView, mapData.name,
            mapData.maxSteps, mapData.numShells,*player1, name1, *player2, name2,
            algo1->getTankAlgorithmFactory(),algo2->getTankAlgorithmFactory()
        );
        

        // Use GameResult to update scores
        bool tie = result.winner == 0;
        if (tie) {
            updateScore(name1, name2, true);
        } else if (result.winner == 1) {
            updateScore(name1, name2, false);
        } else {
            updateScore(name2, name1, false);
        }
    }

    decreaseUsageCount(name1);
    decreaseUsageCount(name2);
}

/**
 * @brief Updates the score table based on the game result.
 *
 * Awards 3 points for a win and 1 point to each algorithm in case of a tie.
 *
 * @param winner Name of the winning algorithm (if applicable).
 * @param loser Name of the losing algorithm (if applicable).
 * @param tie True if the game ended in a tie.
 */
void CompetitiveSimulator::updateScore(const string& winner, const string& loser, bool tie) {
    lock_guard<mutex> lock(scoresMutex_); // Make sure score update is thread safe
    if (tie) {
        scores_[winner] += 1;
        scores_[loser] += 1;
    } else {
        scores_[winner] += 3;
    }
}

/**
 * @brief Writes the simulation results to an output file in the algorithms folder.
 *
 * If the file cannot be created, prints the result to stdout instead.
 *
 * @param outFolder Path to the folder where the output file will be written.
 * @param mapFolder Path to the folder containing map files.
 * @param gmSoPath Path to the GameManager shared library used in the simulation.
 */
void CompetitiveSimulator::writeOutput(const string& outFolder,
                                       const string& mapFolder,
                                       const string& gmSoPath) {
    ofstream out(outFolder + "/competition_" + timestamp() + ".txt");
    std::ostream& dest = out ? static_cast<std::ostream&>(out) : static_cast<std::ostream&>(cout);
    if (!out) {
        cerr << "Failed to open output file, printing to stdout instead." << endl;
    }

    dest << "game_maps_folder=" << mapFolder << "\n";
    dest << "game_manager=" << fs::path(gmSoPath).filename().string() << "\n\n";

    vector<pair<string,int>> sorted(scores_.begin(), scores_.end());
    sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return b.second < a.second; });
    for (auto& [name, score] : sorted) dest << name << " " << score << "\n";
}

/**
 * @brief Creates a new instance of the loaded GameManager using the factory.
 *
 * @return unique_ptr to a new AbstractGameManager instance.
 */
unique_ptr<AbstractGameManager> CompetitiveSimulator::createGameManager() {
    return gameManagerFactory_(verbose_);
}

/**
 * @brief Decreases the usage count of an algorithm and releases its shared library if no longer needed.
 *
 * @param algoName Name of the algorithm whose usage count should be decreased.
 */
void CompetitiveSimulator::decreaseUsageCount(const std::string& algoName) {
    std::lock_guard<std::mutex> lock(handlesMutex_);

    auto it = algoUsageCounts_.find(algoName);
    if (it == algoUsageCounts_.end()) return;
    if (--(it->second) != 0) return;

    const auto pathIt = algoNameToPath_.find(algoName);
    if (pathIt == algoNameToPath_.end()) { algoUsageCounts_.erase(it); return; }
    const std::string soPath = pathIt->second;

    algorithms_.erase(std::remove_if(algorithms_.begin(), algorithms_.end(),
                                     [&](const auto& a){ return a->name() == algoName; }),
                      algorithms_.end());

    algoRegistrar_->eraseByName(algoName);

    if (auto h = algoPathToHandle_.find(soPath); h != algoPathToHandle_.end()) {
        dlclose(h->second);
        algoPathToHandle_.erase(h);
    }

    algoNameToPath_.erase(pathIt);
    algoUsageCounts_.erase(it);
}