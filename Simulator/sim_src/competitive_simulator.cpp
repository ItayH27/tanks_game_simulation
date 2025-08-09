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

CompetitiveSimulator::CompetitiveSimulator(bool verbose, size_t numThreads)
    : verbose_(verbose), numThreads_(numThreads) {}

CompetitiveSimulator::~CompetitiveSimulator() {
    std::lock_guard<std::mutex> lock(handlesMutex_);
    for (auto& [path, handleData] : algoLibHandles_) {
        if (handleData.handle && handleData.refCount > 0) {
            dlclose(handleData.handle);
        }
    }
    if (gameManagerHandle_) {
        dlclose(gameManagerHandle_);
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
    if (!loadAlgorithms(algorithmsFolder)) { // Make sure there are least 2 algorithms
        std::cerr << "Error: Need at least 2 algorithms for competition." << std::endl;
        return 1;
    }

    // Load maps into vector
    std::vector<std::filesystem::path> maps;
    if (!loadMaps(mapsFolder, maps)) {
        std::cerr << "Error: No maps found in folder: " << mapsFolder << std::endl;
        return 1;
    }

    scheduleGames(maps); // Scheduled all games to run
    runGames(); // Run all games using threads
    writeOutput(algorithmsFolder, mapsFolder, gameManagerSoPath); // Write the require output

    return 0;
}

/**
 * @brief Dynamically loads the GameManager shared library and retrieves its factory function.
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

    // Get factory ptr from loaded gamemanger
    void* factoryPtr = dlsym(gameManagerHandle_, "_Z20game_manager_factoryb");
    const char* error = dlerror();
    if (error || !factoryPtr) {
        std::cerr << "dlsym failed to find GameManager factory: " << (error ? error : "null") << std::endl;
        return false;
    }

    // Recast into known type
    gameManagerFactory_ = *reinterpret_cast<GameManagerFactory*>(factoryPtr);
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
bool CompetitiveSimulator::loadAlgorithms(const std::string& folder) {
    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();

    // Go over all files in the algorithms folder
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() == ".so") { // We care only about .so files
            const std::string soPath = entry.path().string(); // Get the .so path
            const std::string name = entry.path().stem().string(); // Get files name

            registrar.createAlgorithmFactoryEntry(name);

            void* handle = nullptr;
            {
                std::lock_guard<std::mutex> lock(handlesMutex_);
                auto& handleData = algoLibHandles_[soPath]; // Get algo handle
                if (!handleData.handle) { // if not already opened
                    handleData.handle = dlopen(soPath.c_str(), RTLD_NOW);
                    if (!handleData.handle) { // Failed to dlopen
                        std::cerr << "Failed to load " << soPath << ": " << dlerror() << std::endl;
                        registrar.removeLast(); // Remove last added alogrithm factory
                        algoLibHandles_.erase(soPath); // Erase leftover soPath
                        continue;
                    }
                }
                ++handleData.refCount; // Increment refCount for habdle data
                handle = handleData.handle;
            }

            try {
                registrar.validateLastRegistration(); // Validate added algo
                algorithms_.push_back(std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>
                    (registrar.end()[-1])); // Add actual algo to algos list

                std::lock_guard<std::mutex> lock(handlesMutex_);
                algoNameToPath_[name] = soPath; // Update mapping to algo
            } catch (const AlgorithmRegistrar::BadRegistrationException& e) {
                std::cerr << "Bad algorithm registration in: " << name << std::endl;
                registrar.removeLast();
                std::lock_guard<std::mutex> lock(handlesMutex_);
                auto& handleData = algoLibHandles_[soPath];
                if (--handleData.refCount == 0 && handleData.handle) {
                    dlclose(handleData.handle);
                    algoLibHandles_.erase(soPath);
                }
                continue;
            }
        }
    }

    return algorithms_.size() > 1;
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
    size_t N = algorithms_.size();
    for (size_t k = 0; k < maps.size(); ++k) {
        for (size_t i = 0; i < N; ++i) {
            size_t j = (i + 1 + k % (N - 1)) % N;
            if (i < j) {
                auto& algo1 = algorithms_[i];
                auto& algo2 = algorithms_[j];
                scheduledGames_.push_back({maps[k], algo1, algo2});

                std::lock_guard<std::mutex> lock(handlesMutex_);
                algoUsageCounts_[algo1->name()]++;
                algoUsageCounts_[algo2->name()]++;
            }
        }
    }
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
 * @brief Runs a single game using the provided GameTask configuration.
 *
 * Currently a stub that creates a GameManager and simulates a tie result.
 * Needs to be extended to actually load the map and run the game.
 *
 * @param task Game configuration including map, algorithm factories, and player data.
 */

void CompetitiveSimulator::runSingleGame(const GameTask& task) {
    auto gm = createGameManager();

    std::filesystem::path mapPath = task.mapPath;
    // TODO: Read map file and extract game data (num_shells, etc...) and create satellite view

    // Get algorithm and player factories from shared libraries
    auto algo1 = task.algo1;
    auto algo2 = task.algo2;
    string name1 = algo1->name();
    string name2 = algo2->name();

    if (!algo1->hasPlayerFactory() || !algo1->hasTankAlgorithmFactory() ||
        !algo2->hasPlayerFactory() || !algo2->hasTankAlgorithmFactory()) {
        std::cerr << "Error: One of the algorithms is missing factories." << std::endl;
        return;
        }

    // Create players using factories
    auto player1 = algo1->createPlayer(/*player_index=*/1, /*x=*/..., /*y=*/..., max_steps, num_shells);
    auto player2 = algo2->createPlayer(/*player_index=*/2, /*x=*/..., /*y=*/..., max_steps, num_shells);

    // Run game manager with players and factories
    GameResult result = gm->run(
        map_width, map_height,
        *mapView, map_name,
        max_steps, num_shells,
        *player1, name1, *player2, name2,
        algo1->getTankAlgorithmFactory(),
        algo2->getTankAlgorithmFactory()
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
 * @brief Generates a timestamp string suitable for use in filenames.
 *
 * @return String in format YYYYMMDD_HHMMSS representing current local time.
 */

std::string CompetitiveSimulator::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ss.str();
}

/**
 * @brief Decrements the reference count of a loaded algorithm and unloads it if no longer in use.
 *
 * @param path Path to the algorithm `.so` file to release.
 */

void CompetitiveSimulator::releaseAlgorithmLib(const std::string& path) {
    std::lock_guard<std::mutex> lock(handlesMutex_); // Make sure algo release is thread safe
    auto it = algoLibHandles_.find(path); // Get iterator to realsed path
    if (it != algoLibHandles_.end()) { // If not already closed
        if (--(it->second.refCount) == 0 && it->second.handle) { // Remove
            dlclose(it->second.handle);
            algoLibHandles_.erase(it);
        }
    }
}

/**
 * @brief Decreases the usage count of an algorithm and releases its shared library if no longer needed.
 *
 * @param algoName Name of the algorithm whose usage count should be decreased.
 */

void CompetitiveSimulator::decreaseUsageCount(const std::string& algoName) {
    std::lock_guard<std::mutex> lock(handlesMutex_);
    auto it = algoUsageCounts_.find(algoName);
    if (it != algoUsageCounts_.end()) {
        if (--(it->second) == 0) {
            const auto& soPath = algoNameToPath_[algoName];
            releaseAlgorithmLib(soPath);
            algoUsageCounts_.erase(it);
        }
    }
}
