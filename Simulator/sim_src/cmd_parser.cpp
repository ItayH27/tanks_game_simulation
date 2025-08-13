#include "../sim_include/cmd_parser.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
    using ParseResult = CmdParser::ParseResult;

    // ---- filesystem helpers ----
    inline bool isFileValid(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_regular_file(path, ec);
    }

    inline bool isFolderValid(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_directory(path, ec) && !fs::is_empty(path, ec);
    }

    // ---- token helpers ----
    // Parse "key=value" (no spaces). On error returns {"",""}.
    inline std::pair<std::string, std::string> parseKeyVal(const std::string& tok) {
        const std::size_t eq = tok.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 == tok.size()) return {"", ""};
        return {tok.substr(0, eq), tok.substr(eq + 1)};
    }

    // Parse num_threads if provided. Returns true on success and writes dst.
    inline bool parseNumThreads(const std::unordered_map<std::string, std::string>& args, std::optional<int>& dst) {
        auto it = args.find("num_threads");
        if (it == args.end()) return true; // not provided → OK
        // Accept only decimal positive integers (stoi throws on junk; we guard).
        const std::string& s = it->second;
        if (s.empty()) return false;
        int signFree = 0;
        for (char c : s) {
            if (c < '0' || c > '9') return false;
            signFree = 1;
        }
        if (!signFree) return false;
        try {
            int v = std::stoi(s);
            dst = v;
            return true;
        } catch (...) {
            return false;
        }
    }

    // ---- mode validators ----
    ParseResult validateComparative(const std::unordered_map<std::string, std::string>& args, ParseResult& out) {
        static const std::vector<std::string> required = {
            "game_map", "game_managers_folder", "algorithm1", "algorithm2"
        };
        for (const auto& k : required) {
            if (args.find(k) == args.end()) return ParseResult::fail("Missing required argument: " + k);
        }

        out.gameMapFile        = args.at("game_map");
        out.gameManagersFolder = args.at("game_managers_folder");
        out.algorithm1File     = args.at("algorithm1");
        out.algorithm2File     = args.at("algorithm2");

        if (!parseNumThreads(args, out.numThreads))
            return ParseResult::fail("Invalid value for num_threads (must be a positive integer).");

        if (!isFileValid(out.gameMapFile))
            return ParseResult::fail("Invalid or missing file: " + out.gameMapFile);
        if (!isFolderValid(out.gameManagersFolder))
            return ParseResult::fail("Invalid folder: " + out.gameManagersFolder);
        if (!isFileValid(out.algorithm1File))
            return ParseResult::fail("Invalid or missing file: " + out.algorithm1File);
        if (!isFileValid(out.algorithm2File))
            return ParseResult::fail("Invalid or missing file: " + out.algorithm2File);

        out.valid = true;
        return out;
    }

    ParseResult validateCompetition(const std::unordered_map<std::string, std::string>& args, ParseResult& out) {
        static const std::vector<std::string> required = {
            "game_maps_folder", "game_manager", "algorithms_folder"
        };
        for (const auto& k : required) {
            if (args.find(k) == args.end()) return ParseResult::fail("Missing required argument: " + k);
        }

        out.gameMapsFolder   = args.at("game_maps_folder");
        out.gameManagerFile  = args.at("game_manager");
        out.algorithmsFolder = args.at("algorithms_folder");

        if (!parseNumThreads(args, out.numThreads))
            return ParseResult::fail("Invalid value for num_threads (must be a positive integer).");

        if (!isFolderValid(out.gameMapsFolder))
            return ParseResult::fail("Invalid folder: " + out.gameMapsFolder);
        if (!isFileValid(out.gameManagerFile))
            return ParseResult::fail("Invalid file: " + out.gameManagerFile);
        if (!isFolderValid(out.algorithmsFolder))
            return ParseResult::fail("Invalid folder: " + out.algorithmsFolder);

        out.valid = true;
        return out;
    }
} // namespace

// ---- CmdParser implementation ----
CmdParser::ParseResult CmdParser::parse(int argc, char** argv) {
    ParseResult res;
    res.mode = Mode::None;

    bool wantComparative = false;
    bool wantCompetition = false;

    std::unordered_map<std::string, std::string> kv;

    for (int i = 1; i < argc; ++i) {
        const std::string token = argv[i];

        if (token == "-comparative") {
            wantComparative = true;
            continue;
        }
        if (token == "-competition") {
            wantCompetition = true;
            continue;
        }
        if (token == "-verbose") {
            res.verbose = true;
            continue;
        }

        auto [k, v] = parseKeyVal(token);
        if (k.empty()) {
            return ParseResult::fail("Unsupported argument format: " + token);
        }
        kv[std::move(k)] = std::move(v);
    }

    if (wantComparative == wantCompetition) {
        return ParseResult::fail("Exactly one of -comparative or -competition must be specified.");
    }

    res.mode = wantComparative ? Mode::Comparative : Mode::Competition;
    return (res.mode == Mode::Comparative)
        ? validateComparative(kv, res)
        : validateCompetition(kv, res);
}

void CmdParser::printUsage() {
    std::cout
        << "Usage:\n"
        << "  ./simulator_<ids> -comparative "
           "game_map=<file> game_managers_folder=<folder> "
           "algorithm1=<file> algorithm2=<file> "
           "[num_threads=<n>] [-verbose]\n\n"
        << "  ./simulator_<ids> -competition "
           "game_maps_folder=<folder> game_manager=<file> "
           "algorithms_folder=<folder> [num_threads=<n>] [-verbose]\n";
}
