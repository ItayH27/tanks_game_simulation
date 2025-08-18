#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

class CmdParser {
public:
    enum class Mode { None, Comparative, Competition };

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

        static ParseResult fail(std::string msg) {
            ParseResult r;
            r.valid = false;
            r.errorMessage = std::move(msg);
            return r;
        }
        int effectiveThreads() const {
            // Spec: if missing or 1 → single thread (main only):contentReference[oaicite:1]{index=1}.
            if (!numThreads || *numThreads <= 1) return 1;
            return *numThreads;
        }
    };


    ~CmdParser() = delete;
    CmdParser(const CmdParser&) = delete;
    CmdParser& operator=(const CmdParser&) = delete;
    CmdParser(CmdParser&&) = delete;
    CmdParser& operator=(CmdParser&&);
    
    static ParseResult parse(int argc, char** argv);
    static void printUsage();
};
