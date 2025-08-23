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
        if (!algo_registrar) { // Should never happen
            throw std::runtime_error("Error: Failed to get AlgorithmRegistrar instance.");
        }

        game_manager_registrar = &GameManagerRegistrar::getGameManagerRegistrar();
        if (!game_manager_registrar) { // Should never happen
            throw std::runtime_error("Error: Failed to get GameManagerRegistrar instance.");
        }
}


ComparativeSimulator::~ComparativeSimulator() {
    // Ensure algorithm objects are destroyed before unloading .so files
    allResults.clear(); // if GameResult keeps algo-allocated objects
    groups.clear(); // clear the groups vector
    algo1_.reset(); // destroy the shared_ptr
    algo2_.reset(); // destroy the shared_ptr

     // Clear registrars to avoid dangling pointers

    if (game_manager_registrar) {
        game_manager_registrar->clear();
    }
    if (algo_registrar) {
        algo_registrar->clear();
    }

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
    logger_.info("Starting comparative simulation...");
    mapData_ = readMap(mapPath); // Read and validate map data
    if (mapData_.failedInit) {
        logger_.reportError("Failed to read map data from: ", mapPath);
        return 1;
    }
    logger_.debug("Map data read successfully: ", mapData_.name,
                   " (", mapData_.cols, "x", mapData_.rows, "), maxSteps=", mapData_.maxSteps,
                   ", numShells=", mapData_.numShells);

    // Load algorithm shared objects
    if (!loadAlgoSO(algorithmSoPath1) || !loadAlgoSO(algorithmSoPath2)) return 1;
    logger_.info("Loaded algorithm shared objects successfully: ", algorithmSoPath1, " and ", algorithmSoPath2);
    auto p1 = std::filesystem::canonical(algorithmSoPath1);
    auto p2 = std::filesystem::canonical(algorithmSoPath2);

    // Verify that both algorithms have the required factories
    if (p1 == p2) { // Same .so file provided twice
        algo1_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->begin()));
        algo2_ = algo1_;  
    } else { // Different .so files
        algo1_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->begin()));
        algo2_ = std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(*(algo_registrar->end() - 1));
    }

    // Get the game manager path names from the specified folder
    getGameManagers(gmFolder);
    if (gms_paths_.empty()) {
        logger_.reportError("No GameManager shared libraries found in folder: ", gmFolder);
        return 1;
    }
    logger_.debug("Found ", gms_paths_.size(), " GameManager .so files in folder: ", gmFolder);

    // Run all games
    runGames();
    logger_.info("All games executed. Writing output...");

    // Write output to file
    writeOutput(mapPath, algorithmSoPath1, algorithmSoPath2, gmFolder);
    logger_.info("Comparative simulation completed.");

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
    // Get absolute path and derive the .so name
    auto absPath = std::filesystem::absolute(path);
    string soName = absPath.stem().string();

    // Check if already loaded
    if (void* probe = dlopen(absPath.c_str(), RTLD_LAZY | RTLD_NOLOAD)) {
        dlclose(probe); 
        return true;
    }
    logger_.debug("Loading algorithm .so file: ", path);

    // Register the algorithm entry
    if (!algo_registrar) {
        logger_.reportError("Algorithm registrar is null");
        return false;
    }
    algo_registrar->createAlgorithmFactoryEntry(soName);
    logger_.debug("Created algorithm entry for: ", soName);
    
    // Attempt to load the .so file
    void* handle = dlopen(absPath.c_str(), RTLD_LAZY);
    if (!handle) {
        algo_registrar->removeLast(); //Rollback the last entry
        const char* error = dlerror();
        logger_.reportError("Failed loading .so file from path: ", path, "\n", (error ? error : "Unknown error"));
        return false;
    }

    // Validate the registration
    try {
        algo_registrar->validateLastRegistration(); 
    } catch (...) {
        dlclose(handle);
        algo_registrar->removeLast();
        logger_.reportError("Registration incomplete for ", soName);
        return false;
    }

    // Store the handle for later cleanup
    algoHandles_.push_back(handle);

    logger_.debug("Successfully loaded algorithm .so file: ", path);
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
    // Get absolute path and derive the .so name
    auto absPath = std::filesystem::absolute(path);
    string soName = absPath.stem().string();

    // Register the GameManager entry
    if (!game_manager_registrar) {
        logger_.reportError("Game manager registrar is null");
        return nullptr;
    }
    logger_.debug("Loading GameManager .so file: ", path);
    game_manager_registrar->createEntry(soName);
    logger_.debug("Created GameManager entry for: ", soName);

    // Attempt to load the .so file
    void* handle = dlopen(absPath.c_str(), RTLD_LAZY);
    if (!handle) {
        game_manager_registrar->removeLast(); // Rollback the last entry
        const char* error = dlerror();
        logger_.reportError("Failed loading .so file from path: ", path, "\n", (error ? error : "Unknown error"));
        return nullptr;
    }

    // Validate the registration
    try {
        game_manager_registrar->validateLast(); 
    } catch (const std::exception& e) {
        dlclose(handle);
        game_manager_registrar->removeLast();
        logger_.reportError("Registration incomplete for ", soName, "\n", e.what());
        return nullptr;
    }

    logger_.debug("Successfully loaded GameManager .so file: ", path);
    return handle;
}

/**
 * @brief Scans a folder for GameManager shared libraries and records their paths.
 *
 * Iterates over all directory entries under @p gameManagerFolder and collects the
 * absolute paths of files with the ".so" extension. Only regular files ending with
 * ".so" are considered. The discovered paths are appended to the internal list
 * @c gms_paths_ without clearing it first.
 *
 * @note The filesystem iteration order is implementation-defined; no sorting is applied.
 * @note This function does not attempt to dlopen or validate the binary beyond the extension.
 *
 * @param gameManagerFolder Path to the directory that is expected to contain GameManager .so files.
 */
void ComparativeSimulator::getGameManagers(const string& gameManagerFolder) {
    for (const auto& entry : directory_iterator(gameManagerFolder)) { // Check each entry in the directory
        if (entry.path().extension() == ".so") { // We care only about .so files
            gms_paths_.push_back(entry.path()); // Store the path of the GameManager .so file
            logger_.debug("Found GameManager .so file: ", entry.path().string());
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
        logger_.debug("Running all games sequentially on the main thread.");
        for (const auto& task : gms_paths_) {
            runSingleGame(task); // Run all games sequentially if only one thread
        }
        return;
    }

    logger_.debug("Running games using a thread pool with ", threadCount, " threads.");

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
    logger_.info("All games completed.");
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
    logger_.debug("Thread ", std::this_thread::get_id(), " running game with GameManager: ", gmPath.string());
    string gm_name = gmPath.stem().string();
    // load .so file for Game Manager
    void* gm_handle = loadGameManagerSO(gmPath);
    if (errorHandle(!gm_handle, "Failed to load GameManager .so file: ", gm_handle, gmPath.string())) { return; }
   
    {
        unique_ptr<AbstractGameManager> gameManager;
        bool createdGameManager = false;
        {
            // Get the GameManager factory and name from the registrar
            lock_guard<mutex> lock(gmRegistrarmutex_);

            auto gm = (game_manager_registrar->managerByName(gm_name)); // Get the last registered GameManager
            
            // Create the GameManager instance
            gameManager = gm.create(verbose_);
            createdGameManager = (gameManager != nullptr);
            logger_.debug("Thread ", std::this_thread::get_id(), " created GameManager instance for: ", gm_name);
        }
        
        if (errorHandle(!createdGameManager, "Failed to create GameManager instance for: ", gm_handle, gm_name)) return;

        // Create players using the algorithm factories
        unique_ptr<Player> player1 = algo1_->createPlayer(0, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
        unique_ptr<Player> player2 = algo2_->createPlayer(1, mapData_.cols, mapData_.rows, mapData_.maxSteps, mapData_.numShells);
        if (errorHandle(!player1 || !player2, "Failed to create one (or more) of the players", gm_handle)) return;

        // Get algorithm names and factories
        string name1 = algo1_->name();
        string name2 = algo2_->name();
        TankAlgorithmFactory tankAlgorithmFactory1 = algo1_->getTankAlgorithmFactory();
        TankAlgorithmFactory tankAlgorithmFactory2 = algo2_->getTankAlgorithmFactory();
        logger_.debug("Thread ", std::this_thread::get_id(), " created players: ", name1, " and ", name2, " for GameManager: ", gm_name);

        // Run game manager with players and factories
        logger_.info("Thread ", std::this_thread::get_id(), " starting game with GameManager: ", gm_name);
        GameResult result = gameManager->run(mapData_.cols, mapData_.rows, *mapData_.satelliteView, mapData_.name,
            mapData_.maxSteps, mapData_.numShells, *player1, name1, *player2, name2, tankAlgorithmFactory1, tankAlgorithmFactory2);

        // Store the result in allResults
        {
            lock_guard<mutex> lock(allResultsMutex_);
            SnapshotGameResult snap = makeSnapshot(result, mapData_.rows, mapData_.cols);
            if (errorHandle(snap.board.empty(), "Empty board in GameResult for GameManager: ", gm_handle, gm_name)) { return; }
            allResults.emplace_back(snap, gm_name);
        }
    
        // Remove the GameManager entry from the registrar
        
        lock_guard<mutex> lock(gmRegistrarmutex_);
        game_manager_registrar->eraseByName(gm_name); // Remove the GameManager entry by name
        

        logger_.info("Thread ", std::this_thread::get_id(), " finished game with GameManager: ", gm_name,
                    ". Winner: ", result.winner, ", Reason: ", result.reason, ", Rounds: ", result.rounds);
    }
    

    // Remove the GameManager entry from the registrar and close the handle
    dlclose(gm_handle); 
}

/**
 * @brief Handles errors related to GameManager operations.
 *
 * This function checks a condition and prints an error message if the condition is true.
 * It also closes the GameManager shared object handle and returns true to indicate an error.
 *
 * @param condition The condition to check.
 * @param msg The error message to print if the condition is true.
 * @param gm_handle The handle of the GameManager shared object.
 * @param name Optional name of the GameManager for more context in the error message.
 * @return true if an error occurred, false otherwise.
 */
bool ComparativeSimulator::errorHandle (bool condition ,const string& msg, void* gm_handle, const string& name) {
    if (condition) {
        logger_.reportWarn(msg, name);
        if (gm_handle) {
            dlclose(gm_handle); // Close the GameManager shared object handle
        }
        return true; // Indicate an error occurred
    }
    return false; // No error
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
    if (a.winner != b.winner || a.reason != b.reason || a.rounds != b.rounds)
        return false;

    // Fast path: exact structural/content equality
    if (a.board == b.board) return true;

    // Compare boards cell-by-cell, treating '$' as equivalent to '#'
    auto norm = [](char c) constexpr { return c == '$' ? '#' : c; };

    // Check dimensions, and then each cell
    if (a.board.size() != b.board.size()) return false;
    for (size_t y = 0; y < a.board.size(); ++y) {
        if (a.board[y].size() != b.board[y].size()) return false;
        for (size_t x = 0; x < a.board[y].size(); ++x) {
            if (norm(a.board[y][x]) != norm(b.board[y][x])) return false;
        }
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
        bool placed = false; // Flag to check if the result was placed in a group
        for (auto& group : groups) {
            if (sameResult(result.first, group.result)) { // Found a matching group
                group.gm_names.push_back(result.second);
                group.count += 1;
                placed = true;
                break;
            }
        }
        if (!placed) { // Create a new group if not placed
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
        logger_.reportError("Failed to open output file in folder: ", gmFolder);
        printf("%s\n", outputBuffer.c_str());
        return;
    }
    // Else, write the output buffer to the file
    outFile << outputBuffer;
    outFile.close();
}

/**
 * @brief Renders a snapshot of the final board to an output stream.
 *
 * Prints the 2D board stored in @p result.board to @p os, row by row, followed by '\n'.
 * The character '$' is an internal sentinel and is replaced with '#' before printing
 * to keep the output user-facing and consistent with the expected file format.
 *
 * @param os Output stream to write the board to (e.g., a file or std::cout).
 * @param result Snapshot of a single game's final state, including a @c board matrix.
 *
 * @pre @p result.board is a rectangular grid (all rows have identical length).
 * @post The stream @p os is advanced by the full board plus trailing newlines.
 */
void ComparativeSimulator::printSatellite(std::ostream& os,
                           const SnapshotGameResult& result) {
    // Print the board row by row
    for (size_t y = 0; y < result.board.size(); ++y) {
        for (size_t x = 0; x < result.board[y].size(); ++x) {
            char cell = result.board[y][x];
            if (cell == '$') cell = '#'; // $ is an internaly used char
            os << cell;
        }
        os << "\n";
    }
}

/**
 * @brief Builds the full comparative results buffer for all game-manager groups.
 *
 * Produces the text that should be written to the comparative results file, adhering to
 * the assignment’s output spec. The buffer starts with:
 *
 *   1) "game_map=<file_name>\n"
 *   2) "algorithm1=<file_name>\n"
 *   3) "algorithm2=<file_name>\n"
 *   4) blank line
 *
 * Then, for each remaining group in @c groups (processed in LIFO order), this function appends:
 *   - A comma-separated list of GameManager names that achieved the identical final result.
 *   - A human-readable result line (winner/tie and reason), derived from @c group.result.
 *   - The round number in which the game finished.
 *   - A full map of the final state via @c printSatellite(...).
 *   - A blank line between groups (but not after the last).
 *
 * @warning This function consumes @c groups by repeatedly calling @c pop_back(); after it returns,
 *          @c groups will be empty. If you need the groups later, make a copy before calling.
 *
 * @param mapPath Absolute or relative path to the game map; only the filename is printed.
 * @param algorithmSoPath1 Path to the first algorithm’s shared object; only the filename is printed.
 * @param algorithmSoPath2 Path to the second algorithm’s shared object; only the filename is printed.
 *
 * @return A single string containing the entire formatted output, ready to be written to file.
 *
 * @note File names are extracted using @c getFilename(...). The tie/win message formatting matches
 *       the assignment requirements (including the special-case replacement of '$' in the board).
 * @note The helper @c tanksOrZero(...) defensively handles missing indices in @c remaining_tanks.
 */
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

        // Print the GameManager names, comma-separated
        if (!group.gm_names.empty()) {
            for (size_t i = 0; i + 1 < group.gm_names.size(); ++i) {
                oss << group.gm_names[i] << ", ";
            }
            oss << group.gm_names.back() << "\n";
        } else {
            oss << "\n"; // defensive: keep shape even if no names
        }

        // Helper to safely get remaining tanks or return 0 if index is out of bounds
        auto tanksOrZero = [&](size_t idx) -> int {
            return idx < group.result.remaining_tanks.size() ? group.result.remaining_tanks[idx] : 0;
        };

        // Format the winner/tie message according to the spec
        oss << (
            group.result.winner == 0
            ? (group.result.reason == GameResult::ALL_TANKS_DEAD
                ? "Tie, both players have zero tanks"
                : group.result.reason == GameResult::MAX_STEPS
                    ? "Tie, reached max steps = " + std::to_string(group.result.rounds) +
                    ", player 1 has " + std::to_string(tanksOrZero(0)) +
                    " tanks, player 2 has " + std::to_string(tanksOrZero(1)) + " tanks"
                    : "Tie, both players have zero shells for 40 steps") // replace 40 with your constant if you have one
            : "Player " + std::to_string(group.result.winner) + " won with " +
            std::to_string(tanksOrZero(group.result.winner - 1)) + " tanks still alive"
        ) << "\n";

        // Print the number of rounds
        oss << group.result.rounds << "\n";

        // Print the final board state
        printSatellite(oss, group.result);

        // Print a blank line between groups, but not after the last
        if (!groups.empty()) {
            oss << "\n";
        }
    }

    return oss.str();
}
