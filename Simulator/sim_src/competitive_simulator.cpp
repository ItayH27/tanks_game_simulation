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

/**
 * @brief Constructor for CompetitiveSimulator.
 *
 * @param verbose Whether verbose output is enabled.
 * @param numThreads Number of threads to use for parallel game execution.
 */
CompetitiveSimulator::CompetitiveSimulator(bool verbose, size_t numThreads)
    : verbose_(verbose), numThreads_(numThreads) {}

/**
 * @brief Runs the competition simulation.
 *
 * Loads GameManager and Algorithm .so files, loads all map files,
 * schedules all valid algorithm pairings per map, executes games concurrently,
 * and writes the competition results to a file.
 *
 * @param mapsFolder Path to the folder containing game maps.
 * @param gameManagerSoPath Path to the GameManager shared object (.so).
 * @param algorithmsFolder Path to folder containing algorithm .so files.
 * @return int 0 on success, non-zero on error.
 */
int CompetitiveSimulator::run(const std::string& mapsFolder,
                              const std::string& gameManagerSoPath,
                              const std::string& algorithmsFolder) {
    // Load GameManager
    if (!loadGameManager(gameManagerSoPath)) {
        return 1;
    }

    // Load Algorithms
    if (!loadAlgorithms(algorithmsFolder)) {
        return 1;
    }
    if (algorithms_.size() < 2) {
        std::cerr << "Error: Need at least 2 algorithms for competition." << std::endl;
        return 1;
    }

    // Load Maps
    std::vector<std::filesystem::path> maps;
    if (!loadMaps(mapsFolder, maps)) {
        std::cerr << "Error: No maps found in folder: " << mapsFolder << std::endl;
        return 1;
    }

    // Schedule games
    scheduleGames(maps);

    // Run games
    runGames();

    // Write output
    writeOutput(algorithmsFolder, mapsFolder, gameManagerSoPath);

    return 0;
}

/**
 * @brief Loads the GameManager from a shared object file.
 *
 * Uses dlopen and dlsym to retrieve the GameManager factory.
 *
 * @param soPath Path to the GameManager .so file.
 * @return true if the GameManager was successfully loaded.
 * @return false otherwise.
 */
bool CompetitiveSimulator::loadGameManager(const std::string& soPath) {
    gameManagerHandle_ = dlopen(soPath.c_str(), RTLD_LAZY);
    if (!gameManagerHandle_) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return false;
    }

    void* factoryPtr = dlsym(gameManagerHandle_, "_Z20game_manager_factoryb");
    const char* error = dlerror();
    if (error || !factoryPtr) {
        std::cerr << "dlsym failed to find GameManager factory: " << (error ? error : "null") << std::endl;
        return false;
    }

    gameManagerFactory_ = *reinterpret_cast<GameManagerFactory*>(factoryPtr);
    return true;
}

/**
 * @brief Loads all algorithm .so files from a folder.
 *
 * Dynamically loads and validates algorithm shared libraries, each of which
 * must register a TankAlgorithm and Player. Valid algorithms are stored.
 *
 * @param folder Path to the folder containing .so files.
 * @return true if at least one algorithm was successfully loaded.
 * @return false otherwise.
 */
bool CompetitiveSimulator::loadAlgorithms(const std::string& folder) {
    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();

    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() == ".so") {
            const std::string soPath = entry.path().string();
            const std::string name = entry.path().stem().string();

            registrar.createAlgorithmFactoryEntry(name);
            void* handle = dlopen(soPath.c_str(), RTLD_NOW);
            if (!handle) {
                std::cerr << "Failed to load " << soPath << ": " << dlerror() << std::endl;
                registrar.removeLast();
                continue;
            }

            try {
                registrar.validateLastRegistration();
                algorithms_.push_back(std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(registrar.end()[-1]));
            } catch (const AlgorithmRegistrar::BadRegistrationException& e) {
                std::cerr << "Bad algorithm registration in: " << name << std::endl;
                registrar.removeLast();
                continue;
            }
        }
    }

    return !algorithms_.empty();
}

/**
 * @brief Loads all map file paths from a specified folder.
 *
 * @param folder Directory containing game maps.
 * @param outMaps Output vector to store the full paths of valid map files.
 * @return true if at least one map file was found.
 * @return false otherwise.
 */
bool CompetitiveSimulator::loadMaps(const std::string& folder, std::vector<std::filesystem::path>& outMaps) {
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_regular_file()) {
            outMaps.push_back(entry.path());
        }
    }
    return !outMaps.empty();
}

/**
 * @brief Schedules all matchups for the competition.
 *
 * Applies the assignment's rotation formula to pair algorithms
 * across all provided maps.
 *
 * @param maps List of map files to schedule games on.
 */
void CompetitiveSimulator::scheduleGames(const std::vector<std::filesystem::path>& maps) {
    size_t N = algorithms_.size();
    for (size_t k = 0; k < maps.size(); ++k) {
        for (size_t i = 0; i < N; ++i) {
            size_t j = (i + 1 + k % (N - 1)) % N;
            if (i < j) {
                scheduledGames_.push_back({maps[k], algorithms_[i], algorithms_[j]});
            }
        }
    }
}

/**
 * @brief Runs all scheduled games concurrently.
 *
 * Spawns up to numThreads_ worker threads, each responsible
 * for executing game tasks pulled from a shared queue.
 */
void CompetitiveSimulator::runGames() {
    std::mutex queueMutex;
    size_t nextTask = 0;
    std::vector<std::thread> workers;

    auto worker = [&]() {
        while (true) {
            GameTask task;
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (nextTask >= scheduledGames_.size()) return;
                task = scheduledGames_[nextTask++];
            }
            runSingleGame(task);
        }
    };

    size_t threadCount = std::min(numThreads_, scheduledGames_.size());
    for (size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back(worker);
    }
    for (auto& t : workers) {
        t.join();
    }
}

/**
 * @brief Executes a single game match between two algorithms.
 *
 * Instantiates GameManager, players, and runs a simulation.
 * Records the result for scoring.
 *
 * @param task GameTask containing the map and two algorithms to compete.
 */
void CompetitiveSimulator::runSingleGame(const GameTask& task) {
    auto gm = createGameManager();

    // TODO: Use actual params for the game
    /*
    auto player1 = task.algo1->createPlayer(1, 0, 0, 100, 10);
    auto player2 = task.algo2->createPlayer(2, 0, 0, 100, 10);

    auto result = gm->run(10, 10, *(task.algo1->createPlayer(0,0,0,0,0)->gameState),
     "map", 100, 10,
     *player1, task.algo1->name(), *player2, task.algo2->name(),
     [algo = task.algo1](int p, int t) { return algo->createTankAlgorithm(p, t); },
     [algo = task.algo2](int p, int t) { return algo->createTankAlgorithm(p, t); }
    );
    */

    bool tie = result.winner == 0;
    std::string name1 = task.algo1->name();
    std::string name2 = task.algo2->name();

    if (tie) {
        updateScore(name1, name2, true);
    } else if (result.winner == 1) {
        updateScore(name1, name2, false);
    } else {
        updateScore(name2, name1, false);
    }
}

/**
 * @brief Updates internal score table based on a game result.
 *
 * Awards 3 points for a win, 1 point each for a tie.
 * Thread-safe.
 *
 * @param winner Name of the winning algorithm.
 * @param loser Name of the losing algorithm.
 * @param tie True if the game was a tie.
 */
void CompetitiveSimulator::updateScore(const std::string& winner, const std::string& loser, bool tie) {
    std::lock_guard<std::mutex> lock(scoresMutex_);
    if (tie) {
        scores_[winner] += 1;
        scores_[loser] += 1;
    } else {
        scores_[winner] += 3;
    }
}

/**
 * @brief Writes the final competition results to a file.
 *
 * Outputs sorted algorithm scores to `competition_<timestamp>.txt`
 * inside the algorithms folder. Falls back to stdout if file can't be created.
 *
 * @param outFolder Algorithms folder where output file should be saved.
 * @param mapFolder Path to the maps folder, logged in header.
 * @param gmSoPath Path to the loaded GameManager .so file, logged in header.
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
 * @brief Creates a new instance of the loaded GameManager.
 *
 * @return std::unique_ptr<AbstractGameManager> New GameManager instance.
 */
std::unique_ptr<AbstractGameManager> CompetitiveSimulator::createGameManager() {
    return gameManagerFactory_(verbose_);
}

/**
 * @brief Generates a timestamp string for output file naming.
 *
 * @return std::string Timestamp in format YYYYMMDD_HHMMSS.
 */
std::string CompetitiveSimulator::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ss.str();
}
