#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <optional>

class Simulator {
public:
    virtual ~Simulator() = default;

protected:
    void readMap(const std::string& file_path);
    bool extractLineValue(const std::string& line, int& value, const std::string& key, const size_t line_number);

    struct MapData {
        size_t numShells;
        size_t cols = 0;
        size_t rows = 0;
        std::string name;
        size_t maxSteps;
    };

    std::optional<MapData> map_;
};

#endif // SIMULATOR_H
