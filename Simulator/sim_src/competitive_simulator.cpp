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

#include "sim_include/Simulator.h"

CompetitiveSimulator::CompetitiveSimulator(bool verbose, size_t numThreads)
    : Simulator(verbose, numThreads) {}

CompetitiveSimulator::~CompetitiveSimulator() {
    std::lock_guard<std::mutex> lock(handlesMutex_);
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
int CompetitiveSimulator::run(const std::string& mapsFolder,
                              const std::string& gameManagerSoPath,
                              const std::string& algorithmsFolder) {

    // Load gamemanager using .so path
    if (!loadGameManager(gameManagerSoPath)) {
        return 1;
    }

    // Load algorithms using folder path
    if (!getAlgorithms(algorithmsFolder)) { // Make sure there are least 2 algorithms
        std::cerr << "Usage: At least two algorithms must be present in folder: " << algorithmsFolder << std::endl;
        return 1;
    }

    // Load maps into vector
    std::vector<std::filesystem::path> maps;
    if (!loadMaps(mapsFolder, maps)) {
        std::cerr << "Usage Error: No valid map files found in folder: " << mapsFolder << "\n"
          << "Make sure the folder exists and contains at least one valid map file." << std::endl;
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
bool CompetitiveSimulator::loadGameManager(const std::string& soPath) {
    gameManagerHandle_ = dlopen(soPath.c_str(), RTLD_LAZY);
    if (!gameManagerHandle_) { // Failed to dlopen gamemanager
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
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
bool CompetitiveSimulator::getAlgorithms(const std::string& folder) {
    bool foundAny = false;

    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() == ".so") {
            const std::string soPath = entry.path().string();
            const std::string name = entry.path().stem().string();

            // Record metadata — don't dlopen yet
            {
                std::lock_guard<std::mutex> lock(handlesMutex_);
                algoNameToPath_[name] = soPath;
                algoUsageCounts_[name] = 0;
            }

            foundAny = true;
        }
    }

    return foundAny;
}

/**
 * @brief Loads all regular files from the given folder as candidate game maps.
 *
 * @param folder Path to the folder containing map files.
 * @param outMaps Output vector where the discovered map paths will be stored.
 * @return true if any map files were found, false otherwise.
 */
bool CompetitiveSimulator::loadMaps(const std::string& folder, std::vector<std::filesystem::path>& outMaps) {
    // Iterate over all maps in folder and add them to maps vector
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
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
void CompetitiveSimulator::scheduleGames(const std::vector<std::filesystem::path>& maps) {
    std::vector<std::string> algoNames;

    {
        std::lock_guard<std::mutex> lock(handlesMutex_);
        for (const auto& [name, _] : algoNameToPath_) {
            algoNames.push_back(name);
        }
    }

    size_t N = algoNames.size();
    for (size_t k = 0; k < maps.size(); ++k) {
        // Skip duplicated round if N is even and k == N / 2 - 1
        if (N % 2 == 0 && k == N / 2 - 1) {
            continue;
        }

        for (size_t i = 0; i < N; ++i) {
            size_t j = (i + 1 + k % (N - 1)) % N;
            if (i < j) {
                scheduledGames_.push_back({maps[k], algoNames[i], algoNames[j]});

                std::lock_guard<std::mutex> lock(handlesMutex_);
                algoUsageCounts_[algoNames[i]]++;
                algoUsageCounts_[algoNames[j]]++;
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
 * @throws std::runtime_error if the library fails to load or registration is invalid.
 */
void CompetitiveSimulator::ensureAlgorithmLoaded(const std::string& name) {
    std::lock_guard<std::mutex> lock(handlesMutex_);

    if (algoPathToHandle_.count(algoNameToPath_[name])) {
        // already loaded
        return;
    }

    const std::string& soPath = algoNameToPath_[name];
    void* handle = dlopen(soPath.c_str(), RTLD_LAZY);
    if (!handle) {
        {
            std::lock_guard<std::mutex> errLock(stderrMutex_);
            std::cerr << "Error: Failed to load algorithm " << soPath << ": " << dlerror() << std::endl;
        }
        return;
    }

    algoPathToHandle_[soPath] = handle;

    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();
    registrar.createAlgorithmFactoryEntry(name);

    try {
        registrar.validateLastRegistration();
        algorithms_.push_back(std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(
            registrar.end()[-1]));
    } catch (const AlgorithmRegistrar::BadRegistrationException& e) {
        {
            std::lock_guard<std::mutex> errLock(stderrMutex_);
            std::cerr << "Bad registration in " << name << ": " << e.name << std::endl;
        }
        registrar.removeLast();
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
auto CompetitiveSimulator::getValidatedAlgorithm(const std::string& name) {
    for (const auto& algo : algorithms_) {
        if (algo->name() == name) {
            if (!algo->hasPlayerFactory() || !algo->hasTankAlgorithmFactory()) {
                return nullptr;
            }
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
    std::mutex queueMutex;
    size_t nextTask = 0;
    std::vector<std::thread> workers;

    // Worker workflow
    auto worker = [&]() {
        while (true) {
            GameTask task;
            {
                // Make sure each game is safely grabbed by a single thread
                std::lock_guard<std::mutex> lock(queueMutex);
                if (nextTask >= scheduledGames_.size()) return;
                task = scheduledGames_[nextTask++];
            }
            runSingleGame(task); // Runs the scheduled game
        }
    };

    // Create worker pool
    size_t threadCount = std::min(numThreads_, scheduledGames_.size());
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
    auto gm = createGameManager();

    std::filesystem::path mapPath = task.mapPath;
    MapData mapData = readMap(mapPath);
    if (mapData.failedInit) {
        std::lock_guard<std::mutex> lock(stderrMutex_);
        std::cerr << "Failed to load map: " << mapPath << std::endl;
        return;
    }

    // Get algorithm and player factories from shared libraries
    const std::string& name1 = task.algoName1;
    const std::string& name2 = task.algoName2;

    try {
        ensureAlgorithmLoaded(name1);
        ensureAlgorithmLoaded(name2);
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stderrMutex_);
        std::cerr << "Failed to load algorithm(s) for game on map: " << mapPath << "\n" << "Reason: " << e.what() << std::endl;
        return;
    }

    // Get the corresponding AlgorithmAndPlayerFactories
    auto algo1 = getValidatedAlgorithm(name1);
    auto algo2 = getValidatedAlgorithm(name2);
    if (!algo1 || !algo2) {
        std::lock_guard<std::mutex> lock(stderrMutex_);
        std::cerr << "Error: Missing factories for one of the algorithms while running map: " << mapPath << "\n"
              << "Algorithms: " << name1 << " vs. " << name2 << std::endl;
        return;
    }

    // Create players
    auto player1 = algo1->createPlayer(1, mapData.cols, mapData.rows, mapData.maxSteps, mapData.numShells);
    auto player2 = algo2->createPlayer(2, mapData.cols, mapData.rows, mapData.maxSteps, mapData.numShells);


    // Run game manager with players and factories
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
void CompetitiveSimulator::updateScore(const std::string& winner, const std::string& loser, bool tie) {
    std::lock_guard<std::mutex> lock(scoresMutex_); // Make sure score update is thread safe
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
void CompetitiveSimulator::writeOutput(const std::string& outFolder, const std::string& mapFolder, const std::string& gmSoPath) {
    std::ofstream out(outFolder + "/competition_" + timestamp() + ".txt");
    if (!out) {
        std::cerr << "Failed to open output file, printing to stdout instead." << std::endl;
        out.copyfmt(std::cout); out.clear(std::cout.rdstate());
    }
    out << "game_maps_folder=" << mapFolder << "\n";
    out << "game_manager=" << std::filesystem::path(gmSoPath).filename().string() << "\n\n";

    std::vector<std::pair<std::string, int>> sortedScores(scores_.begin(), scores_.end());
    std::sort(sortedScores.begin(), sortedScores.end(), [](const auto& a, const auto& b) {
        return b.second < a.second;
    });

    for (const auto& [name, score] : sortedScores) {
        out << name << " " << score << "\n";
    }
}

/**
 * @brief Creates a new instance of the loaded GameManager using the factory.
 *
 * @return std::unique_ptr to a new AbstractGameManager instance.
 */
std::unique_ptr<AbstractGameManager> CompetitiveSimulator::createGameManager() {
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

    if (--(it->second) == 0) {
        const std::string& soPath = algoNameToPath_[algoName];

        auto handleIt = algoPathToHandle_.find(soPath);
        if (handleIt != algoPathToHandle_.end()) {
            dlclose(handleIt->second);
            algoPathToHandle_.erase(handleIt);
        }

        algoNameToPath_.erase(algoName);
        algoUsageCounts_.erase(it);
    }
}
