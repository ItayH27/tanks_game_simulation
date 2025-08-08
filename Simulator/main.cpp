#include <iostream>
#include "cmd_parser.h"
#include "simulator.h"  // You need to implement this: contains runComparative() and runCompetition()

int main(int argc, char** argv) {
    CmdParser::ParseResult result = CmdParser::parse(argc, argv);

    if (!result.valid) {
        std::cerr << "Error: " << result.errorMessage << "\n\n";
        CmdParser::printUsage();
        return 1;
    }

    try {
        if (result.mode == CmdParser::Mode::Comparative) {
            runComparative(
                result.gameMapFile,
                result.gameManagersFolder,
                result.algorithm1File,
                result.algorithm2File,
                result.numThreads.value_or(1),
                result.verbose
            );
        } else if (result.mode == CmdParser::Mode::Competition) {
            runCompetition(
                result.gameMapsFolder,
                result.gameManagerFile,
                result.algorithmsFolder,
                result.numThreads.value_or(1),
                result.verbose
            );
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error during simulation: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
