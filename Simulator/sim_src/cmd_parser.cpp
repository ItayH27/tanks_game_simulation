#include "../sim_include/cmd_parser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
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
    inline std::string trim(std::string s) {
        auto issp = [](unsigned char c){ return std::isspace(c); };
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), issp));
        s.erase(std::find_if_not(s.rbegin(), s.rend(), issp).base(), s.end());
        return s;
    }

    inline bool isFileValid(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_regular_file(path, ec);
    }

    inline bool isFolderValid(const std::string& path) {
        std::error_code ec;
        return fs::exists(path, ec) && fs::is_directory(path, ec) && !fs::is_empty(path, ec);
    }

    // Parse num_threads strictly: digits only, integer >= 1. Default = 1 when absent.
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

    // -------- Normalization of argv into switches and k=v pairs --------
    struct NormalizedArgs {
        std::unordered_map<std::string, std::string> kv;
        std::vector<std::string> unsupported;
        std::vector<std::string> duplicates;
        bool wantComparative = false;
        bool wantCompetition = false;
        bool verbose = false;
    };

    static NormalizedArgs normalizeArgs(int argc, char** argv) {
        NormalizedArgs out;
        std::unordered_map<std::string,int> seen;

        std::string pendingKey;   // holds a key awaiting '=' and value
        bool seenEqForPending = false; // we saw an '=' after pendingKey

        auto noteKV = [&](std::string k, std::string v, const std::string& original) {
            k = trim(std::move(k));
            v = trim(std::move(v));
            if (k.empty() || v.empty()) {
                out.unsupported.push_back(original);
                return;
            }
            if (++seen[k] > 1) out.duplicates.push_back(k);
            out.kv[k] = v; // store last; duplicates are reported separately
        };

        for (int i = 1; i < argc; ++i) {
            std::string tok = argv[i];

            // switches
            if (tok == "-comparative") { out.wantComparative = true; continue; }
            if (tok == "-competition") { out.wantCompetition = true; continue; }
            if (tok == "-verbose")     { out.verbose        = true; continue; }

            if (tok == "=") {
                if (!pendingKey.empty() && !seenEqForPending) {
                    seenEqForPending = true; // expect value next
                } else {
                    out.unsupported.push_back(tok);
                }
                continue;
            }

            // token contains '='
            const auto pos = tok.find('=');
            if (pos != std::string::npos) {
                std::string left  = tok.substr(0, pos);
                std::string right = tok.substr(pos + 1);

                if (!pendingKey.empty() && !seenEqForPending) {
                    // Had a dangling bare key before; since this token is a k=v, consider that bare key unsupported
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
                    // "key=" -> expect value in the next token
                    pendingKey = trim(left);
                    seenEqForPending = true;
                    continue;
                }
                // "=value" (no key) -> unsupported
                out.unsupported.push_back(tok);
                continue;
            }

            // no '=' inside token
            if (!pendingKey.empty() && seenEqForPending) {
                // this token is the value for the pending key
                noteKV(pendingKey, tok, pendingKey + "=" + tok);
                pendingKey.clear();
                seenEqForPending = false;
                continue;
            }

            // Potential bare key followed by '=' token? Look ahead
            if (!pendingKey.empty() && !seenEqForPending) {
                // We already had a bare key with no '=', encountering another bare token -> previous was unsupported
                out.unsupported.push_back(pendingKey);
                pendingKey.clear();
            }

            // Start a potential key that might be followed by '='
            pendingKey = trim(tok);
            seenEqForPending = false;

            // If this is the last token or the next token isn't '=', this will be marked unsupported at the end.
            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (next != "=") {
                    // We'll decide on unsupported later; keep pendingKey to allow forms like "key=" on next token
                    continue;
                }
            }
        }

        // Clean up any dangling pendingKey
        if (!pendingKey.empty()) {
            if (seenEqForPending) {
                out.unsupported.push_back(pendingKey + "=");
            } else {
                out.unsupported.push_back(pendingKey);
            }
        }

        return out;
    }

    // Validate paths and populate result for comparative mode. num_threads has been validated/set earlier.
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

    // Validate paths and populate result for competition mode. num_threads has been validated/set earlier.
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
