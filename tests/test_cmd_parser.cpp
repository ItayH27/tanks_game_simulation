#include "cmd_parser.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace {

char* str(const std::string& s) {
    return const_cast<char*>(s.c_str());
}

std::vector<char*> buildArgv(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.push_back(str("./simulator"));
    for (const auto& arg : args) argv.push_back(str(arg));
    return argv;
}

} // namespace

TEST(CmdParserTest, ValidComparativeBasic) {
    auto argv = buildArgv({
        "-comparative",
        "game_map=map.txt",
        "game_managers_folder=gm_folder",
        "algorithm1=algo1.so",
        "algorithm2=algo2.so"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.mode, CmdParser::Mode::Comparative);
    EXPECT_EQ(result.gameMapFile, "map.txt");
    EXPECT_EQ(result.algorithm1File, "algo1.so");
}

TEST(CmdParserTest, ValidCompetitionBasic) {
    auto argv = buildArgv({
        "-competition",
        "game_maps_folder=maps",
        "game_manager=gm.so",
        "algorithms_folder=algos"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.mode, CmdParser::Mode::Competition);
    EXPECT_EQ(result.algorithmsFolder, "algos");
}

TEST(CmdParserTest, MissingComparativeArgument) {
    auto argv = buildArgv({
        "-comparative",
        "game_map=map.txt",
        "algorithm1=algo1.so",
        "algorithm2=algo2.so"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.errorMessage.find("game_managers_folder"), std::string::npos);
}

TEST(CmdParserTest, AmbiguousMode) {
    auto argv = buildArgv({
        "-comparative",
        "-competition",
        "game_map=map.txt",
        "game_managers_folder=gm",
        "algorithm1=algo1.so",
        "algorithm2=algo2.so"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_FALSE(result.valid);
    EXPECT_NE(result.errorMessage.find("Exactly one of"), std::string::npos);
}

TEST(CmdParserTest, InvalidFormat) {
    auto argv = buildArgv({
        "-comparative",
        "game_map = map.txt", // space around '='
        "game_managers_folder=gm",
        "algorithm1=algo1.so",
        "algorithm2=algo2.so"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_FALSE(result.valid);
}

TEST(CmdParserTest, OptionalNumThreadsParsed) {
    auto argv = buildArgv({
        "-competition",
        "game_maps_folder=maps",
        "game_manager=gm.so",
        "algorithms_folder=algos",
        "num_threads=8"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_TRUE(result.valid);
    ASSERT_TRUE(result.numThreads.has_value());
    EXPECT_EQ(result.numThreads.value(), 8);
}

TEST(CmdParserTest, ParsesVerboseFlag) {
    auto argv = buildArgv({
        "-competition",
        "game_maps_folder=maps",
        "game_manager=gm.so",
        "algorithms_folder=algos",
        "-verbose"
    });
    auto result = CmdParser::parse(argv.size(), argv.data());
    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.verbose);
}
