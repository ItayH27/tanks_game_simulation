#include "../sim_include/cmd_parser.h"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <regex>

namespace fs = std::filesystem;

namespace {

    /**
     * @brief Checks whether the given path is a valid regular file.
     *
     * @param path The file path to check.
     * @return true if the path exists and is a regular file, false otherwise.
     */
    bool isFileValid(const std::string& path) {
        return fs::exists(path) && fs::is_regular_file(path);
    }

    /**
     * @brief Checks whether the given path is a non-empty directory.
     *
     * @param path The folder path to check.
     * @return true if the path exists, is a directory, and is not empty.
     */
    bool isFolderValid(const std::string& path) {
        return fs::exists(path) && fs::is_directory(path) && !fs::is_empty(path);
    }

    /**
     * @brief Trims leading and trailing spaces and tabs from a string.
     *
     * @param s The input string.
     * @return A new trimmed string.
     */
    std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t");
        auto end = s.find_last_not_of(" \t");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    }

    /**
     * @brief Parses a command-line argument of the form key=value.
     *
     * Assumes there are no spaces around '='. Returns an empty pair if format is invalid.
     *
     * @param arg The argument string to parse.
     * @return A pair of strings: {key, value}, or {"", ""} if invalid.
     */
    std::pair<std::string, std::string> parseArg(const std::string& arg) {
        size_t eqPos = arg.find('=');
        if (eqPos == std::string::npos || eqPos == 0 || eqPos == arg.length() - 1)
            return {"", ""}; // invalid format
        return { arg.substr(0, eqPos), arg.substr(eqPos + 1) };
    }

    /**
     * @brief Validates required arguments for comparative mode.
     *
     * @param args Parsed argument map.
     * @param result The output ParseResult to populate.
     * @return A populated ParseResult indicating success or failure.
     */
    CmdParser::ParseResult validateComparative(const std::unordered_map<std::string, std::string>& args, CmdParser::ParseResult& result) {
        const std::vector<std::string> required = {"game_map", "game_managers_folder", "algorithm1", "algorithm2"};
        for (const auto& key : required)
            if (!args.count(key)) return {false, "Missing required argument: " + key};

        result.gameMapFile = args.at("game_map");
        result.gameManagersFolder = args.at("game_managers_folder");
        result.algorithm1File = args.at("algorithm1");
        result.algorithm2File = args.at("algorithm2");
        if (args.count("num_threads")) result.numThreads = std::stoi(args.at("num_threads"));

        if (!isFileValid(result.gameMapFile)) return {false, "Invalid or missing file: " + result.gameMapFile};
        if (!isFolderValid(result.gameManagersFolder)) return {false, "Invalid folder: " + result.gameManagersFolder};
        if (!isFileValid(result.algorithm1File)) return {false, "Invalid or missing file: " + result.algorithm1File};
        if (!isFileValid(result.algorithm2File)) return {false, "Invalid or missing file: " + result.algorithm2File};

        result.valid = true;
        return result;
    }

    /**
     * @brief Validates required arguments for competition mode.
     *
     * @param args Parsed argument map.
     * @param result The output ParseResult to populate.
     * @return A populated ParseResult indicating success or failure.
     */
    CmdParser::ParseResult validateCompetition(const std::unordered_map<std::string, std::string>& args, CmdParser::ParseResult& result) {
        const std::vector<std::string> required = {"game_maps_folder", "game_manager", "algorithms_folder"};
        for (const auto& key : required)
            if (!args.count(key)) return {false, "Missing required argument: " + key};

        result.gameMapsFolder = args.at("game_maps_folder");
        result.gameManagerFile = args.at("game_manager");
        result.algorithmsFolder = args.at("algorithms_folder");
        if (args.count("num_threads")) result.numThreads = std::stoi(args.at("num_threads"));

        if (!isFolderValid(result.gameMapsFolder)) return {false, "Invalid folder: " + result.gameMapsFolder};
        if (!isFileValid(result.gameManagerFile)) return {false, "Invalid file: " + result.gameManagerFile};
        if (!isFolderValid(result.algorithmsFolder)) return {false, "Invalid folder: " + result.algorithmsFolder};

        result.valid = true;
        return result;
    }
}

/**
 * @brief Parses the command-line arguments into structured simulator parameters.
 *
 * Determines whether comparative or competition mode is used, validates inputs,
 * and returns a populated ParseResult.
 *
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return A ParseResult indicating success or failure and containing parsed values.
 */
CmdParser::ParseResult CmdParser::parse(int argc, char** argv) {
    ParseResult result;
    std::unordered_map<std::string, std::string> args;
    bool hasComparative = false, hasCompetition = false;

    auto processToken = [&](const std::string& token) {
        if (token == "-comparative") hasComparative = true;
        else if (token == "-competition") hasCompetition = true;
        else if (token == "-verbose") result.verbose = true;
        else {
            auto [key, value] = parseArg(token);
            if (!key.empty() && !value.empty()) args[key] = value;
            else { result.errorMessage = "Unsupported argument format: " + token; }
        }
    };

    for (int i = 1; i < argc && result.errorMessage.empty(); ++i)
        processToken(argv[i]);

    if (hasComparative == hasCompetition)
        return {false, "Exactly one of -comparative or -competition must be specified."};

    result.mode = hasComparative ? Mode::Comparative : Mode::Competition;
    return result.mode == Mode::Comparative
           ? validateComparative(args, result)
           : validateCompetition(args, result);
}

/**
 * @brief Prints usage instructions for the simulator.
 */
void CmdParser::printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  ./simulator_<ids> -comparative "
              << "game_map=<file> game_managers_folder=<folder> "
              << "algorithm1=<file> algorithm2=<file> "
              << "[num_threads=<n>] [-verbose]\n\n";
    std::cout << "  ./simulator_<ids> -competition "
              << "game_maps_folder=<folder> game_manager=<file> "
              << "algorithms_folder=<folder> [num_threads=<n>] [-verbose]\n";
}
