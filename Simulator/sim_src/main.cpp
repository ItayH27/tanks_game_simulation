#include <iostream>
#include "../sim_include/cmd_parser.h"
#include "../sim_include/competitive_simulator.h"
#include "../sim_include/comparative_simulator.h"
#include "logger.h"

static void configureLogger(const CmdParser::ParseResult& r) {
    using utils::Logger;
    auto& L = Logger::get();

    if (!r.enableLogging) {
        L.setLevel(Logger::Level::Off);
        L.setAlsoConsole(false);
        (void)L.setOutputFile(""); // disable file
        return;
    }

    // File (optional)
    if (r.logFile && !r.logFile->empty()) {
        if (!L.setOutputFile(*r.logFile, /*append=*/true)) {
            std::cerr << "Warning: could not open log file '" << *r.logFile
                      << "'. Logging to console only.\n";
            (void)L.setOutputFile(""); // console-only
        }
    } else {
        (void)L.setOutputFile(""); // console-only
    }

    // Level & extras
    L.setLevel(r.debug ? Logger::Level::Debug : Logger::Level::Info);
    L.setAlsoConsole(true);
    L.setUseUTC(false);
}

int main(int argc, char** argv) {
    CmdParser::ParseResult result = CmdParser::parse(argc, argv);

    if (!result.valid) {
        std::cerr << "Error: " << result.errorMessage << "\n\n";
        CmdParser::printUsage();
        return 1;
    }

    configureLogger(result);

    try {
        if (result.mode == CmdParser::Mode::Comparative) {
            ComparativeSimulator comparativeSimulator(result.verbose, (result.numThreads.value()));
            comparativeSimulator.run(
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
