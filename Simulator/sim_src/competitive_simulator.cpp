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
#include <unordered_set>
#include <condition_variable>
#include "GameManagerRegistrar.h"
#include "../sim_include/Simulator.h"
#include "../sim_include/AlgorithmRegistrar.h"

CompetitiveSimulator::CompetitiveSimulator(bool verbose, size_t numThreads)
    : Simulator(verbose, numThreads) {
        logger_.debug("CompetitiveSimulator initialized with verbosity=", verbose, ", threads=", numThreads);
        algoRegistrar_ = &AlgorithmRegistrar::getAlgorithmRegistrar();
    }

CompetitiveSimulator::~CompetitiveSimulator() {
    // Ensure algorithm objects are destroyed before unloading .so files
    for (auto& [path, handle] : algoPathToHandle_) {
        if (handle) {
            int result = dlclose(handle);
            if (result != 0) {
                logger_.reportWarn("dlclose failed for ", path, ": ", dlerror());
            }
            handle = nullptr;
        }
    }
    algoPathToHandle_.clear();

    if (gameManagerHandle_) {
        int result = dlclose(gameManagerHandle_);
        if (result != 0) {
            logger_.reportWarn("dlclose failed for game manager: ", dlerror());
        }
        gameManagerHandle_ = nullptr;
    }

    logger_.debug("CompetitiveSimulator destroyed, all .so handles closed.");
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

    logger_.info("Starting competitive simulation...");
    if (!loadGameManager(gameManagerSoPath)) {
        return 1;
    }

    if (!getAlgorithms(algorithmsFolder)) { // Make sure there are least 2 algorithms
        logger_.reportError("At least two algorithms must be present in folder: ", algorithmsFolder);
        return 1;
    }
    for (auto& [name, _] : algoNameToPath_) { scores_[name] = 0; } // Zero out scores for all algorithms

    vector<fs::path> maps;
    if (!loadMaps(mapsFolder, maps)) {
        logger_.reportError("No valid map files found in folder: ", mapsFolder,
                             "\nMake sure the folder exists and contains at least one valid map file.");
        return 1;
    }

    scheduleGames(maps); 
    runGames(); 
    writeOutput(algorithmsFolder, mapsFolder, gameManagerSoPath); 

    logger_.info("Competitive simulation completed.");
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

    // Register the GameManager
    auto& registrar = GameManagerRegistrar::getGameManagerRegistrar();
    logger_.debug("Registering GameManager: ", soName);
    registrar.createEntry(soName);

    logger_.debug("Loading GameManager from: ", absPath.string());
    gameManagerHandle_ = dlopen(absPath.c_str(), RTLD_LAZY | RTLD_NODELETE);
    if (!gameManagerHandle_) {
        logger_.reportError("Failed loading GameManager .so file from path: ", absPath.string(),
                             "\n", dlerror());
        registrar.removeLast();
        return false;
    }

    try {
        registrar.validateLast(); // REGISTER_GAME_MANAGER ran and set a factory

        // Capture soName and registrar by reference to create the factory lambda
        gameManagerFactory_ = [soName, &registrar](bool verbose) -> std::unique_ptr<AbstractGameManager> {
            for (auto it = registrar.begin(); it != registrar.end(); ++it) {
                if (it->name() == soName) {
                    return it->create(verbose);
                }
            }
            throw std::runtime_error("GameManager not registered: " + soName);
        };
    } catch (const std::exception& e) {
        logger_.reportError("Error validating GameManager registration for ", soName, ": ", e.what());
        registrar.removeLast();
        dlclose(gameManagerHandle_);
        gameManagerHandle_ = nullptr;
        return false;
    }

    logger_.info("Successfully loaded GameManager: ", soName);
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

    // Iterate over all .so files in the folder
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.path().extension() == ".so") {
            const string soPath = entry.path().string();
            const string name = entry.path().stem().string();
            logger_.debug("Found algorithm .so: ", soPath);
            
            algoNameToPath_[name] = soPath;
            algoUsageCounts_[name] = 0; // Initialize usage count

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
            logger_.debug("Found map file: ", entry.path().string());
            outMaps.push_back(entry.path());
        }
    }

    logger_.info("Found ", outMaps.size(), " map(s) in folder: ", folder);
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
    // Collect algo names in the container's natural iteration order (no sorting).
    std::vector<std::string> algoNames;
    algoNames.reserve(algoNameToPath_.size());
    for (const auto& kv : algoNameToPath_) {
        algoNames.push_back(kv.first);
    }

    const size_t N = algoNames.size();
    if (N < 2) return;

    const size_t R = N - 1; // used for k % (N-1)

    // Helper to create a unique key for an unordered pair (a,b) with a != b
    auto keyOf = [](uint32_t a, uint32_t b) -> uint64_t {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | b;
    };

    // Schedule games according to the round-robin scheme
    for (size_t k = 0; k < maps.size(); ++k) {
        const auto& map = maps[k];

        // Dedup unordered pairs per map (handles the even-N middle "symmetric" case)
        std::unordered_set<uint64_t> seen;

        const uint32_t shift = static_cast<uint32_t>(k % R); // r = k % (N-1)
        for (uint32_t i = 0; i < static_cast<uint32_t>(N); ++i) {
            const uint32_t j = (i + 1 + shift) % static_cast<uint32_t>(N);

            const uint64_t key = keyOf(i, j);
            if (seen.insert(key).second) {
                logger_.debug("Scheduling game: ", algoNames[i], " vs. ", algoNames[j],
                               " on map ", map.filename().string());
                // Schedule one direction only (no mirroring).
                scheduledGames_.push_back({ map, algoNames[i], algoNames[j] });
                ++algoUsageCounts_[algoNames[i]];
                ++algoUsageCounts_[algoNames[j]];
            }
        }
    }

    logger_.info("Scheduled ", scheduledGames_.size(), " game(s) across ", maps.size(), " map(s) and ", N, " algorithm(s).");
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
    if (algoPathToHandle_.count(soPath)) {
        logger_.debug("Algorithm already loaded: ", name);
        return;
    }

    algoRegistrar_->createAlgorithmFactoryEntry(name);

    logger_.debug("Loading algorithm from: ", soPath);
    void* handle = dlopen(soPath.c_str(), RTLD_LAZY | RTLD_NODELETE);
    if (!handle) {
        algoRegistrar_->removeLast();
        throw std::runtime_error(std::string("Failed to load algorithm ") + soPath + ": " + dlerror());
    }

    algoPathToHandle_[soPath] = handle;

    try {
        algoRegistrar_->validateLastRegistration();
        algorithms_.push_back(make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(algoRegistrar_->end()[-1]));
    } catch (const AlgorithmRegistrar::BadRegistrationException& e) {
        logger_.reportError("Bad registration in ", name, ": ",
                                "hasName=", e.hasName, ", hasPlayerFactory=", e.hasPlayerFactory,
                                ", hasTankAlgorithmFactory=", e.hasTankAlgorithmFactory);
        algoRegistrar_->removeLast();
        dlclose(handle);
        algoPathToHandle_.erase(soPath);
        algoNameToPath_.erase(name);
        algoUsageCounts_.erase(name);
        throw;
    }

    logger_.info("Successfully loaded algorithm: ", name); 
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

    logger_.reportWarn("Algorithm not found in validated list: ", name);
    return nullptr;
}

/**
 * @brief Executes all scheduled games using a thread pool.
 *
 * Creates multiple worker threads (up to the user-specified maximum) that process the game queue concurrently.
 */
void CompetitiveSimulator::runGames() {
    size_t threadCount = min(numThreads_, scheduledGames_.size()); // Deciede number of threads to use based on scheduled games
    logger_.info("Running games using ", threadCount, " thread(s)...");
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
        logger_.debug("Thread ", std::this_thread::get_id(), " started.");
        while (true) {
            size_t idx = nextTask.fetch_add(1, std::memory_order_relaxed);
            if (idx >= scheduledGames_.size()) break;
            runSingleGame(scheduledGames_[idx]);
            logger_.debug("Thread ", std::this_thread::get_id(), " completed game ", idx + 1, "/", scheduledGames_.size());
        }
    };

    for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back(worker);
        }
    
    // Wait for all threads to finish working
    for (auto& t : workers) {
        logger_.debug("Joining thread ", t.get_id());
        t.join();
    }

    logger_.info("All games completed.");
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
        logger_.reportWarn("Failed to load map: ", mapPath.string(), " - skipping game.");
        return;
    }

    // Get algorithm and player factories from shared libraries
    const string& name1 = task.algoName1;
    const string& name2 = task.algoName2;

    try {
        // Load algorithms if not already loaded
        ensureAlgorithmLoaded(name1);
        ensureAlgorithmLoaded(name2);
    } catch (const exception& e) {
        logger_.reportWarn("Failed to load algorithm(s) for game on map: ", mapPath.string(),
                             " - skipping game.\nReason: ", e.what());
        return;
    }

    {
        // Get the corresponding AlgorithmAndPlayerFactories
        auto algo1 = getValidatedAlgorithm(name1);
        auto algo2 = getValidatedAlgorithm(name2);
        if (!algo1 || !algo2) {
            logger_.reportWarn("Missing factories for one of the algorithms while running map: ",
                                mapPath.string(), " - skipping game.");
            return;
        }

        // Create players
        auto player1 = algo1->createPlayer(1, mapData.cols, mapData.rows, mapData.maxSteps, mapData.numShells);
        auto player2 = algo2->createPlayer(2, mapData.cols, mapData.rows, mapData.maxSteps, mapData.numShells);

        // Run game manager with players and factories
        auto gm = createGameManager();
        if (!gm) {
            logger_.reportWarn("Failed to create game manager for map: ", mapPath.string());
            return;
        }
        logger_.debug("Thread ", std::this_thread::get_id(), " running game: ",
                   task.algoName1, " vs. ", task.algoName2, " on map ", task.mapPath);
        GameResult result = gm->run(mapData.cols, mapData.rows, *mapData.satelliteView, mapData.name,
            mapData.maxSteps, mapData.numShells,*player1, name1, *player2, name2,
            algo1->getTankAlgorithmFactory(),algo2->getTankAlgorithmFactory()
        );
        

        // Use GameResult to update scores
        bool tie = result.winner == 0;
        logger_.info("Game completed: ", name1, " vs. ", name2, " on map ", mapPath.filename().string(),
                     " - Result: ", tie ? "Tie" : (result.winner == 1 ? (name1 + " wins") : (name2 + " wins")));
        if (tie) {
            updateScore(name1, name2, true);
        } else if (result.winner == 1) {
            updateScore(name1, name2, false);
        } else {
            updateScore(name2, name1, false);
        }
    }

    // Decrease usage count and unload algorithms if no longer needed
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
    logger_.debug("Updating score: ", winner, " vs. ", loser, tie ? " (tie)" : " (win/loss)");
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
    // Create output file with timestamped name
    string resultName = "competition_" + timestamp() + ".txt";
    fs::path outPath = fs::path(outFolder) / resultName;
    std::ofstream out(outPath.string());
    std::ostream& dest = out ? static_cast<std::ostream&>(out) : static_cast<std::ostream&>(std::cout);
    if (!out) {
        logger_.reportWarn("Failed to open output file in folder: ", outFolder,
                            " - printing results to stdout instead.");
    }

    dest << "game_maps_folder=" << mapFolder << "\n";
    dest << "game_manager=" << fs::path(gmSoPath).filename().string() << "\n\n";

    // Sort scores in descending order and write to output
    vector<pair<string,int>> sorted(scores_.begin(), scores_.end());
    sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return b.second < a.second; });
    for (auto& [name, score] : sorted) dest << name << " " << score << "\n";

    logger_.info("Results written to ", out ? outPath.string() : "stdout");
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

    // Unload the .so file
    if (auto h = algoPathToHandle_.find(soPath); h != algoPathToHandle_.end()) {
        logger_.debug("Unloading algorithm: ", algoName, " from path: ", soPath);
        dlclose(h->second);
        algoPathToHandle_.erase(h);
    }

    // Clean up maps
    algoNameToPath_.erase(pathIt);
    algoUsageCounts_.erase(it);
}