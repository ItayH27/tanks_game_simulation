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

int CompetitiveSimulator::run(const std::string& mapsFolder,
                              const std::string& gameManagerSoPath,
                              const std::string& algorithmsFolder) {
    if (!loadGameManager(gameManagerSoPath)) {
        return 1;
    }

    if (!loadAlgorithms(algorithmsFolder)) {
        return 1;
    }
    if (algorithms_.size() < 2) {
        std::cerr << "Error: Need at least 2 algorithms for competition." << std::endl;
        return 1;
    }

    std::vector<std::filesystem::path> maps;
    if (!loadMaps(mapsFolder, maps)) {
        std::cerr << "Error: No maps found in folder: " << mapsFolder << std::endl;
        return 1;
    }

    scheduleGames(maps);
    runGames();
    writeOutput(algorithmsFolder, mapsFolder, gameManagerSoPath);

    return 0;
}

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

bool CompetitiveSimulator::loadAlgorithms(const std::string& folder) {
    auto& registrar = AlgorithmRegistrar::getAlgorithmRegistrar();

    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() == ".so") {
            const std::string soPath = entry.path().string();
            const std::string name = entry.path().stem().string();

            registrar.createAlgorithmFactoryEntry(name);

            void* handle = nullptr;
            {
                std::lock_guard<std::mutex> lock(handlesMutex_);
                auto& handleData = algoLibHandles_[soPath];
                if (!handleData.handle) {
                    handleData.handle = dlopen(soPath.c_str(), RTLD_NOW);
                    if (!handleData.handle) {
                        std::cerr << "Failed to load " << soPath << ": " << dlerror() << std::endl;
                        registrar.removeLast();
                        algoLibHandles_.erase(soPath);
                        continue;
                    }
                }
                ++handleData.refCount;
                handle = handleData.handle;
            }

            try {
                registrar.validateLastRegistration();
                algorithms_.push_back(std::make_shared<AlgorithmRegistrar::AlgorithmAndPlayerFactories>(registrar.end()[-1]));

                std::lock_guard<std::mutex> lock(handlesMutex_);
                algoNameToPath_[name] = soPath;
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

    return !algorithms_.empty();
}

bool CompetitiveSimulator::loadMaps(const std::string& folder, std::vector<std::filesystem::path>& outMaps) {
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.is_regular_file()) {
            outMaps.push_back(entry.path());
        }
    }
    return !outMaps.empty();
}

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

void CompetitiveSimulator::runSingleGame(const GameTask& task) {
    auto gm = createGameManager();

    // TODO: Add actual game logic using task.algo1 and task.algo2

    std::string name1 = task.algo1->name();
    std::string name2 = task.algo2->name();

    // Assume dummy result for now
    GameResult result{};
    result.winner = 0; // Tie by default

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

void CompetitiveSimulator::updateScore(const std::string& winner, const std::string& loser, bool tie) {
    std::lock_guard<std::mutex> lock(scoresMutex_);
    if (tie) {
        scores_[winner] += 1;
        scores_[loser] += 1;
    } else {
        scores_[winner] += 3;
    }
}

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

std::unique_ptr<AbstractGameManager> CompetitiveSimulator::createGameManager() {
    return gameManagerFactory_(verbose_);
}

std::string CompetitiveSimulator::timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    return ss.str();
}

void CompetitiveSimulator::releaseAlgorithmLib(const std::string& path) {
    std::lock_guard<std::mutex> lock(handlesMutex_);
    auto it = algoLibHandles_.find(path);
    if (it != algoLibHandles_.end()) {
        if (--(it->second.refCount) == 0 && it->second.handle) {
            dlclose(it->second.handle);
            algoLibHandles_.erase(it);
        }
    }
}

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
