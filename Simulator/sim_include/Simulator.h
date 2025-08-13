#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <optional>
#include <mutex>
#include "AbstractGameManager.h"
#include "../UserCommon/UC_include/ExtSatelliteView.h"

namespace fs = std::filesystem;
using std::string, std::ofstream, std::ifstream, std::endl, std::cerr, std::pair, std::tuple, std::tie, std::unique_ptr;

class Simulator {
public:
    Simulator(bool verbose, size_t numThreads);
    virtual ~Simulator() = default;

protected:
    struct MapData {
        int numShells;
        int cols = 0;
        int rows = 0;
        std::string name;
        int maxSteps;
        bool failedInit;
        unique_ptr<ExtSatelliteView> satelliteView = nullptr;
        ofstream* inputErrors = nullptr;
    };

    bool verbose_;
    size_t numThreads_;
    void* gameManagerHandle_ = nullptr;
    GameManagerFactory gameManagerFactory_;
    std::mutex stderrMutex_;

    MapData readMap(const std::string& file_path);
    string Simulator::timestamp();

private:
    bool extractLineValue(const std::string& line, int& value, const std::string& key, const size_t line_number,
        Simulator::MapData &mapData, ofstream &inputErrors);
    bool Simulator::extractValues(Simulator::MapData &mapData, ifstream& inputFile, ofstream &inputErrors);
    tuple<bool, int, int> Simulator::fillGameBoard(vector<vector<char>> &gameBoard, ifstream &file, Simulator::MapData &mapData,
        ofstream &inputErrors);
    bool Simulator::checkForExtras(int extraRows, int extraCols, ofstream &inputErrors);

    std::optional<MapData> map_;
};

#endif // SIMULATOR_H
