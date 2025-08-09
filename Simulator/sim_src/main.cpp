#include <iostream>
#include "../sim_include/cmd_parser.h"
#include "../sim_include/competitive_simulator.h"

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
                result.algorithm2File
            );
        } else if (result.mode == CmdParser::Mode::Competition) {
            CompetitiveSimulator competitiveSimulator(result.verbose, (result.numThreads.value()));
            competitiveSimulator.run(
                result.gameMapsFolder,
                result.gameManagerFile,
                result.algorithmsFolder
            );
        }
    } catch (const std::exception& error) {
        std::cerr << "Fatal error during simulation: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
