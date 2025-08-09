#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <optional>
#include "../UserCommon/UC_include/ExtSatelliteView.h"

namespace fs = std::filesystem;
using std::string, std::ofstream, std::ifstream, std::endl, std::cerr;

class Simulator {
public:
    virtual ~Simulator() = default;

protected:
    struct MapData {
        int numShells;
        int cols = 0;
        int rows = 0;
        std::string name;
        int maxSteps;
        bool failedInit;
        ExtSatelliteView* gameBoard = nullptr;
        ofstream* inputErrors = nullptr;
    };

    MapData readMap(const std::string& file_path);
    bool extractLineValue(const std::string& line, int& value, const std::string& key, const size_t line_number);
    bool Simulator::extractValues(Simulator::MapData &mapData, ifstream& inputFile);

    std::optional<MapData> map_;
};

#endif // SIMULATOR_H
