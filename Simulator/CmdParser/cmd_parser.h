#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

class CmdParser {
public:
    enum class Mode { Comparative, Competition };

    struct ParseResult {
        bool valid = false;
        std::string errorMessage;

        Mode mode;
        std::string gameMapFile;
        std::string gameMapsFolder;
        std::string gameManagersFolder;
        std::string gameManagerFile;
        std::string algorithm1File;
        std::string algorithm2File;
        std::string algorithmsFolder;
        std::optional<int> numThreads;
        bool verbose = false;
    };

    static ParseResult parse(int argc, char** argv);
    static void printUsage();
};
