#include "../sim_include/cmd_parser.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
    using ParseResult = CmdParser::ParseResult;

    // Allowed argument keys for comparative and competition modes
    static const std::vector<std::string> validComparativeKeys = {
        "game_map", "game_managers_folder", "algorithm1", "algorithm2", "num_threads"
    };

    static const std::vector<std::string> validCompetitionKeys = {
        "game_maps_folder", "game_manager", "algorithms_folder", "num_threads"
    };

    /**
     * @brief Verifies that all provided argument keys are valid for the current mode.
     *
     * Iterates over the provided key-value arguments and ensures that each key
     * appears in the given list of valid keys. If any unrecognized key is found,
     * an error message is appended to the errors vector.
     *
     * @param args Map of parsed arguments (key=value pairs from the command line).
     * @param validKeys List of keys considered valid for the current run mode.
     * @param errors Vector to collect error messages describing invalid arguments.
     */
    void checkInvalidKeys(const std::unordered_map<std::string, std::string>& args, 
                        const std::vector<std::string>& validKeys,
                        std::vector<std::string>& errors) {
        for (const auto& [key, _] : args) {
            if (std::find(validKeys.begin(), validKeys.end(), key) == validKeys.end()) {
                errors.emplace_back("Invalid argument: " + key);
            }
        }
    }

    // ---- small utils ----

    /**
     * @brief Removes leading and trailing whitespace characters from a string.
     *
     * Uses std::isspace to trim both ends of the string in-place and returns
     * the cleaned version. Intended for sanitizing raw tokens before further parsing.
     *
     * @param s Input string to trim.
     * @return A string with whitespace removed from both beginning and end.
     */
    inline std::string trim(std::string s) {
        auto issp = [](unsigned char c){ return std::isspace(c); };
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), issp));
        s.erase(std::find_if_not(s.rbegin(), s.rend(), issp).base(), s.end());
        return s;
    }

    /**
     * @brief Checks whether the given path points to an existing regular file.
     *
     * Performs a safe check (ignores filesystem errors) to ensure that the path
     * exists and is classified as a file. This is used to validate command line
     * arguments that should refer to files such as maps or algorithms.
     *
     * @param path Path to check on the filesystem.
     * @return True if the path exists and is a regular file, false otherwise.
     */
    inline bool isFileValid(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_regular_file(path, ec);
    }

    /**
     * @brief Checks whether the given path points to a valid, non-empty directory.
     *
     * Performs a safe check (ignores filesystem errors) to ensure that the path
     * exists, is a directory, and is not empty. This is used to validate folders
     * containing managers, maps, or algorithms.
     *
     * @param path Path to check on the filesystem.
     * @return True if the path exists, is a directory, and has at least one entry.
     */
    inline bool isFolderValid(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_directory(path, ec) && !fs::is_empty(path, ec);
    }

    /**
     * @brief Parses and validates the "num_threads" argument strictly.
     *
     * Ensures that the argument value is a positive integer consisting of digits only.
     * Defaults to 1 if the argument is missing. If the provided value is invalid,
     * returns false without modifying the output parameter.
     *
     * @param kv Map of parsed key-value arguments.
     * @param out Reference to an integer that receives the parsed thread count.
     * @return True if parsing succeeds, false otherwise.
     */
    static bool parseNumThreadsStrict(const std::unordered_map<std::string,std::string>& kv, int& out) {
        auto it = kv.find("num_threads");
        if (it == kv.end()) { out = 1; return true; }
        const std::string& s = it->second;
        if (s.empty() || !std::all_of(s.begin(), s.end(), [](unsigned char c){ return std::isdigit(c); })) return false;
        try {
            long v = std::stol(s);
            if (v < 1) return false; // reject 0 and negatives
            out = static_cast<int>(v);
            return true;
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid number format: " << e.what() << std::endl;
            return false;
        } catch (const std::out_of_range& e) {
            std::cerr << "Number out of range: " << e.what() << std::endl;
            return false;
        }
    }

    // ==== small utils (add next to your existing helpers) ====
    inline std::string absoluteForMsg(const std::string& p) {
        std::error_code ec;
        auto abs = fs::absolute(p, ec);
        return ec ? p : abs.string();
    }

    // For files: exists + is_regular_file + readable (open)
    inline bool isReadableFile(const std::string& path) {
        std::error_code ec;
        if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) return false;
        std::ifstream f(path, std::ios::in | std::ios::binary);
        return static_cast<bool>(f);
    }

    // For folders: exists + is_directory + traversable + non-empty
    inline bool isTraversableNonEmptyDir(const std::string& path) {
        std::error_code ec;
        if (!fs::exists(path, ec) || !fs::is_directory(path, ec)) return false;
        // try to iterate at least one entry to prove traversal works
        fs::directory_iterator it(path, ec);
        if (ec) return false;                // cannot traverse
        return it != fs::directory_iterator{};
    }

    // -------- Normalization of argv into switches and k=v pairs --------
    struct NormalizedArgs {
        std::unordered_map<std::string, std::string> kv;
        std::vector<std::string> unsupported;
        std::vector<std::string> duplicates;
        bool wantComparative = false;
        bool wantCompetition = false;
        bool verbose = false;

        // Logger
        bool enableLogger = false;
        bool debug = false;
        std::optional<std::string> logFile;
    };

    /**
     * @brief Normalizes raw command-line arguments into structured form.
     *
     * Handles tokens with different formats: "key=value", "key =", "key = value",
     * as well as mode switches like "-comparative", "-competition", and "-verbose".
     * Collects unsupported or malformed arguments, detects duplicates, and outputs
     * a normalized representation ready for validation.
     *
     * @param argc Number of command-line arguments.
     * @param argv Array of argument strings.
     * @return A NormalizedArgs struct containing parsed arguments, switches, and errors.
     */
    static NormalizedArgs normalizeArgs(int argc, char** argv) {
        NormalizedArgs out;
        std::unordered_map<std::string,int> seen;

        std::string pendingKey;
        bool seenEqForPending = false;

        auto noteKV = [&](std::string k, std::string v, const std::string& original) {
            k = trim(std::move(k));
            v = trim(std::move(v));
            if (k.empty() || v.empty()) { out.unsupported.push_back(original); return; }
            if (++seen[k] > 1) out.duplicates.push_back(k);
            out.kv[k] = v;
        };

        for (int i = 1; i < argc; ++i) {
            std::string tok = argv[i];

            // existing switches
            if (tok == "-comparative") { out.wantComparative = true; continue; }
            if (tok == "-competition") { out.wantCompetition = true; continue; }
            if (tok == "-verbose")     { out.verbose        = true; continue; }
            if (tok == "-debug") { out.debug = true; continue; }
            if (tok == "-logger") {
                out.enableLogger = true;

                // Optional support for "-logger = path" (space-separated)
                if (i + 2 < argc && std::string(argv[i + 1]) == "=") {
                    out.logFile = trim(std::string(argv[i + 2]));
                    i += 2;
                }
                continue;
            }
            if (tok.rfind("-logger=", 0) == 0) {
                out.enableLogger = true;
                out.logFile = trim(tok.substr(std::string("-logger=").size()));
                continue;
            }

            if (tok == "=") {
                if (!pendingKey.empty() && !seenEqForPending) {
                    seenEqForPending = true;
                } else {
                    out.unsupported.push_back(tok);
                }
                continue;
            }

            const auto pos = tok.find('=');
            if (pos != std::string::npos) {
                std::string left  = tok.substr(0, pos);
                std::string right = tok.substr(pos + 1);

                if (!pendingKey.empty() && !seenEqForPending) {
                    out.unsupported.push_back(pendingKey);
                    pendingKey.clear();
                }

                if (!left.empty() && !right.empty()) {
                    noteKV(left, right, tok);
                    pendingKey.clear();
                    seenEqForPending = false;
                    continue;
                }
                if (!left.empty() && right.empty()) {
                    pendingKey = trim(left);
                    seenEqForPending = true;
                    continue;
                }
                out.unsupported.push_back(tok);
                continue;
            }

            if (!pendingKey.empty() && seenEqForPending) {
                noteKV(pendingKey, tok, pendingKey + "=" + tok);
                pendingKey.clear();
                seenEqForPending = false;
                continue;
            }

            if (!pendingKey.empty() && !seenEqForPending) {
                out.unsupported.push_back(pendingKey);
                pendingKey.clear();
            }

            pendingKey = trim(tok);
            seenEqForPending = false;

            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (next != "=") {
                    continue;
                }
            }
        }

        if (!pendingKey.empty()) {
            if (seenEqForPending) out.unsupported.push_back(pendingKey + "=");
            else                  out.unsupported.push_back(pendingKey);
        }

        return out;
    }

    /**
     * @brief Validates and extracts arguments required for comparative mode.
     *
     * Checks that all required arguments ("game_map", "game_managers_folder",
     * "algorithm1", "algorithm2") are provided and point to valid files/folders.
     * On success, populates the ParseResult with validated values.
     *
     * @param args Map of parsed key-value arguments.
     * @param out ParseResult object to populate with extracted values.
     * @return A ParseResult object, marked valid if all checks pass, invalid otherwise.
     */
    ParseResult validateComparative(const std::unordered_map<std::string, std::string>& args,
                                    ParseResult& out) {
        static const std::vector<std::string> required = {
            "game_map", "game_managers_folder", "algorithm1", "algorithm2"
        };
        for (const auto& k : required) {
            if (!args.count(k)) return ParseResult::fail("Missing required argument: " + k);
        }

        out.gameMapFile        = args.at("game_map");
        out.gameManagersFolder = args.at("game_managers_folder");
        out.algorithm1File     = args.at("algorithm1");
        out.algorithm2File     = args.at("algorithm2");

        if (!isReadableFile(out.gameMapFile))
            return ParseResult::fail("Invalid or unreadable file: " + absoluteForMsg(out.gameMapFile));
        if (!isTraversableNonEmptyDir(out.gameManagersFolder))
            return ParseResult::fail("Invalid or non-traversable folder (or empty): " +
                                    absoluteForMsg(out.gameManagersFolder));
        if (!isReadableFile(out.algorithm1File))
            return ParseResult::fail("Invalid or unreadable file: " + absoluteForMsg(out.algorithm1File));
        if (!isReadableFile(out.algorithm2File))
            return ParseResult::fail("Invalid or unreadable file: " + absoluteForMsg(out.algorithm2File));

        out.valid = true;
        return out;
    }

    /**
     * @brief Validates and extracts arguments required for competition mode.
     *
     * Checks that all required arguments ("game_maps_folder", "game_manager",
     * "algorithms_folder") are provided and point to valid files/folders.
     * On success, populates the ParseResult with validated values.
     *
     * @param args Map of parsed key-value arguments.
     * @param out ParseResult object to populate with extracted values.
     * @return A ParseResult object, marked valid if all checks pass, invalid otherwise.
     */
    ParseResult validateCompetition(const std::unordered_map<std::string, std::string>& args,
                                    ParseResult& out) {
        static const std::vector<std::string> required = {
            "game_maps_folder", "game_manager", "algorithms_folder"
        };
        for (const auto& k : required) {
            if (!args.count(k)) return ParseResult::fail("Missing required argument: " + k);
        }

        out.gameMapsFolder   = args.at("game_maps_folder");
        out.gameManagerFile  = args.at("game_manager");
        out.algorithmsFolder = args.at("algorithms_folder");

        if (!isTraversableNonEmptyDir(out.gameMapsFolder))
            return ParseResult::fail("Invalid or non-traversable folder (or empty): " +
                                    absoluteForMsg(out.gameMapsFolder));
        if (!isReadableFile(out.gameManagerFile))
            return ParseResult::fail("Invalid or unreadable file: " + absoluteForMsg(out.gameManagerFile));
        if (!isTraversableNonEmptyDir(out.algorithmsFolder))
            return ParseResult::fail("Invalid or non-traversable folder (or empty): " +
                                    absoluteForMsg(out.algorithmsFolder));

        out.valid = true;
        return out;
    }
    } // namespace

/**
 * @brief Parses command-line arguments and validates them according to mode.
 *
 * Supports two modes of operation:
 *   - Comparative mode (`-comparative`): requires arguments
 *     game_map, game_managers_folder, algorithm1, and algorithm2.
 *   - Competition mode (`-competition`): requires arguments
 *     game_maps_folder, game_manager, and algorithms_folder.
 *
 * Also handles optional arguments:
 *   - num_threads (must be a positive integer, default = 1)
 *   - -verbose flag for verbose output
 *
 * The parser reports and fails on:
 *   - Missing required arguments
 *   - Unsupported or malformed arguments
 *   - Invalid file/folder paths
 *   - Duplicate keys
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of argument strings.
 * @return A ParseResult object populated with mode, arguments, and validation state.
 *         If parsing fails, the object is marked invalid with an error message.
 */
CmdParser::ParseResult CmdParser::parse(int argc, char** argv) {
    ParseResult res;
    res.mode = Mode::None;

    // Normalize tokens first (handles multi-token k = v forms, etc.)
    NormalizedArgs nz = normalizeArgs(argc, argv);

    // Mode selection
    if (nz.wantComparative == nz.wantCompetition) {
        std::string msg = "Exactly one of -comparative or -competition must be specified.";
        // Also append any unsupported tokens we noticed while scanning
        for (const auto& u : nz.unsupported) msg += "\nUnsupported argument: " + u;
        return ParseResult::fail(msg);
    }

    res.verbose = nz.verbose;
    res.mode = nz.wantComparative ? Mode::Comparative : Mode::Competition;
    res.enableLogging = nz.enableLogger;
    res.debug = nz.debug;
    if (nz.logFile && !nz.logFile->empty()) res.logFile = *nz.logFile;

    // Collect all parse-time errors before path validation
    std::vector<std::string> errors;

    // Duplicates
    for (const auto& k : nz.duplicates) errors.emplace_back("Duplicate argument: " + k);

    // Required keys by mode (collect all missing)
    auto need = [&](std::initializer_list<const char*> keys){
        for (auto k: keys) if (!nz.kv.count(k)) errors.emplace_back(std::string("Missing required argument: ") + k);
    };
    if (nz.wantComparative) {
        need({"game_map","game_managers_folder","algorithm1","algorithm2"});
        checkInvalidKeys(nz.kv, validComparativeKeys, errors);
    } else {
        need({"game_maps_folder","game_manager","algorithms_folder"});
        checkInvalidKeys(nz.kv, validCompetitionKeys, errors);
    }

    // Unsupported tokens (list all)
    for (const auto& t : nz.unsupported) errors.emplace_back("Unsupported argument: " + t);

    // num_threads validation (default to 1 when absent)
    int threads = 1;
    if (!parseNumThreadsStrict(nz.kv, threads)) errors.emplace_back("Invalid value for num_threads (must be a positive integer).");
    res.numThreads = threads;

    if (!errors.empty()) {
        std::string msg;
        for (auto& e : errors) msg += e + '\n';
        return ParseResult::fail(msg);
    }

    // Populate path fields and perform filesystem checks
    return (res.mode == Mode::Comparative)
        ? validateComparative(nz.kv, res)
        : validateCompetition(nz.kv, res);
}


/**
 * @brief Prints usage instructions for the simulator program.
 *
 * Displays the expected command-line syntax for both comparative
 * and competition modes, including required and optional arguments.
 * Intended for use when arguments are missing, invalid, or unsupported.
 */
void CmdParser::printUsage() {
    std::cout
        << "Usage:\n"
        << "  ./simulator_<ids> -comparative "
           "game_map=<file> game_managers_folder=<folder> "
           "algorithm1=<file> algorithm2=<file> "
           "[num_threads=<n>] [-verbose] [-logger[=<path>]] [-debug]\n\n"
        << "  ./simulator_<ids> -competition "
           "game_maps_folder=<folder> game_manager=<file> "
           "algorithms_folder=<folder> "
           "[num_threads=<n>] [-verbose] [-logger[=<path>]] [-debug]\n";
}
